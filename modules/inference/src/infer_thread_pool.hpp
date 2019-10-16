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

#ifndef MODULES_INFERENCE_SRC_INFER_THREAD_POOL_HPP_
#define MODULES_INFERENCE_SRC_INFER_THREAD_POOL_HPP_

#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "infer_task.hpp"

namespace cnstream {

class InferThreadPool {
 public:
  InferThreadPool() {}

  ~InferThreadPool() {}

  void Init(size_t thread_num);

  void Destroy();

  void SubmitTask(const InferTaskSptr& task);

 private:
  InferTaskSptr PopTask();

  void TaskLoop();
  std::vector<std::thread> threads_;
  std::queue<InferTaskSptr> task_q_;
  size_t max_tnum_ = 20;
  std::mutex mtx_;
  std::condition_variable q_push_cond_;
  std::condition_variable q_pop_cond_;
  volatile bool running_ = true;
};  // class InferThreadPool

}  // namespace cnstream

#endif  // MODULES_INFERENCE_SRC_INFER_THREAD_POOL_HPP_
