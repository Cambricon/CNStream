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

#ifndef RTSP_SINK_STREAM_HPP_
#define RTSP_SINK_STREAM_HPP_

#include "RtspStreamPipe.h"

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#else
#error OpenCV required
#endif
#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>
#include <thread>

class RTSPSinkJoinStream {
 public:
  enum PictureFormat {
    YUV420P = 0,
    RGB24,
    BGR24,
    NV21,
    NV12,
  };
  typedef enum CodecHWType_enum {
    FFMPEG = 0,
    MLU,
  } CodecHWType;
  bool Open(int width, int height, PictureFormat format, float refresh_rate, int udp_port, int http_port, int rows,
            int cols, int device_id, CodecHWType hw);
  void Close();
  bool Update(cv::Mat image, int64_t timestamp, int channel_id = -1);
  void Bgr2YUV420NV(const cv::Mat &bgr, bool isnv21, uint8_t *nv_data);

 private:
  void RefreshLoop();
  void EncodeFrame(const cv::Mat &bgr24, int64_t timestamp);
  PictureFormat pix_format_;
  std::mutex canvas_lock_;
  cv::Mat canvas_;
  std::thread *refresh_thread_ = nullptr;
  int udp_port_;
  int http_port_;
  int mosaic_win_width_;
  int mosaic_win_height_;
  int cols_;
  int rows_;
  int device_id_;
  float refresh_rate_ = 0;
  StreamPipeCtx *ctx_ = nullptr;
  bool running_ = false;
  bool start_refresh_ = false;
  bool is_mosaic_style = false;
};

#endif  // RTSP_SINK_JOIN_STREAM_HPP_
