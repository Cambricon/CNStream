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

#ifndef _CNSTREAM_SOURCE_HANDLER_UTIL_HPP_
#define _CNSTREAM_SOURCE_HANDLER_UTIL_HPP_

#include <assert.h>

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <queue>
#include <memory>
#include <iostream>
#include <thread>
#include <string>

#include "cnstream_frame_va.hpp"
#include "cnstream_logging.hpp"
#include "data_source.hpp"
#include "util/video_decoder.hpp"

namespace cnstream {

struct EsPacket {
  explicit EsPacket(ESPacket *pkt) {
    if (pkt && pkt->data && pkt->size) {
      pkt_.data = new(std::nothrow) unsigned char[pkt->size];
      if (pkt_.data) {
        memcpy(pkt_.data, pkt->data, pkt->size);
        pkt_.size = pkt->size;
      } else {
        pkt_.size = 0;
      }
      pkt_.pts = pkt->pts;
      pkt_.flags = pkt->flags;
    } else {
      pkt_.data = nullptr;
      pkt_.size = 0;
      pkt_.flags = ESPacket::FLAG_EOS;
      pkt_.pts = ~0;
    }
  }

  ~EsPacket() {
    if (pkt_.data) {
      delete []pkt_.data, pkt_.data = nullptr;
    }
    pkt_.size = 0;
    pkt_.flags = 0;
    pkt_.pts = 0;
  }

  ESPacket pkt_;
};

template<typename T>
class BoundedQueue {
 public:
  BoundedQueue(const BoundedQueue<T>&) = delete;
  BoundedQueue& operator=(const BoundedQueue<T>&) = delete;

  explicit BoundedQueue<T>(size_t maxSize)
    : mutex_(),
    maxSize_(maxSize)
  {}

  void Push(const T& x) {
    std::unique_lock<std::mutex> lk(mutex_);
    notFull_.wait(lk, [this]() {return queue_.size() < maxSize_; });
    queue_.push(x);
    lk.unlock();
    notEmpty_.notify_one();
  }

  bool Push(int timeoutMs, const T& x) {
    std::unique_lock<std::mutex> lk(mutex_);
    notFull_.wait_for(lk, std::chrono::milliseconds(timeoutMs),  [this]() {return queue_.size() < maxSize_; });
    if (queue_.size() >= maxSize_) {
      return false;
    }
    queue_.push(x);
    lk.unlock();
    notEmpty_.notify_one();
    return true;
  }

  T Pop() {
    std::unique_lock<std::mutex> lk(mutex_);
    notEmpty_.wait(lk, [this]() {return !queue_.empty(); });
    T front(queue_.front());
    queue_.pop();
    lk.unlock();
    notFull_.notify_one();
    return front;
  }

  bool Pop(int timeoutMs, T& out) {  // NOLINT
    std::unique_lock<std::mutex> lk(mutex_);
    notEmpty_.wait_for(lk, std::chrono::milliseconds(timeoutMs), [this]() {return !queue_.empty(); });
    if (queue_.empty()) return false;
    out = queue_.front();
    queue_.pop();
    lk.unlock();
    notFull_.notify_one();
    return true;
  }

  bool Empty() const {
    std::unique_lock<std::mutex> lk(mutex_);
    return queue_.empty();
  }

  size_t Size() const {
    std::unique_lock<std::mutex> lk(mutex_);
    return queue_.size();
  }

  size_t MaxSize() const {
    return maxSize_;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable notEmpty_;
  std::condition_variable notFull_;
  size_t maxSize_;
  std::queue<T> queue_;
};
using FrameQueue = BoundedQueue<std::shared_ptr<EsPacket>>;

class SourceRender {
 public:
  explicit SourceRender(SourceHandler *handler) : handler_(handler) {}
  virtual ~SourceRender() = default;

  virtual bool CreateInterrupt() { return interrupt_.load(); }
  std::shared_ptr<CNFrameInfo> CreateFrameInfo(bool eos = false) {
    std::shared_ptr<CNFrameInfo> data;
    while (1) {
      data = handler_->CreateFrameInfo(eos);
      if (data != nullptr) break;
      if (CreateInterrupt()) break;
      std::this_thread::sleep_for(std::chrono::microseconds(5));
    }
    auto dataframe = std::make_shared<CNDataFrame>();
    if (!dataframe) {
      return nullptr;
    }
    auto inferobjs = std::make_shared<CNInferObjs>();
    if (!inferobjs) {
      return nullptr;
    }
    auto inferdata =  std::make_shared<CNInferData>();
    if (!inferdata) {
      return nullptr;
    }
    data->datas[CNDataFramePtrKey] = dataframe;
    data->datas[CNInferObjsPtrKey] = inferobjs;
    data->datas[CNInferDataPtrKey] = inferdata;
    return data;
  }

  void SendFlowEos() {
    if (eos_sent_) return;
    auto data = CreateFrameInfo(true);
    if (!data) {
      LOGE("SOURCE") << "SendFlowEos: Create CNFrameInfo failed";
      return;
    }
    SendFrameInfo(data);
    eos_sent_ = true;
  }

  bool SendFrameInfo(std::shared_ptr<CNFrameInfo> data) {
    return handler_->SendData(data);
  }

  //
  void SetPerfManager(std::shared_ptr<PerfManager> perf_manager) {
    perf_manager_ = perf_manager;
  }
  void SetThreadName(std::string module_name, uint64_t stream_idx) {
    thread_name_ = "cn-" + module_name + "-" + NumToFormatStr(stream_idx, 2);
  }
  void RecordStartTime(const std::string &module_name, int64_t pts) {
    if (perf_manager_ != nullptr) {
      perf_manager_->Record(false, PerfManager::GetDefaultType(), module_name, pts);
      perf_manager_->Record(PerfManager::GetDefaultType(), PerfManager::GetPrimaryKey(), std::to_string(pts),
                            module_name + PerfManager::GetThreadSuffix(), "'" + thread_name_ + "'");
    }
  }

 protected:
  SourceHandler *handler_;
  bool eos_sent_ = false;
  std::shared_ptr<PerfManager> perf_manager_;
  std::string thread_name_;

 protected:
  std::atomic<bool> interrupt_{false};
  uint64_t frame_count_ = 0;
  uint64_t frame_id_ = 0;

 public:
  static int Process(std::shared_ptr<CNFrameInfo> frame_info,
                     DecodeFrame *frame, uint64_t frame_id, const DataSourceParam &param_);
};

}  // namespace cnstream

#endif  // _CNSTREAM_SOURCE_HANDLER_UTIL_HPP_
