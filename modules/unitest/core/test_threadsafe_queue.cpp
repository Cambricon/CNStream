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

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "glog/logging.h"
#include "gtest/gtest.h"

#include "threadsafe_queue.hpp"

namespace cnstream {

std::mutex data_mutex_;
bool flag_[100];

void ThreadFuncPush(ThreadSafeQueue<int>* thread_safe_queue, int data) {
  std::lock_guard<std::mutex> lk(data_mutex_);
  thread_safe_queue->Push(data);
  flag_[data] = true;
  // lock_guard<mutex> lk(data_m);
  // cout << "--Test Push:" << data++ << endl;
}

void ThreadFuncTryPop(ThreadSafeQueue<int>* thread_safe_queue) {
  int value = -1;
  bool res = thread_safe_queue->TryPop(value);
  std::lock_guard<std::mutex> lk(data_mutex_);
  if (res) {
    LOG_IF(FATAL, !flag_[value]) << "Test pop data repeatedly:try_pop error! ";
    flag_[value] = false;
  }
}

void ThreadFuncWaitAndPop(ThreadSafeQueue<int>* thread_safe_queue) {
  int value = -1;
  thread_safe_queue->WaitAndPop(value);
  std::lock_guard<std::mutex> lk(data_mutex_);
  LOG_IF(FATAL, !flag_[value]) << "Test pop data repeatedly: wait_and_pop error!";
  flag_[value] = false;
}

void ThreadFuncWaitAndTryPop(ThreadSafeQueue<int>* thread_safe_queue) {
  int value = -1;
  bool res = thread_safe_queue->WaitAndTryPop(value, std::chrono::microseconds(50));
  std::lock_guard<std::mutex> lk(data_mutex_);
  if (res) {
    LOG_IF(FATAL, !flag_[value]) << "Test pop data repeatedly: wait_and_try_pop error!";
    flag_[value] = false;
  }
}

bool TestThreadsafeQueue() {
  ThreadSafeQueue<int> thread_safe_queue;
  memset(flag_, 0, sizeof(flag_));
  std::thread* threads[100];

  int data[100];
  for (int i = 0; i < 100; i++) {
    data[i] = i;
  }
  int i = -1;

  uint32_t seed = (uint32_t)time(0);
  srand(time(nullptr));

  LOG(INFO) << "Test threadsafe_queue: push and pop!";
  while (++i < 40) {
    if (i > 20) {
      threads[i] = new std::thread(ThreadFuncPush, &thread_safe_queue, data[i]);
    } else {
      switch (rand_r(&seed) % 4) {
        case 0:
          threads[i] = new std::thread(ThreadFuncTryPop, &thread_safe_queue);
          break;
        case 1:
          threads[i] = new std::thread(ThreadFuncWaitAndPop, &thread_safe_queue);
          break;
        case 2:
          threads[i] = new std::thread(ThreadFuncWaitAndTryPop, &thread_safe_queue);
          break;
        case 3:
          threads[i] = new std::thread(ThreadFuncPush, &thread_safe_queue, data[i]);
          break;
        default:
          break;
      }
    }
  }
  for (int k = 0; k < 40; ++k) {
    threads[k]->join();
  }

  LOG(INFO) << "Test threadsafe_queue: blocking";
  i--;
  while (++i < 70) {
    if (i < 55) {
      switch (rand_r(&seed) % 2) {
        case 0:
          threads[i] = new std::thread(ThreadFuncWaitAndPop, &thread_safe_queue);
          break;
        case 1:
          threads[i] = new std::thread(ThreadFuncWaitAndTryPop, &thread_safe_queue);
          break;
        default:
          break;
      }
    } else {
      threads[i] = new std::thread(ThreadFuncPush, &thread_safe_queue, data[i]);
    }
  }
  for (int k = 40; k < 70; ++k) {
    threads[k]->join();
  }
  for (int k = 0; k < 70; ++k) {
    delete threads[k];
  }
  return true;
}

TEST(CoreThreadSafeQueue, ThreadsafeQueue) { EXPECT_EQ(true, TestThreadsafeQueue()); }

}  // namespace cnstream
