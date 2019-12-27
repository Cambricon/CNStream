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
#include "cnencoder_stream.hpp"

#include <glog/logging.h>

#include "easycodec/easy_encode.h"
#include "easycodec/vformat.h"
#include "easyinfer/mlu_context.h"

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#else
#error OpenCV required
#endif

CNEncoderStream::CNEncoderStream(int width, int height, float frame_rate, PictureFormat format, int bit_rate,
                                 int gop_size, CodecType type, uint8_t channelIdx, uint32_t device_id) {
  type_ = type;
  format_ = format;
  device_id_ = device_id;
  channelIdx_ = channelIdx;
  if (width < 1 || height < 1)  return;
  canvas_ = cv::Mat(height, width, CV_8UC3);

  width_ = width;
  height_ = height;
  copy_frame_buffer_ = false;
  // copy_frame_buffer_ = (frame_rate > 0);
  frame_rate_num_ = frame_rate;
  frame_rate_den_ = 1;
  gop_size_ = gop_size;
  bit_rate_ = bit_rate;
  uint32_t bps = bit_rate_/1024;

  LOG(INFO) << "device Id: " << device_id_;
  LOG(INFO) << "bps:　" << bps;
  LOG(INFO) << "fps: " << frame_rate;
  LOG(INFO) << "gop: " << gop_size_;
  LOG(INFO) << "format: " << format_;
  LOG(INFO) << "width: " << width_;
  LOG(INFO) << "height: " << height_;

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
    // context.SetChannelId(0);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError &err) {
    std::cout << "CNEncoderStream: set mlu env failed" << std::endl;
    return;
  }

  edk::EasyEncode::Attr attr;
  attr.maximum_geometry.w = width_;
  attr.maximum_geometry.h = height_;
  attr.output_geometry.w = width_;
  attr.output_geometry.h = height_;
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
    // context.SetChannelId(0);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError &err) {
    std::cout << "Close(): set mlu env failed" << std::endl;
    return;
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
      // context.SetChannelId(0);
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
  canvas_lock_.lock();
  canvas_ = image.clone();
  frame_size_ = image.cols * image.rows * 3 / 2;
  uint8_t *nv_data_buf = new uint8_t[frame_size_];
  Bgr2YUV420NV(canvas_, format_, nv_data_buf);
  SendFrame(nv_data_buf, timestamp);
  delete[] nv_data_buf;
  nv_data_buf = nullptr;
  canvas_lock_.unlock();
  return true;
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
  // data is nv21 or nv12
  if (!running_) return false;
  edk::CnFrame *cnframe = new edk::CnFrame;
  memset(cnframe, 0, sizeof(edk::CnFrame));

  input_mutex_.lock();
  if (input_data_q_.size() <= input_queue_size_) {
    cnframe->pts = timestamp;
    cnframe->width = width_;
    cnframe->height = height_;
    cnframe->pformat = picture_format_;
    cnframe->frame_size = frame_size_;
    cnframe->n_planes = 2;
    cnframe->strides[0] = width_;
    cnframe->strides[1] = width_;
    if (copy_frame_buffer_) {
      uint8_t *ptr = new uint8_t[frame_size_];
      cnframe->ptrs[0] = reinterpret_cast<void *>(ptr);
      cnframe->ptrs[1] = reinterpret_cast<void *>(ptr + width_ * height_);
      memcpy(cnframe->ptrs[0], data, cnframe->frame_size);
    } else {
      cnframe->ptrs[0] = reinterpret_cast<void *>(data);
      cnframe->ptrs[1] = reinterpret_cast<void *>(data + width_ * height_);
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

void CNEncoderStream::RefreshEOS(bool eos) {
  edk::MluContext context;
  context.SetDeviceId(device_id_);
  // context.SetChannelId(0);
  context.ConfigureForThisThread();
  // std::cout << "receive eos!" <<std::endl;
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
    // context.SetChannelId(0);
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
    // context.SetChannelId(0);
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
    // context.SetChannelId(0);
    context.ConfigureForThisThread();
  } catch(edk::MluContextError & err) {
    std::cout << "set mlu env faild" << std::endl;
    return;
  }
}
