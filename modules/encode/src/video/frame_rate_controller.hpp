/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#include <chrono>
#include <thread>

namespace cnstream {

class FrameRateController {
 public:
  explicit FrameRateController(double frame_rate = 0) : frame_rate_(frame_rate) {}

  void Start() { start_ = std::chrono::steady_clock::now(); }

  void Control() {
    if (frame_rate_ <= 0) return;
    double delay = 1000000.0 / frame_rate_;
    end_ = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::micro> diff = end_ - start_;
    auto gap = delay - diff.count() - time_gap_;
    if (gap > 0) {
      std::chrono::duration<double, std::micro> dura(gap);
      std::this_thread::sleep_for(dura);
      time_gap_ = 0;
    } else {
      time_gap_ = -gap;
    }
    start_ = std::chrono::steady_clock::now();
  }

  double GetFrameRate() const { return frame_rate_; }
  void SetFrameRate(double frame_rate) { frame_rate_ = frame_rate; }

 private:
  FrameRateController(const FrameRateController &) = delete;
  FrameRateController &operator=(const FrameRateController &) = delete;
  FrameRateController(const FrameRateController &&) = delete;
  FrameRateController &operator=(const FrameRateController &&) = delete;

  double frame_rate_ = 0;
  double time_gap_ = 0;
  std::chrono::time_point<std::chrono::steady_clock> start_, end_;
};  // class FrameRateController

}  // namespace cnstream
