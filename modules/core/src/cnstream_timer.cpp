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

#include "cnstream_timer.hpp"

#include <iostream>
#include <string>

#include "glog/logging.h"

namespace cnstream {

void CNTimer::Dot(uint32_t cnt_step) {
  if (0 == cnt_step) {
    LOG(WARNING) << "fps calculator count step is zero. Skip!";
    return;
  }

  if (first_dot_) {
    // first dot
    last_t_ = std::chrono::high_resolution_clock::now();
    first_dot_ = false;
  } else {
    auto now_t = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> diff = now_t - last_t_;
    last_t_ = now_t;
    avg_ = avg_ * cnt_ + diff.count();
    cnt_ += cnt_step;
    avg_ /= cnt_;
  }
}

void CNTimer::Dot(double time, uint32_t cnt_step) {
  if (0 == cnt_step) {
    LOG(WARNING) << "fps calculator count step is zero. Skip!";
    return;
  }
  if (time < 0) {
    LOG(WARNING) << "fps calculator time is negtive. Skip!";
    return;
  }
  avg_ = avg_ * cnt_ + time;
  cnt_ += cnt_step;
  avg_ /= cnt_;
}

void CNTimer::PrintFps(const std::string& head) const {
  double fps = avg_ != 0 ? 1e3 / avg_ : 0.0f;
  std::cout << head << "avg : " << avg_ << "ms"
            << " fps : " << fps << " frame count : " << cnt_ << std::endl;
}

void CNTimer::Clear() {
  avg_ = 0;
  cnt_ = 0;
  first_dot_ = true;
}

void CNTimer::MixUp(const CNTimer& other) {
  uint64_t n = cnt_ + other.cnt_;
  if (n > 0) {
    // calculate cnt_ / n first to prevent data overflow
    avg_ = avg_ * (1.0f * cnt_ / n) + other.avg_ * (1.0f * other.cnt_ / n);
  }
  cnt_ = n;
}

double CNTimer::GetAvg() const { return avg_; }

}  // namespace cnstream
