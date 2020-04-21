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
#include "rtsp_sink_stream.hpp"
#include <glog/logging.h>

#include "easyinfer/mlu_context.h"

#ifdef HAVE_OPENCV
#include "opencv2/opencv.hpp"
#endif

#define DEFAULT_BPS 0x200000
#define DEFAULT_GOP 20
#define PTS_QUEUE_MAX 20
// #define CNENCODE

bool RTSPSinkJoinStream::Open(int width, int height, PictureFormat format, float refresh_rate, int udp_port,
                              int http_port, int rows, int cols, int device_id, CodecHWType hw) {
  if (width < 1 || height < 1 || udp_port < 1 || http_port < 1) {
    return false;
  }

  device_id_ = device_id;
  if (rows > 0 || cols > 0) {
    is_mosaic_style = true;
    cols_ = cols;
    rows_ = rows;
    if (6 == cols_ * rows_) {
      mosaic_win_width_ = width / cols;
      mosaic_win_height_ = height / cols;
    } else {
      mosaic_win_width_ = width / cols;
      mosaic_win_height_ = height / rows;
    }
  }
  refresh_rate_ = refresh_rate;
  udp_port_ = udp_port > 0 ? udp_port : 8554;
  http_port_ = http_port > 0 ? http_port : 8080;
  if (refresh_rate_ <= 0) {
    refresh_rate_ = 25.0;
  }
  uint32_t bit_rate = DEFAULT_BPS;
  uint32_t gop_size = DEFAULT_GOP;
  if (height <= 720)
    bit_rate = 0x100000, gop_size = 10;
  else
    bit_rate = 0x200000, gop_size = 20;

  running_ = true;
  StreamContext rtsp_ctx;
  rtsp_ctx.udp_port = udp_port_;
  rtsp_ctx.http_port = http_port_;
  rtsp_ctx.fps = refresh_rate_;
  rtsp_ctx.kbps = bit_rate / 1000;
  rtsp_ctx.gop = gop_size;
  rtsp_ctx.width_out = width;
  rtsp_ctx.height_out = height;
  rtsp_ctx.width_in = width;
  rtsp_ctx.height_in = height;

  switch (format) {
    case YUV420P:
      rtsp_ctx.format = ColorFormat_YUV420;
      break;
    case BGR24:
      rtsp_ctx.format = ColorFormat_BGR24;
      break;
    case NV21:
      rtsp_ctx.format = ColorFormat_NV21;
      break;
    case NV12:
      rtsp_ctx.format = ColorFormat_NV12;
      break;
    default:
      rtsp_ctx.format = ColorFormat_BGR24;
      break;
  }
  switch (hw) {
    case FFMPEG:
      rtsp_ctx.hw = VideoCodecHWType::FFMPEG;
      break;
    case MLU:
      rtsp_ctx.hw = VideoCodecHWType::MLU;
      break;
    default:
      rtsp_ctx.hw = VideoCodecHWType::MLU;
      break;
  }
  LOG(INFO) << "fps: " << rtsp_ctx.fps;
  LOG(INFO) << "format: " << rtsp_ctx.format;
  LOG(INFO) << "kbps: " << rtsp_ctx.kbps;
  LOG(INFO) << "gop: " << rtsp_ctx.gop;
  LOG(INFO) << "code type: " << rtsp_ctx.hw;
  ctx_ = StreamPipeCreate(&rtsp_ctx, device_id_);

  canvas_ = cv::Mat(height, width, CV_8UC3);  // for bgr24

  refresh_thread_ = new std::thread(&RTSPSinkJoinStream::RefreshLoop, this);

  std::cout << "\n***** Start RTSP server, UDP port:" << udp_port_ << ", HTTP port:" << http_port_ << std::endl;

  return true;
}

void RTSPSinkJoinStream::EncodeFrame(const cv::Mat &bgr24, int64_t timestamp) {
  uint8_t *nv_data = new uint8_t[bgr24.cols * bgr24.rows * 3 / 2];
  bool is_nv21 = true;
  Bgr2YUV420NV(bgr24, is_nv21, nv_data);
  StreamPipePutPacket(ctx_, nv_data, timestamp);
  delete[] nv_data;
  // StreamPipePutPacket(ctx_, bgr24.data, timestamp);
}

void RTSPSinkJoinStream::Close() {
  running_ = false;
  if (refresh_thread_ && refresh_thread_->joinable()) {
    refresh_thread_->join();
    delete refresh_thread_;
    refresh_thread_ = nullptr;
  }

  StreamPipeClose(ctx_);
  canvas_.release();
  std::cout << "Release stream resources" << std::endl;
}

bool RTSPSinkJoinStream::Update(cv::Mat image, int64_t timestamp, int channel_id) {
  canvas_lock_.lock();
  if (is_mosaic_style && channel_id >= 0) {
    if (6 == cols_ * rows_) {
      int x = channel_id % cols_ * mosaic_win_width_;
      int y = channel_id / cols_ * mosaic_win_height_;
      switch (channel_id) {
        case 0:
          cv::resize(image, image, cv::Size(mosaic_win_width_ * 2, mosaic_win_height_ * 2), cv::INTER_CUBIC);
          image.copyTo(canvas_(cv::Rect(x, y, mosaic_win_width_ * 2, mosaic_win_height_ * 2)));
          break;
        case 1:
          x += mosaic_win_width_;
          cv::resize(image, image, cv::Size(mosaic_win_width_, mosaic_win_height_), cv::INTER_CUBIC);
          image.copyTo(canvas_(cv::Rect(x, y, mosaic_win_width_, mosaic_win_height_)));
          break;
        default:
          y += mosaic_win_height_;
          cv::resize(image, image, cv::Size(mosaic_win_width_, mosaic_win_height_), cv::INTER_CUBIC);
          image.copyTo(canvas_(cv::Rect(x, y, mosaic_win_width_, mosaic_win_height_)));
          break;
      }
    } else {
      int x = channel_id % cols_ * mosaic_win_width_;
      int y = channel_id / cols_ * mosaic_win_height_;
      cv::resize(image, image, cv::Size(mosaic_win_width_, mosaic_win_height_), cv::INTER_CUBIC);
      image.copyTo(canvas_(cv::Rect(x, y, mosaic_win_width_, mosaic_win_height_)));
    }
  } else {
    image.copyTo(canvas_);
  }
  canvas_lock_.unlock();
  return true;
}

void RTSPSinkJoinStream::RefreshLoop() {
  edk::MluContext context;
  context.SetDeviceId(device_id_);
  context.ConfigureForThisThread();

  int64_t delay_us = 0;
  auto start = std::chrono::steady_clock::now();
  int64_t pts_us;
  uint32_t index = 0;

  while (running_) {
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::micro> dura = end - start;
    int64_t rt = delay_us - dura.count();
    if (rt > 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(rt));
      start = std::chrono::steady_clock::now();
    } else {
      start = end;
    }

    pts_us = index++ * 1e6 / refresh_rate_;

    if (ctx_) {
      canvas_lock_.lock();
      EncodeFrame(canvas_, pts_us / 1000);
      canvas_lock_.unlock();
      delay_us = index * 1e6 / refresh_rate_ - pts_us;
    }
  }
}

void RTSPSinkJoinStream::Bgr2YUV420NV(const cv::Mat &bgr, bool isnv21, uint8_t *nv_data) {
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
        if (isnv21) {
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
