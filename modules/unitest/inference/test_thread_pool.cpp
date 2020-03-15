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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "infer_thread_pool.hpp"

namespace cnstream {

class InferThreadPoolTest {
 public:
  explicit InferThreadPoolTest(InferThreadPool* tp) : tp_(tp) {}
  InferTaskSptr PopTask() { return tp_->PopTask(); }
  int GetThreadNum() { return static_cast<int>(tp_->threads_.size()); }
  int GetTaskNum() {
    std::unique_lock<std::mutex> lk(tp_->mtx_);
    return static_cast<int>(tp_->task_q_.size());
  }

 private:
  InferThreadPool* tp_;
};

TEST(Inferencer, InferThreadPool_Constructor) {
  std::shared_ptr<InferThreadPool> tp = NULL;
  EXPECT_NO_THROW(tp = std::make_shared<InferThreadPool>());
  InferThreadPoolTest tp_test(tp.get());
  EXPECT_EQ(tp_test.GetThreadNum(), 0);
}

TEST(Inferencer, InferThreadPool_Init) {
  InferThreadPool tp;
  EXPECT_NO_THROW(tp.Init(0, 0));
  InferThreadPoolTest tp_test(&tp);
  EXPECT_EQ(tp_test.GetThreadNum(), 0);
  tp.Destroy();
  EXPECT_NO_THROW(tp.Init(0, 5));
  EXPECT_EQ(tp_test.GetThreadNum(), 5);
  tp.Destroy();
}

TEST(Inferencer, InferThreadPool_Destroy) {
  InferThreadPool tp;
  EXPECT_NO_THROW(tp.Init(0, 1));
  EXPECT_NO_THROW(tp.Destroy());
  InferThreadPoolTest tp_test(&tp);
  EXPECT_EQ(tp_test.GetThreadNum(), 0);
}

TEST(Inferencer, InferThreadPool_SubmitTask) {
  InferTaskSptr task = std::make_shared<InferTask>([]() -> int { return 1; });
  InferThreadPool tp;
  tp.Destroy();
  /* not running, submit task failed */
  EXPECT_NO_THROW(tp.SubmitTask(task));
  InferThreadPoolTest tp_test(&tp);
  EXPECT_EQ(tp_test.GetTaskNum(), 0);
  /* running, submit task success */
  std::condition_variable pause;
  std::mutex mtx;
  std::atomic<bool> task_run(false);
  task = std::make_shared<InferTask>([&]() -> int {
    std::unique_lock<std::mutex> lk(mtx);
    /*
      pause and block the only one thread in threadpool
    */
    task_run.store(true);
    pause.wait(lk);
    return 0;
  });
  auto task2 = std::make_shared<InferTask>([]() -> int { return 0; });
  tp.Init(0, 1);
  EXPECT_NO_THROW(tp.SubmitTask(task));
  while (!task_run.load()) {
    // wait for the first task is running
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  EXPECT_NO_THROW(tp.SubmitTask(task2));
  EXPECT_EQ(tp_test.GetTaskNum(), 1);
  pause.notify_one();
  tp.Destroy();
}

TEST(Inferencer, InferThreadPool_PopTask) {
  std::condition_variable pause;
  std::mutex mtx;
  std::atomic<bool> task_run(false);
  InferTaskSptr task = std::make_shared<InferTask>([&]() -> int {
    task_run.store(true);
    std::unique_lock<std::mutex> lk(mtx);
    /*
      pause and block the only one thread in threadpool
    */
    pause.wait(lk);
    return 1;
  });
  InferThreadPool tp;
  tp.Init(0, 1);
  tp.SubmitTask(task);
  InferTaskSptr task_for_pop = std::make_shared<InferTask>([&]() -> int { return 1; });
  task_for_pop->task_msg = "test_pop";
  tp.SubmitTask(task_for_pop);
  while (!task_run.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  InferThreadPoolTest tp_test(&tp);
  auto task_popped = tp_test.PopTask();
  EXPECT_EQ(task_popped->task_msg, "test_pop");
  pause.notify_one();
  tp.Destroy();
}

TEST(Inferencer, InferThreadPool_TaskSequence) {
  constexpr int ktask_num = 5;
  InferThreadPool tp;
  tp.Init(0, ktask_num);
  std::chrono::steady_clock::time_point ts[ktask_num];  // NOLINT
  InferTaskSptr tasks[ktask_num];                       // NOLINT
  std::function<int(std::chrono::steady_clock::time_point * t)> func =
      [](std::chrono::steady_clock::time_point* t) -> int {
    *t = std::chrono::steady_clock::now();
    return 0;
  };
  for (int i = 0; i < ktask_num; ++i) {
    tasks[i] = std::make_shared<InferTask>(std::bind(func, ts + i));
    if (i != 0) {
      tasks[i]->BindFrontTask(tasks[i - 1]);
    }
  }

  for (int i = ktask_num - 1; i >= 0; --i) {
    tp.SubmitTask(tasks[i]);
  }

  for (auto& task : tasks) {
    task->WaitForTaskComplete();
  }

  for (int i = 1; i < ktask_num; ++i) {
    EXPECT_GT(ts[i].time_since_epoch().count(), ts[i - 1].time_since_epoch().count());
  }

  tp.Destroy();
}

}  // namespace cnstream
