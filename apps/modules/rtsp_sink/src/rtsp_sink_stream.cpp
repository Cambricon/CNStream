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

#ifdef HAVE_OPENCV
#include "opencv2/opencv.hpp"
#endif

#define DEFAULT_BPS 0x200000
#define DEFAULT_GOP 20
#define PTS_QUEUE_MAX 20
// #define CNENCODE

bool RTSPSinkJoinStream::Open(int width, int height, PictureFormat format, float refresh_rate, int udp_port,
                              int http_port, int rows, int cols, CodecHWType hw) {
  if (width < 1 || height < 1 || udp_port < 1 || http_port < 1) {
    return false;
  }
  if ( rows > 0 || cols > 0 ) {
    is_mosaic_style = true;
    cols_ = cols;
    rows_ = rows;
    mosaic_win_width_ = width/cols;
    mosaic_win_height_ = height/rows;
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
    bit_rate = 0x250000, gop_size = 10;
  else
    bit_rate = 0x400000, gop_size = 20;

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
      rtsp_ctx.hw = VideoCodecHWType::FFMPEG;
      break;
  }
  LOG(INFO) << "fps: " << rtsp_ctx.fps;
  LOG(INFO) << "format: " << rtsp_ctx.format;
  LOG(INFO) << "kbps:　" << rtsp_ctx.kbps;
  LOG(INFO) << "gop: " << rtsp_ctx.gop;
  ctx_ = StreamPipeCreate(&rtsp_ctx);

  if (format == RGB24 || format == BGR24) {
    canvas_ = cv::Mat(height, width, CV_8UC3);
  } else {
    canvas_ = cv::Mat(height * 3 / 2, width, CV_8UC1);
  }
  refresh_thread_ = new std::thread(&RTSPSinkJoinStream::RefreshLoop, this);

  std::cout << "\n***** Start RTSP server, UDP port:" << udp_port_ << ", HTTP port:" << http_port_ << std::endl;

  return true;
}

void RTSPSinkJoinStream::EncodeFrame(uint8_t *data, int64_t timestamp) { StreamPipePutPacket(ctx_, data, timestamp); }

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
  if ( is_mosaic_style && channel_id >= 0 ) {
    int x = channel_id % cols_ * mosaic_win_width_;
    int y = channel_id / cols_ * mosaic_win_height_;
    cv::resize(image, image, cv::Size(mosaic_win_width_, mosaic_win_height_), cv::INTER_CUBIC);
    image.copyTo(canvas_(cv::Rect(x, y, mosaic_win_width_, mosaic_win_height_)));
  } else {
    image.copyTo(canvas_);
  }
  canvas_lock_.unlock();
  return true;
}

void RTSPSinkJoinStream::RefreshLoop() {
  int64_t delay_us = 0;
  auto start = std::chrono::high_resolution_clock::now();
  int64_t pts_us;
  uint32_t index = 0;

  while (running_) {
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> dura = end - start;
    int64_t rt = delay_us - dura.count();
    if (rt > 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(rt));
      start = std::chrono::high_resolution_clock::now();
    } else {
      start = end;
    }

    pts_us = index++ * 1e6 / refresh_rate_;

    if (ctx_) {
      canvas_lock_.lock();
      EncodeFrame(canvas_.data, pts_us / 1000);
      canvas_lock_.unlock();
      delay_us = index * 1e6 / refresh_rate_ - pts_us;
    }
  }
}
