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

#ifndef __CNSTYPE__H__
#define __CNSTYPE__H__

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#else
#error OpenCV required
#endif

typedef struct {
  bool eos_flag = 0;
  uint64_t frame_id = 0;
  uint32_t width = 0;
  uint32_t height = 0;
} CNSFrameInfo;

typedef struct {
  bool loop = false;
  bool register_data = 0;
  int fps = 0;
  int cache_size = 0;
  int dst_width = 0;
  int dst_height = 0;
} CNServiceInfo;

typedef struct {
  CNSFrameInfo frame_info;
  cv::Mat* bgr_mat = nullptr;
} CNSFrame;

template <typename T>
class CNSQueue {
 public:
  CNSQueue(const CNSQueue<T>&) = delete;
  CNSQueue& operator=(const CNSQueue<T>&) = delete;

  explicit CNSQueue<T>(size_t maxSize) : mutex_(), maxSize_(maxSize) {}

  void Push(const T& x) {
    std::unique_lock<std::mutex> lk(mutex_);
    notFull_.wait(lk, [this]() { return queue_.size() < maxSize_; });
    queue_.push(x);
    notEmpty_.notify_one();
  }

  bool Push(int timeoutMs, const T& x) {
    std::unique_lock<std::mutex> lk(mutex_);
    notFull_.wait_for(lk, std::chrono::milliseconds(timeoutMs), [this]() { return queue_.size() < maxSize_; });
    if (queue_.size() >= maxSize_) {
      return false;
    }
    queue_.push(x);
    notEmpty_.notify_one();
    return true;
  }

  T Pop() {
    std::unique_lock<std::mutex> lk(mutex_);
    notEmpty_.wait(lk, [this]() { return !queue_.empty(); });
    T front(queue_.front());
    queue_.pop();
    notFull_.notify_one();
    return front;
  }

  bool Pop(int timeoutMs, T& out) {  // NOLINT
    std::unique_lock<std::mutex> lk(mutex_);
    notEmpty_.wait_for(lk, std::chrono::milliseconds(timeoutMs), [this]() { return !queue_.empty(); });
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

  bool Full() const {
    std::unique_lock<std::mutex> lk(mutex_);
    return queue_.size() == maxSize_;
  }

  size_t Size() const {
    std::unique_lock<std::mutex> lk(mutex_);
    return queue_.size();
  }

  size_t MaxSize() const { return maxSize_; }

 private:
  mutable std::mutex mutex_;
  std::condition_variable notEmpty_;
  std::condition_variable notFull_;
  size_t maxSize_;
  std::queue<T> queue_;
};

#endif
