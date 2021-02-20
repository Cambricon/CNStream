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
#include <future>
#include <memory>
#include <mutex>
#include <thread>

#include "timeout_helper.hpp"

namespace cnstream {

class TimeoutHelperTest {
 public:
  explicit TimeoutHelperTest(TimeoutHelper* th) : th_(th) {}
  std::thread& getThread() { return th_->handle_th_; }
  float getTime() { return th_->timeout_; }
  void setTime(float time) { th_->timeout_ = time; }
  TimeoutHelper::State getState() { return th_->state_; }
  void setState(int number) { th_->state_ = TimeoutHelper::State(number); }
  // condition_variable do not support move or reference
  void ConditionNotify() { th_->cond_.notify_one(); }
  int get_timeout_print_cnt() { return static_cast<int>(th_->timeout_print_cnt_); }
  void set_timeout_print_cnt(int number) { th_->timeout_print_cnt_ = number; }

 private:
  TimeoutHelper* th_;
};

TEST(Inferencer, TimeoutHelper_Constructor) {
  std::shared_ptr<TimeoutHelper> th = nullptr;
  EXPECT_NO_THROW(th = std::make_shared<TimeoutHelper>());  // 此时创建handle_th_线程
  TimeoutHelperTest th_test(th.get());
  std::thread& Threadhandle = th_test.getThread();
  EXPECT_EQ(Threadhandle.joinable(), true);
}

TEST(Inferencer, TimeoutHelper_SetTimeout) {
  std::shared_ptr<TimeoutHelper> th = std::make_shared<TimeoutHelper>();
  EXPECT_NO_THROW(th->SetTimeout(12.56));
  TimeoutHelperTest th_test(th.get());

  EXPECT_EQ(th->SetTimeout(-1), 1);
}

TEST(Inferencer, TimeoutHelper_Reset) {
  std::shared_ptr<TimeoutHelper> th = std::make_shared<TimeoutHelper>();
  std::function<void()> Func = NULL;

  TimeoutHelperTest th_test(th.get());
  th_test.setState(3);
  EXPECT_EQ(th->Reset(Func), 1);

  Func = []() -> void {};
  th_test.setState(0);
  th->Reset(Func);
  EXPECT_EQ(static_cast<int>(th_test.getState()), 2);

  Func = []() -> void {};
  th_test.setState(2);
  th->Reset(Func);
  EXPECT_EQ(static_cast<int>(th_test.getState()), 1);

  Func = nullptr;
  th_test.setState(0);
  EXPECT_EQ(th->Reset(Func), 0);
  EXPECT_EQ(static_cast<int>(th_test.getState()), 0);
}

TEST(Inferencer, TimeoutHelper_HandleFunc) {
  TimeoutHelper helper;
  const double timeout = 40;  // ms
  helper.SetTimeout(timeout);
  helper.LockOperator();
  std::promise<std::chrono::steady_clock::time_point> task_call_promise;
  auto task = [&task_call_promise] () {
    task_call_promise.set_value(std::chrono::steady_clock::now());
    return;
  };
  auto task_submit_time = std::chrono::steady_clock::now();
  helper.Reset(task);
  helper.UnlockOperator();
  auto task_call_time = task_call_promise.get_future().get();
  std::chrono::duration<double, std::milli> used_time = task_call_time - task_submit_time;
  EXPECT_GE(used_time.count(), timeout);
}

}  // namespace cnstream
