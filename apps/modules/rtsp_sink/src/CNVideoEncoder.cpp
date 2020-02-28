#include "CNVideoEncoder.h"

#include <cstring>
#include <functional>
#include <sstream>
#include <iostream>
#include <glog/logging.h>
#include "easyinfer/mlu_context.h"

#define INPUT_QUEUE_SIZE 0
#define OUTPUT_BUFFER_SIZE 0x200000

CNVideoEncoder::CNVideoFrame::CNVideoFrame(CNVideoEncoder *encoder) : encoder_(encoder) {
  frame_ = new edk::CnFrame;
  memset(frame_, 0, sizeof(edk::CnFrame));

  frame_->width = encoder_->picture_width_;
  frame_->height = encoder_->picture_height_;

  frame_->pformat = edk::PixelFmt::NV21;
  
  if (frame_->pformat == edk::PixelFmt::NV21 || frame_->pformat == edk::PixelFmt::NV12) {
    frame_->frame_size = frame_->width * frame_->height * 3 / 2;
    frame_->n_planes = 2;
    frame_->strides[0] = frame_->width;
    frame_->strides[1] = frame_->width;  // ?    
    uint8_t *ptr = new uint8_t[frame_->frame_size];
    frame_->ptrs[0] = reinterpret_cast<void *>(ptr);
    frame_->ptrs[1] = reinterpret_cast<void *>(ptr + frame_->width * frame_->height);
    frame_->n_planes = 2;
  } else {
    frame_->frame_size = frame_->width * frame_->height * 3;
    frame_->n_planes = 1;
    frame_->strides[0] = frame_->width;   
    frame_->ptrs[0] = reinterpret_cast<void *>(new uint8_t[frame_->frame_size]); 
    frame_->n_planes = 1;
  }
}

CNVideoEncoder::CNVideoFrame::~CNVideoFrame() {
  if (frame_) {    
    uint8_t *ptr = reinterpret_cast<uint8_t *>(frame_->ptrs[0]);
    delete[] ptr;    
    delete frame_;
    frame_ = nullptr;
  }
}

void CNVideoEncoder::CNVideoFrame::Fill(uint8_t *data, int64_t timestamp) {
  if (frame_ == nullptr) return;
  frame_->pts = timestamp;
  if (frame_->pformat == edk::PixelFmt::NV21 || frame_->pformat == edk::PixelFmt::NV12) {
    memcpy(frame_->ptrs[0], data, frame_->frame_size);
  } else {
    std::cout << "unsupport pixel format: " << static_cast<int>(frame_->pformat) << std::endl;
  }
}

CNVideoEncoder::CNVideoEncoder(uint32_t width, uint32_t height, PictureFormat format, CodecType type, float frame_rate,
                               uint32_t gop_size, uint32_t bit_rate, uint32_t device_id)
    : VideoEncoder(frame_rate > 0 ? INPUT_QUEUE_SIZE : 0, OUTPUT_BUFFER_SIZE) {
  device_id_ = device_id;
  picture_width_ = width;
  picture_height_ = height;
  switch (format) {
    case NV21:
      picture_format_ = edk::PixelFmt::NV21;
      break;
    case NV12:
      picture_format_ = edk::PixelFmt::NV12;
      break;
    default:
      picture_format_ = edk::PixelFmt::NV21;
      break;
  }
  switch (type) {
    case H264:
      codec_type_ = edk::CodecType::H264;
      break;
    case HEVC:
      codec_type_ = edk::CodecType::H265;
      break;
    case MPEG4:
      codec_type_ = edk::CodecType::MPEG4;
      break;
    default:
      codec_type_ = edk::CodecType::H264;
      break;
  }

  frame_rate_num_ = (uint32_t)frame_rate;
  frame_rate_den_ = 1;

  gop_size_ = gop_size;
  bit_rate_ = bit_rate;

  edk::MluContext context;
  try {
    context.SetDeviceId(device_id_);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError &err) {
    std::cout << "CNVideoEncoder: set mlu env failed" << std::endl;
    return;
  }

  edk::EasyEncode::Attr attr;
  attr.dev_id = device_id_;
  attr.frame_geometry.w = picture_width_;
  attr.frame_geometry.h = picture_height_;
  attr.pixel_format = picture_format_; 
  attr.codec_type = codec_type_;

  attr.b_frame_num = 0;
  attr.input_buffer_num = 2;
  attr.output_buffer_num = 3;
  attr.max_mb_per_slice = 0;
  memset(&attr.rate_control, 0, sizeof(edk::RateControl));
  attr.rate_control.vbr = false;
  attr.rate_control.gop = gop_size_;
  attr.rate_control.src_frame_rate_num = frame_rate_num_;
  attr.rate_control.src_frame_rate_den = frame_rate_den_;
  attr.rate_control.bit_rate = bit_rate_ ;
  attr.rate_control.max_bit_rate = bit_rate_;
  memset(&attr.crop_config, 0, sizeof(edk::CropConfig));
  attr.crop_config.enable = false;
  attr.silent = false;
  attr.jpeg_qfactor = 50;
  attr.packet_callback = std::bind(&CNVideoEncoder::PacketCallback, this, std::placeholders::_1);
  attr.eos_callback = std::bind(&CNVideoEncoder::EosCallback, this);

  try {
    encoder_ = edk::EasyEncode::Create(attr);
  } catch (edk::EasyEncodeError &err) {
    std::cout << "CnEncodeError: " << err.what() << std::endl;
    Destroy();
    return;
  }
}

CNVideoEncoder::~CNVideoEncoder() {
  Stop();
  Destroy();
}

void CNVideoEncoder::Destroy() {
  edk::MluContext context;
  context.SetDeviceId(device_id_);
  context.ConfigureForThisThread();
  if (encoder_) {
    delete encoder_;
    encoder_ = nullptr;
  }
}

VideoEncoder::VideoFrame *CNVideoEncoder::NewFrame() { return new CNVideoFrame(this); }

void CNVideoEncoder::EncodeFrame(VideoFrame *frame) {
  CNVideoFrame *cnpic = dynamic_cast<CNVideoFrame *>(frame);
  edk::CnFrame *cnframe = cnpic->Get();
  edk::MluContext context;
  try {
    context.SetDeviceId(device_id_);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError &err) {
    std::cout << "CNVideoEncoder: set mlu env failed" << std::endl;
    return;
  }
  try {
    encoder_->SendDataCPU(*cnframe, false);
  } catch (edk::EasyEncodeError &err) {
    std::cout << "CnEncodeError: " << err.what() << std::endl;
    return;
  }
}

uint32_t CNVideoEncoder::GetOffSet(const uint8_t* data) {
  uint32_t offset = 0;
  const uint8_t *p = data;
  if (p[0] == 0x00 && p[1] == 0x00) {
    if (p[2] == 0x01) {
      offset = 3;
    } else if ((p[2] == 0x00) && (p[3] == 0x01)) {
      offset = 4;
    }
  }
  return offset;
}

void CNVideoEncoder::PacketCallback(const edk::CnPacket &packet) {
  if (packet.length == 0 || packet.data == 0) return;
  edk::MluContext context;
  context.SetDeviceId(device_id_);
  context.ConfigureForThisThread();
  // std::cout << "===got packet: size=" << packet.length << ", pts=" << packet.pts << std::endl;
  if (0 == (uint32_t)packet.slice_type) return; // drop first sps/pps packet

  uint32_t length = packet.length;
  uint8_t *packet_data = reinterpret_cast<uint8_t *>(packet.data);
  uint32_t offset = GetOffSet(packet_data); // process start code
  PushOutputBuffer(packet_data + offset, length - offset, frame_count_, packet.pts);

  frame_count_++;
}

void CNVideoEncoder::EosCallback() {
  edk::MluContext context;
  context.SetDeviceId(device_id_);
  context.ConfigureForThisThread();
  std::cout << "CNVideoEncoder got EOS" << std::endl;
}
