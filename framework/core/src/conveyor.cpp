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

#include "conveyor.hpp"

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "connector.hpp"

namespace cnstream {

Conveyor::Conveyor(size_t max_size) : max_size_(max_size) {
}

uint32_t Conveyor::GetBufferSize() {
  std::unique_lock<std::mutex> lk(data_mutex_);
  return dataq_.size();
}

bool Conveyor::PushDataBuffer(CNFrameInfoPtr data) {
  std::unique_lock<std::mutex> lk(data_mutex_);
  if (dataq_.size() < max_size_) {
    dataq_.push(data);
    notempty_cond_.notify_one();
    fail_time_ = 0;
    return true;
  }
  fail_time_ += 1;
  return false;
}

uint64_t Conveyor::GetFailTime() {
  std::unique_lock<std::mutex> lk(data_mutex_);
  return fail_time_;
}

CNFrameInfoPtr Conveyor::PopDataBuffer() {
  std::unique_lock<std::mutex> lk(data_mutex_);
  CNFrameInfoPtr data = nullptr;
  if (notempty_cond_.wait_for(lk, rel_time_, [&] { return !dataq_.empty(); })) {
    data = dataq_.front();
    dataq_.pop();
    return data;
  }
  return data;
}

std::vector<CNFrameInfoPtr> Conveyor::PopAllDataBuffer() {
  std::unique_lock<std::mutex> lk(data_mutex_);
  std::vector<CNFrameInfoPtr> vec_data;
  CNFrameInfoPtr data = nullptr;
  while (!dataq_.empty()) {
    data = dataq_.front();
    dataq_.pop();
    vec_data.push_back(data);
  }
  return vec_data;
}

}  // namespace cnstream
