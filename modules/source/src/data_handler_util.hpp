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
#include <mutex>
#include <queue>
#include <memory>
#include <iostream>
#include <thread>
#include <chrono>

#include "ffmpeg_parser.hpp"
#include "data_source.hpp"

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
      pkt_.pts = 0;
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
    notEmpty_.notify_one();
  }

  bool Push(int timeoutMs, const T& x) {
    std::unique_lock<std::mutex> lk(mutex_);
    notFull_.wait_for(lk, std::chrono::milliseconds(timeoutMs),  [this]() {return queue_.size() < maxSize_; });
    if (queue_.size() >= maxSize_) {
      return false;
    }
    queue_.push(x);
    notEmpty_.notify_one();
    return true;
  }

  T Pop() {
    std::unique_lock<std::mutex> lk(mutex_);
    notEmpty_.wait(lk, [this]() {return !queue_.empty(); });
    T front(queue_.front());
    queue_.pop();
    notFull_.notify_one();
    return front;
  }

  bool Pop(int timeoutMs, T& out) {  // NOLINT
    std::unique_lock<std::mutex> lk(mutex_);
    notEmpty_.wait_for(lk, std::chrono::milliseconds(timeoutMs), [this]() {return !queue_.empty(); });
    if (queue_.empty()) return false;
    out = queue_.front();
    queue_.pop();
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
class IDemuxer {
 public:
  virtual ~IDemuxer() {}
  virtual bool PrepareResources(std::atomic<int> &exit_flag) = 0;  // NOLINT
  virtual void ClearResources(std::atomic<int> &exit_flag) = 0;   // NOLINT
  virtual bool Process() = 0;  // process one frame
  bool GetInfo(VideoStreamInfo &info) {  // NOLINT
    std::unique_lock<std::mutex> lk(mutex_);
    if (info_set_) {
      info = info_;
      return true;
    }
    return false;
  }
 protected:
  void SetInfo(VideoStreamInfo &info) {  // NOLINT
    std::unique_lock<std::mutex> lk(mutex_);
    info_ = info;
    info_set_ = true;
  }

  std::mutex mutex_;
  VideoStreamInfo info_;
  bool info_set_ = false;
};

}  // namespace cnstream

#endif  // _CNSTREAM_SOURCE_HANDLER_UTIL_HPP_
