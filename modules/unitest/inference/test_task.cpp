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

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include "infer_task.hpp"

namespace cnstream {

TEST(Inferencer, InferTask_Constructor) {
  InferTaskSptr task;
  ASSERT_NO_THROW(task = std::make_shared<InferTask>([]() -> int { return 0; }));
}

TEST(Inferencer, InferTask_BindFrontTask) {
  InferTask task([]() -> int { return 0; });
  InferTaskSptr front_task = std::make_shared<InferTask>([]() -> int { return 0; });
  ASSERT_NO_THROW(task.BindFrontTask(front_task));
}

TEST(Inferencer, InferTask_BindFrontTask_NULL) {
  InferTask task([]() -> int { return 0; });
  InferTaskSptr front_task = NULL;
  ASSERT_NO_THROW(task.BindFrontTask(front_task));
}

TEST(Inferencer, InferTask_BindFrontTasks) {
  InferTask task([]() -> int { return 0; });
  InferTaskSptr front_task = std::make_shared<InferTask>([]() -> int { return 0; });
  InferTaskSptr front_task2 = std::make_shared<InferTask>([]() -> int { return 0; });
  std::vector<InferTaskSptr> front_tasks = {front_task, front_task2};
  ASSERT_NO_THROW(task.BindFrontTasks(front_tasks));
}

TEST(Inferencer, InferTask_Execute) {
  InferTask task([]() -> int { return 1000; });
  EXPECT_EQ(1000, task.Execute());
}

TEST(Inferencer, InferTask_WaitForTaskComplete) {
  InferTask task([]() -> int { return 0; });
  task.Execute();
  ASSERT_NO_THROW(task.WaitForTaskComplete());
}

TEST(Inferencer, InferTask_WaitForFrontTasksComplete) {
  InferTask task([]() -> int { return 0; });
  InferTaskSptr front_task = std::make_shared<InferTask>([]() -> int { return 0; });
  InferTaskSptr front_task2 = std::make_shared<InferTask>([]() -> int { return 0; });
  std::vector<InferTaskSptr> front_tasks = {front_task, front_task2};
  task.BindFrontTasks(front_tasks);
  front_task->Execute();
  front_task2->Execute();
  ASSERT_NO_THROW(task.WaitForFrontTasksComplete());
}

TEST(Inferencer, InferTask_RemoveResourceAfterExecute) {
  std::shared_ptr<int> resource = std::make_shared<int>(1);
  InferTask task([=]() -> int { return *resource; });
  EXPECT_EQ(resource.use_count(), 2l);
  task.Execute();
  EXPECT_EQ(resource.use_count(), 1l);
}

TEST(Inferencer, InferTask_ExecuteSequence) {
  std::chrono::steady_clock::time_point task0_tp, task1_tp, task2_tp;
  InferTaskSptr task0 = std::make_shared<InferTask>([&task0_tp]() -> int {
    task0_tp = std::chrono::steady_clock::now();
    return 0;
  });
  InferTaskSptr task1 = std::make_shared<InferTask>([&task1_tp]() -> int {
    task1_tp = std::chrono::steady_clock::now();
    return 0;
  });
  InferTaskSptr task2 = std::make_shared<InferTask>([&task2_tp]() -> int {
    task2_tp = std::chrono::steady_clock::now();
    return 0;
  });

  task1->BindFrontTask(task0);
  task2->BindFrontTask(task1);

  std::future<void> future2 = std::async(std::launch::async, [&task2]() {
    task2->WaitForFrontTasksComplete();
    task2->Execute();
  });

  std::future<void> future1 = std::async(std::launch::async, [&task1]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    task1->WaitForFrontTasksComplete();
    task1->Execute();
  });

  std::future<void> future0 = std::async(std::launch::async, [&task0]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    task0->WaitForFrontTasksComplete();
    task0->Execute();
  });

  future2.get();
  future1.get();
  future0.get();

  EXPECT_GT(task2_tp.time_since_epoch().count(), task1_tp.time_since_epoch().count());
  EXPECT_GT(task1_tp.time_since_epoch().count(), task0_tp.time_since_epoch().count());
}

}  // namespace cnstream
