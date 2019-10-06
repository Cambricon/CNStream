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

#include "data_handler.hpp"
#include "fr_controller.hpp"
#include "glog/logging.h"

namespace cnstream {

std::mutex DataHandler::index_mutex_;
uint64_t DataHandler::index_mask_ = 0;

/*maxStreamNumber is sizeof(index_mask_) * 8  (bytes->bits)
 */
size_t DataHandler::GetStreamIndex() {
  std::unique_lock<std::mutex> lock(index_mutex_);
  if (streamIndex_ != INVALID_STREAM_ID) {
    return streamIndex_;
  }
  for (size_t i = 0; i < sizeof(index_mask_) * 8; i++) {
    if (!(index_mask_ & ((uint64_t)1 << i))) {
      index_mask_ |= (uint64_t)1 << i;
      streamIndex_ = i;
      return i;
    }
  }
  return INVALID_STREAM_ID;
}

void DataHandler::ReturnStreamIndex() const {
  std::unique_lock<std::mutex> lock(index_mutex_);
  if (streamIndex_ < 0 || streamIndex_ >= sizeof(index_mask_) * 8) {
    return;
  }
  index_mask_ &= ~((uint64_t)1 << streamIndex_);
}

bool DataHandler::Open() {
  if (!this->module_) {
    return false;
  }

  // default value
  dev_ctx_.dev_type = DevContext::MLU;
  dev_ctx_.dev_id = 0;

  // updated with paramSet
  param_ = module_->GetSourceParam();
  if (param_.output_type_ == OUTPUT_CPU) {
    dev_ctx_.dev_type = DevContext::CPU;
    dev_ctx_.dev_id = -1;
  } else if (param_.output_type_ == OUTPUT_MLU) {
    dev_ctx_.dev_type = DevContext::MLU;
    dev_ctx_.dev_id = param_.device_id_;
  } else {
    return false;
  }

  size_t chn_idx = this->GetStreamIndex();
  if (chn_idx == DataHandler::INVALID_STREAM_ID) {
    return false;
  }
  dev_ctx_.ddr_channel = chn_idx % 4;

  this->interval_ = param_.interval_;

  // start demuxer
  running_.store(1);
  thread_ = std::move(std::thread(&DataHandler::Loop, this));
  return true;
}

void DataHandler::Close() {
  if (running_.load()) {
    running_.store(0);
    if (thread_.joinable()) {
      thread_.join();
    }
  }
}

void DataHandler::Loop() {
  if (!PrepareResources()) {
    return;
  }

  FrController controller(frame_rate_);
  if (frame_rate_ > 0) controller.Start();

  while (running_.load()) {
    if (!this->Process()) {
      break;
    }
    if (frame_rate_ > 0) controller.Control();
  }

  ClearResources();
}

}  // namespace cnstream
