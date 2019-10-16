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

#include "infer_thread_pool.hpp"

#include <glog/logging.h>
#include <cassert>
#include <string>

namespace cnstream {

void InferThreadPool::Init(size_t thread_num) {
  max_tnum_ = 2 * thread_num;
  for (size_t ti = 0; ti < thread_num; ++ti) {
    threads_.push_back(std::thread(&InferThreadPool::TaskLoop, this));
  }
}

void InferThreadPool::Destroy() {
  std::unique_lock<std::mutex> lk(mtx_);
  running_ = false;
  lk.unlock();
  q_push_cond_.notify_all();
  q_pop_cond_.notify_all();

  for (auto& it : threads_) {
    if (it.joinable()) it.join();
  }
}

void InferThreadPool::SubmitTask(const InferTaskSptr& task) {
  std::unique_lock<std::mutex> lk(mtx_);

  q_push_cond_.wait(lk, [this]() -> bool { return task_q_.size() < max_tnum_ || !running_; });
  assert(task_q_.size() < max_tnum_);

  if (!running_) return;

  task_q_.push(task);
  q_pop_cond_.notify_one();
}

InferTaskSptr InferThreadPool::PopTask() {
  std::unique_lock<std::mutex> lk(mtx_);
  assert(task_q_.size() <= max_tnum_);

  q_pop_cond_.wait(lk, [this]() -> bool { return task_q_.size() > 0 || !running_; });

  if (!running_) return NULL;

  auto task = task_q_.front();
  task_q_.pop();
  q_push_cond_.notify_one();
  return task;
}

void InferThreadPool::TaskLoop() {
  while (running_) {
    InferTaskSptr task = PopTask();
    if (task.get() == nullptr) {
      assert(!running_);
      return;
    }

    try {
      task->WaitForFrontTasksComplete();
    } catch (std::future_error &e) {
      LOG(INFO) << e.what();
    }

    int ret = 0;
    ret = task->Execute();

    if (ret != 0) {
      DLOG(INFO) << "Inference task execute failed. Error code [" << ret << "]. Task message: " << task->task_msg;
    }
  }
}

}  // namespace cnstream
