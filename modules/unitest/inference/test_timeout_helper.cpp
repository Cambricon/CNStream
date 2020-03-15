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
  enum TimeoutHelper::State state = TimeoutHelper::STATE_NO_FUNC;
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
  double wait_time = 600.0;  // ms
  std::shared_ptr<TimeoutHelper> th = std::make_shared<TimeoutHelper>();
  // default state is STATE_NO_FUNC
  TimeoutHelperTest th_test(th.get());
  th_test.setTime(wait_time);
  std::atomic<bool> HandleCall(false);
  std::function<void()> Func = []() -> void {};

  std::thread& Threadhandle = th_test.getThread();
  if (Threadhandle.joinable()) Threadhandle.detach();

  std::function<double()> wait_for_time([&]() -> double {
    // HandleCall.store(true);
    auto stime = std::chrono::steady_clock::now();
    th_test.ConditionNotify();  // unlock the last wait
    th_test.setState(1);
    HandleCall.store(true);
    // get the next state(until wait_for time end)
    while (static_cast<int>(th_test.getState()) != 2) std::this_thread::sleep_for(std::chrono::nanoseconds(1));
    th->Reset(Func);  // for the next test
    auto etime = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::nano> diff = etime - stime;
    return diff.count();
  });
  // one loop finish with state_do
  std::future<double> handle_func = std::async(std::launch::async, wait_for_time);
  while (!HandleCall.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  // wait until the value is get
  double real_wait_time = handle_func.get();
  EXPECT_GE(real_wait_time, static_cast<double>(wait_time));

  // wait for wait_for time end
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  // test state_do
  EXPECT_EQ(th_test.get_timeout_print_cnt(), 1);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  Func = []() -> void {};
  th->Reset(Func);
  th_test.set_timeout_print_cnt(99);
  th_test.setState(2);  // state_do
  th_test.ConditionNotify();
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  EXPECT_EQ(th_test.get_timeout_print_cnt(), 0);

  EXPECT_EQ(static_cast<int>(th_test.getState()), 0);
}

}  // namespace cnstream
