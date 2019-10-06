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

#include "display_stream.hpp"

#include <glog/logging.h>

namespace cnstream {

bool DisplayStream::Open(int window_w, int window_h, int cols, int rows, float display_rate) {
  if (window_w < 1 || window_h < 1 || cols < 1 || rows < 1 || display_rate < 1) {
    return false;
  }
  window_w_ = window_w;
  window_h_ = window_h;
  cols_ = cols;
  rows_ = rows;
  unit_w_ = window_w_ / cols;
  unit_h_ = window_h_ / rows;
  refresh_rate_ = display_rate;

  canvas_ = cv::Mat(window_h_, window_w_, CV_8UC3, cv::Scalar(0, 0, 0));
  running_ = true;
  refresh_thread_ = new std::thread(&DisplayStream::RefreshLoop, this);
  return true;
}

void DisplayStream::Close() {
  running_ = false;
  if (refresh_thread_->joinable()) {
    refresh_thread_->join();
  }
  delete refresh_thread_;
  refresh_thread_ = nullptr;
  canvas_.release();
}

bool DisplayStream::Update(cv::Mat image, int channel_id) {
  if (channel_id > cols_ * rows_ - 1) {
    LOG(ERROR) << "channel id should be less than cols * rows - 1";
    return false;
  }
  int x = channel_id % cols_ * unit_w_;
  int y = channel_id / cols_ * unit_h_;
  cv::resize(image, image, cv::Size(unit_w_, unit_h_));
  image.copyTo(canvas_(cv::Rect(x, y, unit_w_, unit_h_)));
  return true;
}

void DisplayStream::RefreshLoop() {
  int delay_ms = 1000 / refresh_rate_ - 1;
  auto start = std::chrono::high_resolution_clock::now();
  while (running_) {
    auto cycle = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> dura = cycle - start;
    start = cycle;
    int rt = delay_ms - dura.count();
    if (rt > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(rt));
    }
    imshow("CNStream", canvas_);
    cv::waitKey(1);
  }
}

}  // namespace cnstream
