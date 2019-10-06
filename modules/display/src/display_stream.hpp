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

#ifndef DISPLAY_STREAM_HPP_
#define DISPLAY_STREAM_HPP_

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#else
#error OpenCV required
#endif
#include <chrono>
#include <thread>

namespace cnstream {

class DisplayStream {
 public:
  bool Open(int window_w, int window_h, int cols, int rows, float display_rate);
  void Close();
  bool Update(cv::Mat image, int channel_id);

 private:
  void RefreshLoop();
  cv::Mat canvas_;
  std::thread *refresh_thread_;
  int window_h_;
  int window_w_;
  int cols_;
  int rows_;
  int unit_w_;
  int unit_h_;
  float refresh_rate_;
  bool running_ = false;
};

}  // namespace cnstream

#endif  // DISPLAY_STREAM_HPP_
