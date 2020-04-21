/*************************************************************************
 * copyright (C) [2019] by Cambricon, Inc. All rights reserved
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
#include <string>
#include "cnstream_time_utility.hpp"

using std::chrono::steady_clock;
using std::chrono::milliseconds;
using std::chrono::microseconds;

namespace cnstream {

TEST(TimeUtilityTest, TimeStampTest) {
  size_t ts1, ts2 = 0;
  std::string ts2_str;
  ts1 = TimeStamp::Current();
  auto start_time = steady_clock::now();
  while (1) {
    if (steady_clock::now() - start_time > milliseconds(100)) {
      ts2 = TimeStamp::Current();
      ts2_str = TimeStamp::CurrentToString();
      break;
    }
  }
  // inaccurate rate < 1/1000
  EXPECT_NEAR(ts2 - ts1, 1E5, 100);
  EXPECT_NEAR(std::stoll(ts2_str), ts2, 100);
  EXPECT_NEAR(std::stoll(ts2_str) - ts1, 1E5, 100);
}

TEST(TimeUtilityTest, TickClockTest) {
  TickClock tick_clock;
  int tick_times = 10;
  auto start_time = steady_clock::now();
  while (1) {
    if (steady_clock::now() - start_time > milliseconds(10)) {
      tick_clock.Tick();
      start_time = steady_clock::now();
      if (--tick_times <= 0) break;
    }
  }
  double avg_time = tick_clock.ElapsedAverageAsDouble();
  // inaccurate rate < 1/1000
  EXPECT_NEAR(avg_time, 1E4, 10);

  tick_clock.Clear();
  avg_time = tick_clock.ElapsedAverageAsDouble();
  EXPECT_DOUBLE_EQ(avg_time, 0.0);
}

TEST(TimeUtilityTest, TickTockClockTest) {
  TickTockClock duration_recorder;
  for (int i = 0; i < 10; ++i) {
    duration_recorder.Tick();
    auto start_time = steady_clock::now();
    while (steady_clock::now() - start_time < milliseconds(10)) {}
    duration_recorder.Tock();
  }

  double avg_duration = duration_recorder.ElapsedAverageAsDouble();
  // inaccurate rate < 1/1000
  EXPECT_NEAR(avg_duration, 1E4, 10);
}

TEST(TimeUtilityTest, TimerCallbackTimes) {
  // ensure every event will be triggered
  std::atomic<int> call_times{10};
  auto action = [&call_times] { --call_times; };

  Timer timer(microseconds(100));
  for (int i = call_times; i > 0; --i) {
    timer.StartOne(milliseconds(0), action);
  }
  std::this_thread::sleep_for(milliseconds(100));
  EXPECT_EQ(call_times, 0);
}

TEST(TimeUtilityTest, TimerBlockAction) {
  // will not blocked by a long term action
  std::atomic<bool> block{true};
  auto block_action = [&block] { while (block) {} };

  Timer timer(microseconds(100));
  timer.StartOne(microseconds(0), block_action);
  auto another_action = [] { SUCCEED(); };
  for (int i = 0; i < 10; ++i) {
    timer.StartOne(microseconds(100), another_action);
  }
  std::this_thread::sleep_for(milliseconds(100));
  block = false;
  SUCCEED();
}

}  // namespace cnstream
