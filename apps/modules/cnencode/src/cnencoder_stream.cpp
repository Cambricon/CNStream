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

#include <string>
#include <iostream>
#include "cnencoder_stream.hpp"
#include "easycodec/easy_encode.h"
#include "easycodec/vformat.h"
#include "easyinfer/mlu_context.h"

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

  copy_frame_buffer_ = false;
  gop_size_ = gop_size;
  bit_rate_ = bit_rate;
  frame_rate_den_ = 1;
  frame_rate_num_ = frame_rate;
  uint32_t bps = bit_rate_/1024;

  LOG(INFO) << "bps:　" << bps;
  LOG(INFO) << "fps: " << frame_rate;
  LOG(INFO) << "gop: " << gop_size_;
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
        std::cout << "AV_PIX_FMT_NV21" << std::endl;
        dst_pix_fmt_ = AV_PIX_FMT_NV21;
        break;
      case NV12:
        std::cout << "AV_PIX_FMT_NV12" << std::endl;
        dst_pix_fmt_ = AV_PIX_FMT_NV12;
        break;
      default:
        std::cout << "CNEncoder does not support other formate" << std::endl;
        break;
    }

    src_pic_ = av_frame_alloc();
    dst_pic_ = av_frame_alloc();
    if (src_pic_ == nullptr || dst_pic_ == nullptr) {
      LOG(ERROR) << "Failed allocating AVFrame for the src_pic/dst_pic";
      return;
    }
    swsctx_ = sws_getContext(src_width, src_height, src_pix_fmt_, dst_width_, dst_height_,
                            dst_pix_fmt_, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (swsctx_ == nullptr) {
      LOG(ERROR) << "Failed sws_getContext for the src_pic & dst_pic";
      return;
    }
  }

  switch (format_) {
    case YUV420P:
      std::cout << "CNEncoder does not support YUV420P" << std::endl;
      return;
    case RGB24:
      std::cout << "CNEncoder does not support RGB24" << std::endl;
      return;
    case BGR24:
      std::cout << "CNEncoder does not support BGR24" << std::endl;
      return;
    case NV21:
      std::cout << "PixelFmt::YUV420SP_NV21" << std::endl;
      picture_format_ = edk::PixelFmt::YUV420SP_NV21;
      break;
    case NV12:
      std::cout << "PixelFmt::YUV420SP_NV12" << std::endl;
      picture_format_ = edk::PixelFmt::YUV420SP_NV12;
      break;
    default:
      std::cout << "default: YUV420SP_NV12" << std::endl;
      picture_format_ = edk::PixelFmt::YUV420SP_NV12;
      break;
  }
  switch (type_) {
    case H264:
      std::cout << "CodecType::H264" << std::endl;
      codec_type_ = edk::CodecType::H264;
      break;
    case HEVC:
      std::cout << "CodecType::HEVC" << std::endl;
      codec_type_ = edk::CodecType::H265;
      break;
    case MPEG4:
      std::cout << "CodecType::MPEG4" << std::endl;
      codec_type_ = edk::CodecType::MPEG4;
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
    std::cout << "CNEncoderStream: set mlu env failed" << std::endl;
    return;
  }

  edk::EasyEncode::Attr attr;
  attr.maximum_geometry.w = dst_width_;
  attr.maximum_geometry.h = dst_height_;
  attr.output_geometry.w = dst_width_;
  attr.output_geometry.h = dst_height_;
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
  attr.rate_control.bit_rate = bit_rate_;  // in kbps
  attr.profile = edk::VideoProfile::MAIN;  // HIGH
  memset(&attr.crop_config, 0, sizeof(edk::CropConfig));
  attr.color2gray = false;
  attr.packet_buffer_num = 4;
  attr.output_on_cpu = false;
  attr.silent = false;
  attr.eos_callback = std::bind(&CNEncoderStream::EosCallback, this);
  attr.perf_callback = std::bind(&CNEncoderStream::PerfCallback, this, std::placeholders::_1);
  attr.packet_callback = std::bind(&CNEncoderStream::PacketCallback, this, std::placeholders::_1);

  try {
    encoder_ = edk::EasyEncode::Create(attr);
  } catch (edk::EasyEncodeError &err) {
    std::cout << "EncodeError: " << err.what() << std::endl;
    if (encoder_) {
      delete encoder_;
      encoder_ = nullptr;
    }
    return;
  }
}

void CNEncoderStream::Open() {
  running_ = true;
  if (encode_thread_ == nullptr) {
    encode_thread_ = new std::thread(std::bind(&CNEncoderStream::Loop, this));
  }
}

void CNEncoderStream::Close() {
  edk::MluContext context;
  try {
    context.SetDeviceId(device_id_);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError &err) {
    std::cout << "Close(): set mlu env failed" << std::endl;
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

  running_ = false;
  if (encoder_) {
    delete encoder_;
    encoder_ = nullptr;
  }

  if (encode_thread_ && encode_thread_->joinable()) {
    encode_thread_->join();
    delete encode_thread_;
    encode_thread_ = nullptr;
  }

  edk::CnFrame *release_frame_;
  while (input_data_q_.size()) {
    release_frame_ = input_data_q_.front();
    if (release_frame_->ptrs[0] != nullptr && copy_frame_buffer_) {
      uint8_t *ptr = reinterpret_cast<uint8_t *>(release_frame_->ptrs[0]);
      delete[] ptr;
    }
    delete release_frame_;
    release_frame_ = nullptr;
    input_data_q_.pop();
  }

  canvas_.release();
}

void CNEncoderStream::Loop() {
  edk::CnFrame *frame = nullptr;
  while (running_) {
    input_mutex_.lock();
    if (!input_data_q_.empty()) {
      frame = input_data_q_.front();
      input_mutex_.unlock();
    } else {
      input_mutex_.unlock();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      continue;
    }
    if (frame != nullptr) {
      edk::MluContext context;
      context.SetDeviceId(device_id_);
      context.ConfigureForThisThread();
      try {
        encoder_->SendData(*frame, false);
      } catch (edk::EasyEncodeError &err) {
        std::cout << "EncoderError: send data to cnencoder" << err.what() << std::endl;
        return;
      }
      delete frame;
    }
    input_mutex_.lock();
    input_data_q_.pop();
    input_mutex_.unlock();
  }
}

bool CNEncoderStream::Update(const cv::Mat &image, int64_t timestamp, int channel_id) {
  update_lock_.lock();
  uint8_t *nv_data_buf = new uint8_t[output_frame_size_];
  // start_time_ = std::chrono::high_resolution_clock::now();
  if ("opencv" == pre_type_) {
    cv::Mat tmp;
    canvas_ = image.clone();
    cv::resize(canvas_, tmp, cv::Size(dst_width_, dst_height_), 0, 0, cv::INTER_LINEAR);
    Bgr2YUV420NV(tmp, format_, nv_data_buf);
  } else if ("ffmpeg" == pre_type_) {
    uint32_t input_buf_size_ = src_width_ * src_height_ * 3;
    uint32_t output_buf_size_ = dst_width_ * dst_height_ * 3 / 2;
    Convert(image.data, input_buf_size_, nv_data_buf, output_buf_size_);
  }
  // end_time_ = std::chrono::high_resolution_clock::now();
  // std::chrono::duration<double, std::milli> diff = end_time_ - start_time_;
  // std::cout << "colorsapce convert and resize run time: " << diff.count() << " ms" << std::endl;

  SendFrame(nv_data_buf, timestamp);
  delete[] nv_data_buf;
  nv_data_buf = nullptr;
  update_lock_.unlock();
  return true;
}

bool CNEncoderStream::Update(uint8_t *image, int64_t timestamp, int channel_id) {
  update_lock_.lock();
  uint8_t *output_data = new uint8_t[output_frame_size_];
  // start_time_ = std::chrono::high_resolution_clock::now();
  ResizeYUV(image, output_data);
  // end_time_ = std::chrono::high_resolution_clock::now();
  // std::chrono::duration<double, std::milli> diff = end_time_ - start_time_;
  // std::cout << "mlu resize run time: " << diff.count() << " ms" << std::endl;

  SendFrame(output_data, timestamp);
  delete[] output_data;
  output_data = nullptr;
  update_lock_.unlock();
  return true;
}

void CNEncoderStream::ResizeYUV(const uint8_t *src, uint8_t *dst) {
  if (src_width_ == dst_width_ && src_height_ == dst_height_) {
    memcpy(dst, src, src_width_*src_height_*sizeof(uint8_t));
    return;
  }
  const uint8_t *src_plane_y = src;
  const uint8_t *src_plane_uv = src + src_width_ * src_height_;
  uint8_t *dst_plane_y = dst;
  uint8_t *dst_plane_uv = dst + dst_width_ * dst_height_;
  if (src_width_ > dst_width_) {
    int x_scale = std::round(src_width_*1.0 / dst_width_);
    int y_scale = std::round(src_height_*1.0 / dst_height_);
    for (uint32_t i = 0; i < dst_height_; i++) {
      for (uint32_t j = 0; j < dst_width_; j++) {
        *(dst_plane_y + i * dst_width_ + j) = *(src_plane_y + i * y_scale*src_width_ + j * x_scale);
      }
    }
    for (uint32_t i = 0; i < dst_height_/2; i++) {
      for (uint32_t j = 0; j < dst_width_; j += 2) {
        *(dst_plane_uv + i * dst_width_ + j) = *(src_plane_uv + i * y_scale * src_width_ + j * x_scale);
        *(dst_plane_uv + i * dst_width_ + j + 1) = *(src_plane_uv + i * y_scale * src_width_ + j * x_scale + 1);
      }
    }
  } else {
    int x_scale = dst_width_ / src_width_;
    int y_scale = dst_height_ / src_height_;
    for (uint32_t i = 0; i < src_height_; i++) {
      for (uint32_t j = 0; j < src_width_; j++) {
        uint8_t y_value = *(src_plane_y + i * src_width_ + j);
        for (int k = 0; k < x_scale; k++) {
          *(dst_plane_y + i * y_scale * dst_width_ + j * x_scale + k) = y_value;
        }
      }
      uint8_t *src_row = dst_plane_y + i * y_scale * dst_width_;
      uint8_t *dst_row = nullptr;
      for (int t = 1; t < y_scale; t++) {
        dst_row = (dst_plane_y + (i * y_scale + t) * dst_width_);
        memcpy(dst_row, src_row, dst_width_ * sizeof(uint8_t));
      }
    }
    for (uint32_t i = 0; i < src_height_/2; i++) {
      for (uint32_t j = 0; j < src_width_; j += 2) {
        uint8_t v_value = *(src_plane_uv + i * src_width_ + j);
        uint8_t u_value = *(src_plane_uv + i * src_width_ + j + 1);
        for (int k = 0; k < x_scale*2; k +=2) {
          *(dst_plane_uv + i * y_scale * dst_width_ + j * x_scale + k) = v_value;
          *(dst_plane_uv + i * y_scale * dst_width_ + j * x_scale + k + 1) = u_value;
        }
      }
      uint8_t *src_row = dst_plane_uv + i * y_scale * dst_width_;
      uint8_t *dst_row = nullptr;
      for (int t = 1; t < y_scale; t++) {
        dst_row = (dst_plane_uv + (i * y_scale + t) * dst_width_);
        memcpy(dst_row, src_row, dst_width_ * sizeof(uint8_t));
      }
    }
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

bool CNEncoderStream::SendFrame(uint8_t *data, int64_t timestamp) {
  if (!running_) return false;
  edk::CnFrame *cnframe = new edk::CnFrame;
  memset(cnframe, 0, sizeof(edk::CnFrame));
  while (input_data_q_.size() >= input_queue_size_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  input_mutex_.lock();
  if (input_data_q_.size() <= input_queue_size_) {
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
      memcpy(cnframe->ptrs[0], data, cnframe->frame_size);
    } else {
      cnframe->ptrs[0] = reinterpret_cast<void *>(data);
      cnframe->ptrs[1] = reinterpret_cast<void *>(data + dst_width_ * dst_height_);
    }
    input_data_q_.push(cnframe);
  } else {
    delete cnframe;
    std::cout << "input queue full, frame drop out!" <<std::endl;
    input_mutex_.unlock();
    return false;
  }
  input_mutex_.unlock();
  return true;
}

void CNEncoderStream::Convert(const uint8_t* src_buffer, const size_t src_buffer_size,
            uint8_t* dst_buffer, const size_t dst_buffer_size) {
  size_t insize = av_image_get_buffer_size(src_pix_fmt_, src_width_, src_height_, 1);
  if (insize != src_buffer_size) {
    LOG(ERROR) <<"The input buffer size does not match the expected size.  Required:"
               << insize << " Available: " << src_buffer_size;
    return;
  }

  size_t outsize = av_image_get_buffer_size(dst_pix_fmt_, dst_width_, dst_height_, 1);
  if (outsize < dst_buffer_size) {
    LOG(ERROR) << "The input buffer size does not match the expected size.Required:"
               << outsize << " Available: " << dst_buffer_size;
    return;
  }

  if (av_image_fill_arrays(src_pic_->data, src_pic_->linesize,
                           src_buffer, src_pix_fmt_, src_width_, src_height_, 1) <= 0) {
    LOG(ERROR) <<"Failed filling input frame with input buffer";
    return;
  }

  if (av_image_fill_arrays(dst_pic_->data, dst_pic_->linesize,
                           dst_buffer, dst_pix_fmt_, dst_width_, dst_height_, 1) <= 0) {
    LOG(ERROR) <<"Failed filling output frame with output buffer";
    return;
  }

  /* Do the conversion */
  sws_scale(swsctx_, src_pic_->data, src_pic_->linesize, 0, src_height_, dst_pic_->data, dst_pic_->linesize);
}

void CNEncoderStream::RefreshEOS(bool eos) {
  edk::MluContext context;
  context.SetDeviceId(device_id_);
  context.ConfigureForThisThread();
  while (input_data_q_.size() != 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  edk::CnFrame *eosframe = new edk::CnFrame;
  memset(eosframe, 0, sizeof(edk::CnFrame));
  encoder_->SendData(*eosframe, eos);
  delete eosframe;
  eosframe = nullptr;
}

void CNEncoderStream::PacketCallback(const edk::CnPacket &packet) {
  if (packet.length == 0 || packet.data == 0) return;
  try {
    edk::MluContext context;
    context.SetDeviceId(device_id_);
    context.ConfigureForThisThread();
  } catch(edk::MluContextError & err) {
    std::cout << "PacketCallback: set mlu env faild" << std::endl;
    return;
  }

  if (packet.codec_type == edk::CodecType::H264) {
    snprintf(output_file, sizeof(output_file), "./output/cnencode_%d.h264", channelIdx_);
  } else if (packet.codec_type == edk::CodecType::H265) {
    snprintf(output_file, sizeof(output_file), "./output/cnencode_%d.h265", channelIdx_);
  } else {
    std::cout << "ERROR::unknown output codec type !!!" << static_cast<int>(packet.codec_type)<< std::endl;
  }

  if (p_file == nullptr) p_file = fopen(output_file, "wb");
  if ( p_file == nullptr) { std::cout << "open output file failed !!!" <<std::endl;}

  uint32_t length = packet.length;
  uint8_t *buffer = new uint8_t[length];
  if (buffer == nullptr) {
    printf("ERROR: new for output buffer failed!\n");
    return;
  }
  encoder_ -> CopyPacket(buffer, packet);
  written = fwrite(buffer, 1, length, p_file);
  if (written != length) {
    std::cout << "ERROR: written size: " << (uint)written << "!=" << "data length: " << length <<std::endl;
  }
  delete[] buffer; buffer = nullptr;
}

void CNEncoderStream::EosCallback() {
  try {
    edk::MluContext context;
    context.SetDeviceId(device_id_);
    context.ConfigureForThisThread();
  } catch(edk::MluContextError & err) {
    std::cout << "set mlu env faild" << std::endl;
    return;
  }
  // std::cout << " EosCallback ... " << std::endl;
}

void CNEncoderStream::PerfCallback(const edk::EncodePerfInfo &info) {
  try {
    edk::MluContext context;
    context.SetDeviceId(device_id_);
    context.ConfigureForThisThread();
  } catch(edk::MluContextError & err) {
    std::cout << "set mlu env faild" << std::endl;
    return;
  }
}
