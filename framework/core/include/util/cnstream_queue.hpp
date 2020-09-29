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

#ifndef CNSTREAM_THREADSAFE_QUEUE_HPP_
#define CNSTREAM_THREADSAFE_QUEUE_HPP_

#include <condition_variable>
#include <mutex>
#include <queue>

namespace cnstream {

template <typename T>
class ThreadSafeQueue {
 public:
  ThreadSafeQueue() = default;
  ThreadSafeQueue(const ThreadSafeQueue& other) = delete;
  ThreadSafeQueue& operator=(const ThreadSafeQueue& other) = delete;

  bool TryPop(T& value);

  void WaitAndPop(T& value);

  bool WaitAndTryPop(T& value, const std::chrono::microseconds rel_time);

  void Push(const T& new_value);

  bool Empty() {
    std::lock_guard<std::mutex> lk(data_m_);
    return q_.empty();
  }

  uint32_t Size() {
    std::lock_guard<std::mutex> lk(data_m_);
    return q_.size();
  }

 private:
  std::mutex data_m_;
  std::queue<T> q_;
  std::condition_variable notempty_cond_;
};

template <typename T>
bool ThreadSafeQueue<T>::TryPop(T& value) {
  std::lock_guard<std::mutex> lk(data_m_);
  if (q_.empty()) {
    return false;
  } else {
    value = q_.front();
    q_.pop();
    return true;
  }
}

template <typename T>
void ThreadSafeQueue<T>::WaitAndPop(T& value) {
  std::unique_lock<std::mutex> lk(data_m_);
  notempty_cond_.wait(lk, [&] { return !q_.empty(); });
  value = q_.front();
  q_.pop();
}

template <typename T>
bool ThreadSafeQueue<T>::WaitAndTryPop(T& value, const std::chrono::microseconds rel_time) {
  std::unique_lock<std::mutex> lk(data_m_);
  if (notempty_cond_.wait_for(lk, rel_time, [&] { return !q_.empty(); })) {
    value = q_.front();
    q_.pop();
    return true;
  } else {
    return false;
  }
}

template <typename T>
void ThreadSafeQueue<T>::Push(const T& new_value) {
  std::unique_lock<std::mutex> lk(data_m_);
  q_.push(new_value);
  lk.unlock();
  notempty_cond_.notify_one();
}

}  // namespace cnstream

#endif  // CNSTREAM_THREADSAFE_QUEUE_HPP_
