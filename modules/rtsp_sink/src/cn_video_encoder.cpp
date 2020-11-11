/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *************************************************************************/

#include "cn_video_encoder.hpp"

#include <glog/logging.h>

#include <cstring>
#include <functional>
#include <sstream>
#include <string>

#define OUTPUT_BUFFER_SIZE 0x200000

namespace cnstream {

CNVideoEncoder::CNVideoFrame::CNVideoFrame(CNVideoEncoder *encoder) : encoder_(encoder) {
  frame_ = new edk::CnFrame;
  memset(frame_, 0, sizeof(edk::CnFrame));

  frame_->width = encoder_->rtsp_param_.dst_width;
  frame_->height = encoder_->rtsp_param_.dst_height;
  frame_->pformat = encoder_->picture_format_;

  if (frame_->pformat == edk::PixelFmt::NV21 || frame_->pformat == edk::PixelFmt::NV12) {
    frame_->frame_size = frame_->width * frame_->height * 3 / 2;
    frame_->n_planes = 2;
    frame_->strides[0] = frame_->width;
    frame_->strides[1] = frame_->width;
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
    LOG(INFO) << "Unsupport Pixel Format: " << static_cast<int>(frame_->pformat) << std::endl;
  }
}

CNVideoEncoder::CNVideoEncoder(const RtspParam &rtsp_param) : VideoEncoder(OUTPUT_BUFFER_SIZE) {
  rtsp_param_ = rtsp_param;
  switch (rtsp_param.color_format) {
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
  switch (rtsp_param.codec_type) {
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

  frame_rate_den_ = 1;
  frame_rate_num_ = rtsp_param.frame_rate;

  edk::MluContext context;
  context.SetDeviceId(rtsp_param.device_id);
  context.ConfigureForThisThread();

  edk::EasyEncode::Attr attr;
  attr.b_frame_num = 0;
  attr.input_buffer_num = 6;
  attr.output_buffer_num = 6;
  attr.max_mb_per_slice = 0;
  attr.insertSpsPpsWhenIDR = 1;
  attr.dev_id = rtsp_param.device_id;
  attr.codec_type = codec_type_;
  attr.pixel_format = picture_format_;
  attr.frame_geometry.w = rtsp_param.dst_width;
  attr.frame_geometry.h = rtsp_param.dst_height;
  attr.gop_type = edk::GopType::BIDIRECTIONAL;
  if (rtsp_param.codec_type == H264) {
    attr.level = edk::VideoLevel::H264_41;
    attr.profile = edk::VideoProfile::H264_MAIN;
  } else {
    attr.level = edk::VideoLevel::H265_MAIN_41;
    attr.profile = edk::VideoProfile::H265_MAIN;
  }
  memset(&attr.rate_control, 0, sizeof(edk::RateControl));
  attr.rate_control.vbr = false;
  attr.rate_control.gop = rtsp_param.gop;
  attr.rate_control.bit_rate = rtsp_param.kbps * 1000;
  attr.rate_control.max_bit_rate = rtsp_param.kbps * 1000;
  attr.rate_control.frame_rate_num = frame_rate_num_;
  attr.rate_control.frame_rate_den = frame_rate_den_;
  memset(&attr.crop_config, 0, sizeof(edk::CropConfig));
  attr.silent = false;
  attr.jpeg_qfactor = 50;
  attr.crop_config.enable = false;
  attr.packet_callback = std::bind(&CNVideoEncoder::PacketCallback, this, std::placeholders::_1);
  attr.eos_callback = std::bind(&CNVideoEncoder::EosCallback, this);

  /*
  if ("mlu" == pre_type_) {
    resize_ = new edk::MluResizeYuv2Yuv();
    edk::MluResizeAttr resize_attr;
    resize_attr.batch_size = 1;
    resize_attr.src_h = src_height;
    resize_attr.src_w = src_width;
    resize_attr.dst_h = dst_height;
    resize_attr.dst_w = dst_width;
    resize_attr.core_number = 4;
    if (src_height < dst_height || src_width < dst_width) {
      LOG(ERROR) << "MLU Resize does not sopport up scaler," <<
                    "source width and heigit must be lagger then dstination width and height";
      return;
    }

    if (!resize_->Init(resize_attr)) {
      resize_->Destroy();
      delete resize_;
      LOG(ERROR) << "resize Init() failed";
    }
  }
  */
  try {
    encoder_ = edk::EasyEncode::Create(attr);
  } catch (edk::Exception &err) {
    LOG(INFO) << "CnEncodeError: " << err.what();
    Destroy();
    return;
  }
}

CNVideoEncoder::~CNVideoEncoder() {
  try {
    edk::MluContext context;
    context.SetDeviceId(rtsp_param_.device_id);
    context.BindDevice();
  } catch (edk::Exception &err) {
    LOG(ERROR) << "CNEncoderStream: set mlu env failed";
  }
  Stop();
  Destroy();
}

void CNVideoEncoder::Destroy() {
  if (encoder_) {
    delete encoder_;
    encoder_ = nullptr;
  }
  /*
  if ("mlu" == pre_type_) {
    resize_->Destroy();
    delete resize_;
  }
  */
}

VideoEncoder::VideoFrame *CNVideoEncoder::NewFrame() { return new CNVideoFrame(this); }

void CNVideoEncoder::EncodeFrame(VideoFrame *frame) {
  CNVideoFrame *cnpic = dynamic_cast<CNVideoFrame *>(frame);
  edk::CnFrame *cnframe = cnpic->Get();
  try {
    encoder_->SendDataCPU(*cnframe, false);
  } catch (edk::Exception &err) {
    LOG(INFO) << "CnEncodeError: " << err.what();
    return;
  }
}

/*
void CNVideoEncoder::EncodeFrame(void *y, void *uv, int64_t timestamp) {
  edk::CnFrame *cnframe = new edk::CnFrame;
  memset(cnframe, 0, sizeof(edk::CnFrame));
  uint64_t mlu_output_y, mlu_output_uv;
  encoder_->GetEncoderInputAddress(&mlu_output_y, &mlu_output_uv);
  if(resize_->InvokeOp(reinterpret_cast<void*>(mlu_output_y),
                       reinterpret_cast<void*>(mlu_output_uv), y, uv)) {
    LOG(INFO) << "CnEncodeError: InvokeOp error!!!" << std::endl;
    return;
  }
  cnframe->pts = timestamp;
  cnframe->width = picture_width_;
  cnframe->height = picture_height_;
  cnframe->pformat = picture_format_;
  cnframe->frame_size = picture_width_ * picture_height_ * 3 / 2;

  cnframe->n_planes = 2;
  cnframe->using_mlu_address = true;
  cnframe->strides[0] = picture_width_;
  cnframe->strides[1] = picture_width_;
  cnframe->mlu_ptrs[0] = reinterpret_cast<void*>(mlu_output_y);
  cnframe->mlu_ptrs[1] = reinterpret_cast<void*>(mlu_output_uv);
  try {
    encoder_->SendData(*cnframe, false);
  } catch (edk::Exception &err) {
    LOG(INFO) << "CnEncodeError: " << err.what();
    return;
  }
}
*/

uint32_t CNVideoEncoder::GetOffset(const uint8_t *data) {
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
  context.SetDeviceId(rtsp_param_.device_id);
  context.ConfigureForThisThread();
  // std::cout << "===got packet: size=" << packet.length << ", pts=" << packet.pts << std::endl;
  if (0 == (uint32_t)packet.slice_type) return;

  uint32_t length = packet.length;
  uint8_t *packet_data = reinterpret_cast<uint8_t *>(packet.data);
  uint32_t offset = GetOffset(packet_data);
  PushOutputBuffer(packet_data + offset, length - offset, frame_count_, packet.pts);
  encoder_->ReleaseBuffer(packet.buf_id);
  frame_count_++;
  Callback(NEW_FRAME);
}

void CNVideoEncoder::EosCallback() {
  edk::MluContext context;
  context.SetDeviceId(rtsp_param_.device_id);
  context.ConfigureForThisThread();
  LOG(INFO) << "CNVideoEncoder got EOS";
}

}  // namespace cnstream
