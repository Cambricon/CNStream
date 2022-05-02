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

#ifndef MODULES_INFERENCE_SRC_INFER_TASK_HPP_
#define MODULES_INFERENCE_SRC_INFER_TASK_HPP_

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "exception.hpp"

namespace cnstream {

class InferTask;
using InferTaskSptr = std::shared_ptr<InferTask>;

class InferTask {
 public:
  std::string task_msg = "task";  // for debug.

  explicit InferTask(const std::function<int()>& task_func) : func_(task_func) {
    statem_ = promise_.get_future();
  }

  ~InferTask() {}

  void BindFrontTask(const InferTaskSptr& ftask) {
    if (ftask.get()) pre_task_statem_.push_back(ftask->statem_);
  }

  void BindFrontTasks(const std::vector<InferTaskSptr>& ftasks) {
    for (const auto& task : ftasks) {
      BindFrontTask(task);
    }
  }

  int Execute() {
    int ret = 0;
    try {
      ret = func_();
    } catch (CnstreamError& e) {
      ret = -1;
      func_ = NULL;  // unbind resources.
      promise_.set_value(ret);
      statem_.get();
      throw e;
    }
    func_ = NULL;  // unbind resources.
    promise_.set_value(ret);
    return statem_.get();
  }

  void WaitForTaskComplete() { statem_.wait(); }

  void WaitForFrontTasksComplete() {
    for (const auto& task_statem : pre_task_statem_) {
      task_statem.wait();
    }
  }

 private:
  std::promise<int> promise_;
  std::function<int()> func_;
  std::shared_future<int> statem_;
  std::vector<std::shared_future<int>> pre_task_statem_;
};  // class InferTask

}  // namespace cnstream

#endif  // MODULES_INFERENCE_SRC_INFER_TASK_HPP_
