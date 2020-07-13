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

#ifndef MODULES_RTSP_SINK_INCLUDE_RTSP_SINK_STREAM_HPP_
#define MODULES_RTSP_SINK_INCLUDE_RTSP_SINK_STREAM_HPP_

#ifdef HAVE_OPENCV
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#else
#error OpenCV required
#endif

#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace cnstream {

struct RtspParam;
struct StreamPipeCtx;

class RtspSinkJoinStream {
 public:
  bool Open(const RtspParam &rtsp_param);
  void Close();
  bool UpdateYUV(uint8_t *image, int64_t timestamp);
  // bool UpdateYUVs(void *y, void *yu, int64_t timestamp);
  bool UpdateBGR(cv::Mat image, int64_t timestamp, int channel_id = -1);
  void Bgr2Yuv420nv(const cv::Mat &bgr, uint8_t *nv_data);
  void ResizeYuvNearest(uint8_t *src, uint8_t *dst);

 private:
  void RefreshLoop();
  void EncodeFrameYUV(uint8_t *in_data, int64_t timestamp);
  void EncodeFrameBGR(const cv::Mat &bgr24, int64_t timestamp);
  // void EncodeFramYUVs(void *y, void *uv, int64_t timestamp);

  std::shared_ptr<RtspParam> rtsp_param_ = nullptr;

  std::mutex canvas_lock_;
  cv::Mat canvas_;
  uint8_t *canvas_data_;

  std::thread *refresh_thread_ = nullptr;
  bool running_ = false;
  bool is_mosaic_style_ = false;
  int mosaic_win_width_;
  int mosaic_win_height_;
  StreamPipeCtx *ctx_ = nullptr;
};

}  // namespace cnstream

#endif  // MODULES_RTSP_SINK_INCLUDE_RTSP_SINK_STREAM_HPP_
