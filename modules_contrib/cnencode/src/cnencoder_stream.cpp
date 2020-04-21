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
#include <glog/logging.h>

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#else
#error OpenCV required
#endif

#include <iostream>
#include <string>
#include "cnencoder_stream.hpp"
#include "easycodec/easy_encode.h"
#include "easycodec/vformat.h"
#include "easyinfer/mlu_context.h"

#define SAVE_PACKET 1
#define ALIGN(size, alignment) (((uint32_t)(size) + (alignment)-1) & ~((alignment)-1))

CNEncoderStream::CNEncoderStream(int src_width, int src_height, int dst_width, int dst_height, float frame_rate,
                                 PictureFormat format, int bit_rate, int gop_size, CodecType type, uint8_t channelIdx,
                                 uint32_t device_id, std::string pre_type) {
  type_ = type;
  format_ = format;
  pre_type_ = pre_type;
  device_id_ = device_id;
  channelIdx_ = channelIdx;
  canvas_ = cv::Mat(src_height, src_width, CV_8UC3);

  src_width_ = src_width;
  src_height_ = src_height;
  dst_width_ = dst_width;
  dst_height_ = dst_height;
  output_frame_size_ = dst_width_ * dst_height_ * 3 / 2;
  output_data = new uint8_t[output_frame_size_];

  copy_frame_buffer_ = false;
  gop_size_ = gop_size;
  bit_rate_ = bit_rate;
  frame_rate_den_ = 1;
  frame_rate_num_ = frame_rate;
  uint32_t bps = bit_rate_ / 1000;

  LOG(INFO) << "kbps: " << bps;
  LOG(INFO) << "gop: " << gop_size_;
  LOG(INFO) << "fps: " << frame_rate;
  LOG(INFO) << "format: " << format_;
  LOG(INFO) << "device Id: " << device_id_;
  LOG(INFO) << "input_width: " << src_width_;
  LOG(INFO) << "input_height: " << src_height_;
  LOG(INFO) << "output_width: " << dst_width_;
  LOG(INFO) << "output_height: " << dst_height_;

  if ("ffmpeg" == pre_type_) {
    src_pix_fmt_ = AV_PIX_FMT_BGR24;
    switch (format_) {
      case NV21:
        LOG(INFO) << "AV_PIX_FMT_NV21";
        dst_pix_fmt_ = AV_PIX_FMT_NV21;
        break;
      case NV12:
        LOG(INFO) << "AV_PIX_FMT_NV12";
        dst_pix_fmt_ = AV_PIX_FMT_NV12;
        break;
      default:
        LOG(INFO) << "CNEncoder does not support other formate";
        break;
    }

    src_pic_ = av_frame_alloc();
    dst_pic_ = av_frame_alloc();
    if (src_pic_ == nullptr || dst_pic_ == nullptr) {
      LOG(ERROR) << "Failed allocating AVFrame for the src_pic/dst_pic";
      return;
    }
    swsctx_ = sws_getContext(src_width, src_height, src_pix_fmt_, dst_width_, dst_height_, dst_pix_fmt_,
                             SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (swsctx_ == nullptr) {
      LOG(ERROR) << "Failed sws_getContext for the src_pic & dst_pic";
      return;
    }
  }

  switch (format_) {
    case NV21:
      LOG(INFO) << "PixelFmt::NV21";
      picture_format_ = edk::PixelFmt::NV21;
      break;
    case NV12:
      LOG(INFO) << "PixelFmt::NV12";
      picture_format_ = edk::PixelFmt::NV12;
      break;
    default:
      LOG(INFO) << "default: NV21";
      picture_format_ = edk::PixelFmt::NV21;
      break;
  }
  switch (type_) {
    case H264:
      LOG(INFO) << "CodecType::H264";
      codec_type_ = edk::CodecType::H264;
      break;
    case HEVC:
      LOG(INFO) << "CodecType::HEVC";
      codec_type_ = edk::CodecType::H265;
      break;
    case MPEG4:
      LOG(INFO) << "CodecType::MPEG4";
      codec_type_ = edk::CodecType::MPEG4;
      break;
    case JPEG:
      LOG(INFO) << "CodecType::JPEG";
      codec_type_ = edk::CodecType::JPEG;
      break;
    default:
      codec_type_ = edk::CodecType::H264;
      break;
  }

  edk::MluContext context;
  try {
    context.SetDeviceId(device_id_);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError &err) {
    LOG(ERROR) << "CNEncoderStream: set mlu env failed";
    return;
  }

  edk::EasyEncode::Attr attr;
  attr.dev_id = device_id_;
  attr.frame_geometry.w = dst_width_;
  attr.frame_geometry.h = dst_height_;
  attr.pixel_format = picture_format_;
  attr.codec_type = codec_type_;
  attr.b_frame_num = 0;
  attr.input_buffer_num = 6;
  attr.output_buffer_num = 6;
  attr.gop_type = edk::GopType::BIDIRECTIONAL;
  if (type_ == H264) {
    attr.insertSpsPpsWhenIDR = 1;
    attr.level = edk::VideoLevel::H264_41;
    attr.profile = edk::VideoProfile::H264_MAIN;
  } else {
    attr.level = edk::VideoLevel::H265_MAIN_41;
    attr.profile = edk::VideoProfile::H265_MAIN;
  }
  memset(&attr.rate_control, 0, sizeof(edk::RateControl));
  attr.rate_control.vbr = false;
  attr.rate_control.gop = gop_size_;
  attr.rate_control.frame_rate_num = frame_rate_num_;
  attr.rate_control.frame_rate_den = frame_rate_den_;
  attr.rate_control.bit_rate = bit_rate_;
  attr.rate_control.max_bit_rate = bit_rate_;
  memset(&attr.crop_config, 0, sizeof(edk::CropConfig));
  attr.crop_config.enable = false;
  attr.silent = false;
  attr.jpeg_qfactor = 50;
  attr.eos_callback = std::bind(&CNEncoderStream::EosCallback, this);
  attr.packet_callback = std::bind(&CNEncoderStream::PacketCallback, this, std::placeholders::_1);

  try {
    encoder_ = edk::EasyEncode::Create(attr);
  } catch (edk::EasyEncodeError &err) {
    LOG(ERROR) << "EncodeError: " << err.what();
    if (encoder_) {
      delete encoder_;
      encoder_ = nullptr;
    }
    return;
  }
}

CNEncoderStream::~CNEncoderStream() {
  edk::MluContext context;
  try {
    context.SetDeviceId(device_id_);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError &err) {
    LOG(ERROR) << "Close(): set mlu env failed";
    return;
  }

  if ("ffmpeg" == pre_type_) {
    if (src_pic_ != nullptr) {
      av_frame_free(&src_pic_);
      src_pic_ = nullptr;
    }
    if (dst_pic_ != nullptr) {
      av_frame_free(&dst_pic_);
      dst_pic_ = nullptr;
    }
    if (swsctx_ != nullptr) {
      sws_freeContext(swsctx_);
      swsctx_ = nullptr;
    }
  }

  if (encoder_) {
    delete encoder_;
    encoder_ = nullptr;
  }
  canvas_.release();
  if (output_data != nullptr) delete[] output_data;
}

bool CNEncoderStream::Update(const cv::Mat &image, int64_t timestamp, bool eos) {
  update_lock_.lock();
  edk::CnFrame *cnframe = new edk::CnFrame;
  memset(cnframe, 0, sizeof(edk::CnFrame));
  if (!eos) {
    if ("opencv" == pre_type_) {
      cv::Mat tmp;
      canvas_ = image.clone();
      cv::resize(canvas_, tmp, cv::Size(dst_width_, dst_height_), 0, 0, cv::INTER_LINEAR);
      Bgr2YUV420NV(tmp, format_, output_data);
    } else if ("ffmpeg" == pre_type_) {
      uint32_t input_buf_size_ = src_width_ * src_height_ * 3;
      uint32_t output_buf_size_ = dst_width_ * dst_height_ * 3 / 2;
      Convert(image.data, input_buf_size_, output_data, output_buf_size_);
    }

    cnframe->pts = timestamp;
    cnframe->width = dst_width_;
    cnframe->height = dst_height_;
    cnframe->pformat = picture_format_;
    cnframe->frame_size = output_frame_size_;
    cnframe->n_planes = 2;
    cnframe->strides[0] = dst_width_;
    cnframe->strides[1] = dst_width_;
    if (copy_frame_buffer_) {
      uint8_t *ptr = new uint8_t[output_frame_size_];
      cnframe->ptrs[0] = reinterpret_cast<void *>(ptr);
      cnframe->ptrs[1] = reinterpret_cast<void *>(ptr + dst_width_ * dst_height_);
      memcpy(cnframe->ptrs[0], output_data, cnframe->frame_size);
    } else {
      cnframe->ptrs[0] = reinterpret_cast<void *>(output_data);
      cnframe->ptrs[1] = reinterpret_cast<void *>(output_data + dst_width_ * dst_height_);
    }
  }
  try {
    encoder_->SendDataCPU(*cnframe, eos);
  } catch (edk::EasyEncodeError &err) {
    LOG(ERROR) << "EncoderError: send data to cnencoder" << err.what();
    return false;
  }
  delete cnframe;
  update_lock_.unlock();
  return true;
}

bool CNEncoderStream::Update(uint8_t *image, int64_t timestamp, bool eos) {
  update_lock_.lock();
  edk::CnFrame *cnframe = new edk::CnFrame;
  memset(cnframe, 0, sizeof(edk::CnFrame));
  if (!eos) {
    ResizeYuvNearest(image, output_data);
    cnframe->pts = timestamp;
    cnframe->width = dst_width_;
    cnframe->height = dst_height_;
    cnframe->pformat = picture_format_;
    cnframe->frame_size = output_frame_size_;
    cnframe->n_planes = 2;
    cnframe->strides[0] = dst_width_;
    cnframe->strides[1] = dst_width_;
    if (copy_frame_buffer_) {
      uint8_t *ptr = new uint8_t[output_frame_size_];
      cnframe->ptrs[0] = reinterpret_cast<void *>(ptr);
      cnframe->ptrs[1] = reinterpret_cast<void *>(ptr + dst_width_ * dst_height_);
      memcpy(cnframe->ptrs[0], output_data, cnframe->frame_size);
    } else {
      cnframe->ptrs[0] = reinterpret_cast<void *>(output_data);
      cnframe->ptrs[1] = reinterpret_cast<void *>(output_data + dst_width_ * dst_height_);
    }
  }
  try {
    encoder_->SendDataCPU(*cnframe, eos);
  } catch (edk::EasyEncodeError &err) {
    LOG(ERROR) << "EncoderError: send data to cnencoder" << err.what();
    return false;
  }
  delete cnframe;
  update_lock_.unlock();
  return true;
}

void CNEncoderStream::ResizeYuvNearest(uint8_t* src, uint8_t* dst) {
  if (src_width_ == dst_width_ && src_height_ == dst_height_) {
    memcpy(dst, src, output_frame_size_ * sizeof(uint8_t));
    return;
  }
  uint32_t srcy, srcx, src_index;
  uint32_t xrIntFloat_16 = (src_width_ << 16) / dst_width_ + 1;
  uint32_t yrIntFloat_16 = (src_height_ << 16) / dst_height_ + 1;

  uint8_t* dst_uv = dst + dst_height_ * dst_width_;
  uint8_t* src_uv = src + src_height_ * src_width_;
  uint8_t* dst_uv_yScanline = nullptr;
  uint8_t* src_uv_yScanline = nullptr;
  uint8_t* dst_y_slice = dst;
  uint8_t* src_y_slice = nullptr;
  uint8_t* sp = nullptr;
  uint8_t* dp = nullptr;

  for (uint32_t y = 0; y < (dst_height_ & ~7); ++y) {
    srcy = (y * yrIntFloat_16) >> 16;
    src_y_slice = src + srcy * src_width_;
    if (0 == (y & 1)) {
      dst_uv_yScanline = dst_uv + (y / 2) * dst_width_;
      src_uv_yScanline = src_uv + (srcy / 2) * src_width_;
    }
    for (uint32_t x = 0; x < (dst_width_ & ~7); ++x) {
      srcx = (x * xrIntFloat_16) >> 16;
      dst_y_slice[x] = src_y_slice[srcx];
      if ((y & 1) == 0) {  // y is even
        if ((x & 1) == 0) {  // x is even
          src_index = (srcx / 2) * 2;
          sp = dst_uv_yScanline + x;
          dp = src_uv_yScanline + src_index;
          *sp = *dp;
          ++sp;
          ++dp;
          *sp = *dp;
        }
      }
    }
    dst_y_slice += dst_width_;
  }
}

void CNEncoderStream::Bgr2YUV420NV(const cv::Mat &bgr, PictureFormat ToFormat, uint8_t *nv_data) {
  uint32_t width, height, stride;
  width = bgr.cols;
  height = bgr.rows;
  stride = bgr.cols;

  uint8_t *src_y, *src_u, *src_v, *dst_y, *dst_uv;
  cv::Mat yuvI420 = cv::Mat(height * 3 / 2, width, CV_8UC1);
  cv::cvtColor(bgr, yuvI420, cv::COLOR_BGR2YUV_I420);

  src_y = yuvI420.data;
  src_u = yuvI420.data + width * height;
  src_v = yuvI420.data + width * height * 5 / 4;

  dst_y = nv_data;
  dst_uv = nv_data + stride * height;

  for (uint32_t i = 0; i < height; i++) {
    // y data
    memcpy(dst_y + i * stride, src_y + i * width, width);
    // uv data
    if (i % 2 == 0) {
      for (uint32_t j = 0; j < width / 2; j++) {
        if (ToFormat == NV21) {
          *(dst_uv + i * stride / 2 + 2 * j) = *(src_v + i * width / 4 + j);
          *(dst_uv + i * stride / 2 + 2 * j + 1) = *(src_u + i * width / 4 + j);
        } else {
          *(dst_uv + i * stride / 2 + 2 * j) = *(src_u + i * width / 4 + j);
          *(dst_uv + i * stride / 2 + 2 * j + 1) = *(src_v + i * width / 4 + j);
        }
      }
    }
  }
  yuvI420.release();
}

void CNEncoderStream::Convert(const uint8_t *src_buffer, const size_t src_buffer_size, uint8_t *dst_buffer,
                              const size_t dst_buffer_size) {
  size_t insize = av_image_get_buffer_size(src_pix_fmt_, src_width_, src_height_, 1);
  if (insize != src_buffer_size) {
    LOG(ERROR) << "The input buffer size does not match the expected size.  Required:" << insize
               << " Available: " << src_buffer_size;
    return;
  }

  size_t outsize = av_image_get_buffer_size(dst_pix_fmt_, dst_width_, dst_height_, 1);
  if (outsize < dst_buffer_size) {
    LOG(ERROR) << "The input buffer size does not match the expected size.Required:" << outsize
               << " Available: " << dst_buffer_size;
    return;
  }

  if (av_image_fill_arrays(src_pic_->data, src_pic_->linesize, src_buffer, src_pix_fmt_, src_width_, src_height_, 1) <=
      0) {
    LOG(ERROR) << "Failed filling input frame with input buffer";
    return;
  }

  if (av_image_fill_arrays(dst_pic_->data, dst_pic_->linesize, dst_buffer, dst_pix_fmt_, dst_width_, dst_height_, 1) <=
      0) {
    LOG(ERROR) << "Failed filling output frame with output buffer";
    return;
  }

  /* Do the conversion */
  sws_scale(swsctx_, src_pic_->data, src_pic_->linesize, 0, src_height_, dst_pic_->data, dst_pic_->linesize);
}

void CNEncoderStream::PacketCallback(const edk::CnPacket &packet) {
  if (packet.length == 0 || packet.data == 0) return;
  try {
    edk::MluContext context;
    context.SetDeviceId(device_id_);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError &err) {
    LOG(ERROR) << "PacketCallback: set mlu env faild";
    return;
  }

#if SAVE_PACKET
  if (packet.codec_type == edk::CodecType::H264) {
    snprintf(output_file, sizeof(output_file), "./output/cnencode_%d.h264", channelIdx_);
  } else if (packet.codec_type == edk::CodecType::H265) {
    snprintf(output_file, sizeof(output_file), "./output/cnencode_%d.h265", channelIdx_);
  } else if (packet.codec_type == edk::CodecType::JPEG) {
    snprintf(output_file, sizeof(output_file), "./output/cnencoded_%d_%02d.jpg", channelIdx_, frame_count_++);
  } else {
    LOG(ERROR) << "ERROR::unknown output codec type !!!" << static_cast<int>(packet.codec_type);
  }

  if (packet.codec_type == edk::CodecType::JPEG) {
    p_file = fopen(output_file, "wb");
  } else {
    if (p_file == nullptr) p_file = fopen(output_file, "wb");
    if (p_file == nullptr) {
      LOG(ERROR) << "open output file failed !!!";
    }
  }

  uint32_t length = packet.length;
  written = fwrite(packet.data, 1, length, p_file);
  if (written != length) {
    LOG(ERROR) << "ERROR: written size: " << (uint)written << "!="
               << "data length: " << length;
  }
#endif
}

void CNEncoderStream::EosCallback() {
  try {
    edk::MluContext context;
    context.SetDeviceId(device_id_);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError &err) {
    LOG(ERROR) << "set mlu env faild";
    return;
  }
  LOG(INFO) << " EosCallback ... ";
}
