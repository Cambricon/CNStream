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

#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>

#include "gtest/gtest.h"

#include "cnstream_timer.hpp"

using time_point = std::chrono::time_point<std::chrono::high_resolution_clock>;

namespace cnstream {

class TestCoreCNTimer : public testing::Test {
 public:
  inline void Start();
  inline void End();
  double Interval();
  CNTimer timer_;
  time_point start_, end_;
  uint32_t seed_ = (uint32_t)time(0);
};

inline void TestCoreCNTimer::Start() { start_ = std::chrono::high_resolution_clock::now(); }

inline void TestCoreCNTimer::End() { end_ = std::chrono::high_resolution_clock::now(); }

double TestCoreCNTimer::Interval() {
  std::chrono::duration<double, std::milli> diff;
  diff = end_ - start_;
  return diff.count();
}

// Test Dot(step)
TEST_F(TestCoreCNTimer, DotOneParam) {
  EXPECT_NO_THROW(timer_.Dot(0)) << "step = 0, Dot(step) should return.";

  uint32_t step = 1;
  uint32_t interval = rand_r(&seed_) % 100;
  std::chrono::duration<double, std::micro> dura(interval * 1000);
  timer_.Dot(1);
  Start();
  std::this_thread::sleep_for(dura);
  End();
  timer_.Dot(step);
  EXPECT_NEAR(timer_.GetAvg(), Interval() / (double)step, 0.02f);

  timer_.Clear();
  EXPECT_EQ(timer_.GetAvg(), 0);

  step = rand_r(&seed_) % 10;
  interval = rand_r(&seed_) % 100;
  dura = std::chrono::duration<double, std::micro>(interval * 1000);
  timer_.Dot(1);
  Start();
  std::this_thread::sleep_for(dura);
  End();
  timer_.Dot(step);
  if (step == 0) {
    EXPECT_EQ(timer_.GetAvg(), 0);
  } else {
    EXPECT_NEAR(timer_.GetAvg(), Interval() / (double)step, 0.02f);
  }
}

// test dot(time, step)
TEST_F(TestCoreCNTimer, DotTwoParam) {
  EXPECT_NO_THROW(timer_.Dot(0, rand_r(&seed_) % 100)) << "time = 0, Dot(time, step) should return.";
  EXPECT_NO_THROW(timer_.Dot(rand_r(&seed_) % 100, 0)) << "step = 0, Dot(time, step) should return.";

  timer_.Clear();
  double rand_time = rand_r(&seed_) % 10000 + 1;
  uint32_t step = rand_r(&seed_) % 100 + 1;
  timer_.Dot(rand_time, step);
  EXPECT_FLOAT_EQ(timer_.GetAvg(), (rand_time / step));
}

// test MixUp two FpsCalculator
TEST_F(TestCoreCNTimer, MixUp) {
  timer_.Clear();
  double rand_time = rand_r(&seed_) % 10000 + 1;
  uint32_t step = rand_r(&seed_) % 100 + 1;
  timer_.Dot(rand_time, step);

  CNTimer fps_calculator_addend;
  double time_addend = rand_r(&seed_) % 10000 + 1;
  uint32_t step_addend = rand_r(&seed_) % 100 + 1;
  fps_calculator_addend.Dot(time_addend, step_addend);

  timer_.MixUp(fps_calculator_addend);
  step_addend += step;
  EXPECT_FLOAT_EQ(timer_.GetAvg(), (rand_time / step_addend + time_addend / step_addend));
}

}  // namespace cnstream
