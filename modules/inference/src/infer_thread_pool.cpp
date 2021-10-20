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

#include <cassert>
#include <string>
#include <vector>

#include "cnstream_logging.hpp"
#include "device/mlu_context.h"
#include "exception.hpp"

namespace cnstream {

void InferThreadPool::Init(int dev_id, size_t thread_num) {
  std::unique_lock<std::mutex> lk(mtx_);
  dev_id_ = dev_id;
  running_ = true;
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

  lk.lock();
  threads_.clear();
  while (!task_q_.empty()) {
    task_q_.pop();
  }
}

void InferThreadPool::SubmitTask(const InferTaskSptr& task) {
  if (!task.get()) return;
  std::unique_lock<std::mutex> lk(mtx_);

  q_push_cond_.wait(lk, [this]() -> bool { return task_q_.size() < max_tnum_ || !running_; });
  assert(task_q_.size() < max_tnum_);

  if (!running_) return;

  task_q_.push(task);

  lk.unlock();
  q_pop_cond_.notify_one();
}

void InferThreadPool::SubmitTask(const std::vector<InferTaskSptr>& tasks) {
  for (auto it : tasks) SubmitTask(it);
}

InferTaskSptr InferThreadPool::PopTask() {
  std::unique_lock<std::mutex> lk(mtx_);
  assert(task_q_.size() <= max_tnum_);

  q_pop_cond_.wait(lk, [this]() -> bool { return task_q_.size() > 0 || !running_; });

  if (!running_) return NULL;

  auto task = task_q_.front();
  task_q_.pop();

  lk.unlock();
  q_push_cond_.notify_one();
  return task;
}

void InferThreadPool::SetErrorHandleFunc(const std::function<void(const std::string& err_msg)>& err_func) {
  std::lock_guard<std::mutex> lk(mtx_);
  error_func_ = err_func;
}

void InferThreadPool::TaskLoop() {
  edk::MluContext context;
  context.SetDeviceId(dev_id_);
  context.BindDevice();
  while (running_) {
    InferTaskSptr task = PopTask();
    if (task.get() == nullptr) {
      assert(!running_);
      return;
    }

    task->WaitForFrontTasksComplete();

    int ret = 0;
    try {
      ret = task->Execute();
    } catch (CnstreamError& e) {
      if (error_func_) {
        error_func_(e.what());
      } else {
        LOGF(INFERENCER) << "Not handled error: " << std::string(e.what());
      }
    }

    if (ret != 0) {
      LOGI(INFERENCER) << "Inference task execute failed. Error code [" << ret << "]. Task message: " << task->task_msg;
    }
  }
}

}  // namespace cnstream
