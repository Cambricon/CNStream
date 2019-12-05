#include "CNVideoEncoder.h"

#include <cstring>
#include <functional>
#include <iostream>

#include "easyinfer/mlu_context.h"

#define INPUT_QUEUE_SIZE 10
#define OUTPUT_BUFFER_SIZE 0x100000

CNVideoEncoder::CNVideoFrame::CNVideoFrame(CNVideoEncoder *encoder) : encoder_(encoder) {
  frame_ = new edk::CnFrame;
  memset(frame_, 0, sizeof(edk::CnFrame));

  frame_->width = encoder_->picture_width_;
  frame_->height = encoder_->picture_height_;
  frame_->pformat = encoder_->picture_format_;

  if (frame_->pformat == edk::PixelFmt::RGB24 || frame_->pformat == edk::PixelFmt::BGR24) {
    frame_->frame_size = frame_->width * frame_->height * 3;
    frame_->n_planes = 1;
    frame_->strides[0] = frame_->width;
    if (encoder_->copy_frame_buffer_) {
      frame_->ptrs[0] = reinterpret_cast<void *>(new uint8_t[frame_->frame_size]);
    } else {
      frame_->ptrs[0] = 0;
    }
    frame_->n_planes = 1;
  } else {
    frame_->frame_size = frame_->width * frame_->height * 3 / 2;
    frame_->n_planes = 2;
    frame_->strides[0] = frame_->width;
    frame_->strides[1] = frame_->width;  // ?
    if (encoder_->copy_frame_buffer_) {
      uint8_t *ptr = new uint8_t[frame_->frame_size];
      frame_->ptrs[0] = reinterpret_cast<void *>(ptr);
      frame_->ptrs[1] = reinterpret_cast<void *>(ptr + frame_->width * frame_->height);
    } else {
      frame_->ptrs[0] = nullptr;
      frame_->ptrs[1] = nullptr;
    }
    frame_->n_planes = 2;
  }
}

CNVideoEncoder::CNVideoFrame::~CNVideoFrame() {
  if (frame_) {
    if (frame_->ptrs[0] != nullptr && encoder_->copy_frame_buffer_) {
      uint8_t *ptr = reinterpret_cast<uint8_t *>(frame_->ptrs[0]);
      delete[] ptr;
    }
    delete frame_;
    frame_ = nullptr;
  }
}

void CNVideoEncoder::CNVideoFrame::Fill(uint8_t *data, int64_t timestamp) {
  if (frame_ == nullptr) return;

  frame_->pts = timestamp;

  if (frame_->pformat == edk::PixelFmt::RGB24 || frame_->pformat == edk::PixelFmt::BGR24) {
    if (encoder_->copy_frame_buffer_) {
      memcpy(frame_->ptrs[0], data, frame_->frame_size);
    } else {
      frame_->ptrs[0] = reinterpret_cast<void *>(data);
    }
  } else if (frame_->pformat == edk::PixelFmt::YUV420SP_NV21 || frame_->pformat == edk::PixelFmt::YUV420SP_NV21) {
    if (encoder_->copy_frame_buffer_) {
      memcpy(frame_->ptrs[0], data, frame_->frame_size);
    } else {
      frame_->ptrs[0] = reinterpret_cast<void *>(data);
      frame_->ptrs[1] = reinterpret_cast<void *>(data + frame_->width * frame_->height);
    }
  } else {
    std::cout << "unsupport pixel format: " << static_cast<int>(frame_->pformat) << std::endl;
  }
}

CNVideoEncoder::CNVideoEncoder(uint32_t width, uint32_t height, PictureFormat format, CodecType type, float frame_rate,
                               uint32_t gop_size, uint32_t bit_rate)
    : VideoEncoder(frame_rate > 0 ? INPUT_QUEUE_SIZE : 0, OUTPUT_BUFFER_SIZE) {
  picture_width_ = width;
  picture_height_ = height;
  switch (format) {
    case YUV420P:
      std::cout << "CNEncoder does not support YUV420P";
      return;
    case RGB24:
      picture_format_ = edk::PixelFmt::RGB24;
      break;
    case BGR24:
      picture_format_ = edk::PixelFmt::BGR24;
      break;
    case NV21:
      picture_format_ = edk::PixelFmt::YUV420SP_NV21;
      break;
    case NV12:
      picture_format_ = edk::PixelFmt::YUV420SP_NV12;
      break;
    default:
      picture_format_ = edk::PixelFmt::BGR24;
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
  copy_frame_buffer_ = (frame_rate > 0);
  if (0 /* frame_rate_ > 0 */) {
    // frame_rate_ = av_d2q(frame_rate, 60000);
  } else {
    frame_rate_num_ = 30;
    frame_rate_den_ = 1;
  }
  gop_size_ = gop_size;
  bit_rate_ = bit_rate;

  edk::MluContext context;
  try {
    context.SetDeviceId(0);
    context.SetChannelId(0);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError &err) {
    std::cout << "CNVideoEncoder: set mlu env failed" << std::endl;
    return;
  }

  edk::EasyEncode::Attr attr;
  attr.maximum_geometry.w = attr.output_geometry.w = picture_width_;
  attr.maximum_geometry.h = attr.output_geometry.h = picture_height_;
  attr.pixel_format = picture_format_;
  attr.codec_type = codec_type_;
  memset(&attr.rate_control, 0, sizeof(edk::RateControl));
  attr.rate_control.vbr = false;
  attr.rate_control.gop = gop_size_;
  attr.rate_control.stat_time = 1;
  attr.rate_control.src_frame_rate_num = frame_rate_num_;
  attr.rate_control.src_frame_rate_den = frame_rate_den_;
  attr.rate_control.dst_frame_rate_num = frame_rate_num_;
  attr.rate_control.dst_frame_rate_den = frame_rate_den_;
  attr.rate_control.bit_rate = bit_rate_ / 1000;  // in kbps
  attr.profile = edk::VideoProfile::HIGH;
  memset(&attr.crop_config, 0, sizeof(edk::CropConfig));
  attr.color2gray = false;
  attr.packet_buffer_num = 4;
  attr.output_on_cpu = true;
  attr.silent = false;
  attr.packet_callback = std::bind(&CNVideoEncoder::PacketCallback, this, std::placeholders::_1);
  attr.eos_callback = std::bind(&CNVideoEncoder::EosCallback, this);
  attr.perf_callback = std::bind(&CNVideoEncoder::PerfCallback, this, std::placeholders::_1);

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
  if (encoder_) {
    delete encoder_;
    encoder_ = nullptr;
  }
}

VideoEncoder::VideoFrame *CNVideoEncoder::NewFrame() { return new CNVideoFrame(this); }

void CNVideoEncoder::EncodeFrame(VideoFrame *frame) {
  CNVideoFrame *cnpic = dynamic_cast<CNVideoFrame *>(frame);
  edk::CnFrame *cnframe = cnpic->Get();

  try {
    encoder_->SendData(*cnframe, false);
  } catch (edk::EasyEncodeError &err) {
    std::cout << "CnEncodeError: " << err.what() << std::endl;
    return;
  }
}

void CNVideoEncoder::PacketCallback(const edk::CnPacket &packet) {
  if (packet.length == 0 || packet.data == 0) return;

  uint8_t *packet_data = reinterpret_cast<uint8_t *>(packet.data);
  int offset = 0;
  if (packet_data[0] == 0x00 && packet_data[1] == 0x00) {
    if (packet_data[2] == 0x01) {
      offset = 3;
    } else if ((packet_data[2] == 0x00) && (packet_data[3] == 0x01)) {
      offset = 4;
    }
  }

  int length = packet.length - offset;
  uint8_t *data = packet_data + offset;

  PushOutputBuffer(data, length, frame_count_, packet.pts);

  encoder_->ReleaseBuffer(packet.buf_id);

  frame_count_++;

  Callback(NEW_FRAME);
}

void CNVideoEncoder::EosCallback() { std::cout << "CNVideoEncoder got EOS" << std::endl; }

void CNVideoEncoder::PerfCallback(const edk::EncodePerfInfo &info) {}
