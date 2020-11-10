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

#include "infer_trans_data_helper.hpp"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include "inferencer.hpp"

namespace cnstream {

InferTransDataHelper::InferTransDataHelper(Inferencer* infer, int batchsize) : infer_(infer), batchsize_(batchsize) {
  running_.store(true);
  th_ = std::thread(&InferTransDataHelper::Loop, this);
}

InferTransDataHelper::~InferTransDataHelper() {
  running_.store(false);
  {
    cond_not_empty_.notify_all();
    cond_not_full_.notify_all();
  }
  if (th_.joinable()) th_.join();
}

void InferTransDataHelper::SubmitData(
    const std::pair<std::shared_ptr<CNFrameInfo>, InferEngine::ResultWaitingCard>& data) {
  std::unique_lock<std::mutex> lk(mtx_);
  cond_not_full_.wait(lk, [this] () { return !running_.load() || queue_.size() < size_t(3 * batchsize_); });
  if (!running_.load()) return;
  queue_.push(data);
  lk.unlock();
  cond_not_empty_.notify_one();
}

void InferTransDataHelper::Loop() {
  while (running_.load()) {
    std::unique_lock<std::mutex> lk(mtx_);
    cond_not_empty_.wait(lk, [this]() { return !running_.load() || !queue_.empty(); });
    if (!running_.load()) break;
    auto data = queue_.front();
    queue_.pop();
    lk.unlock();
    cond_not_full_.notify_one();

    if (cnstream::IsStreamRemoved(data.first->stream_id)) {
      if (!data.first->IsEos()) {
        // discard packet if stream has been removed
        continue;
      }
    }

    auto finfo = data.first;
    auto card = data.second;
    card.WaitForCall();

    if (infer_) {
      infer_->TransmitData(finfo);
    }
  }
}

}  // namespace cnstream
