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
#include <future>
#include <string>

#include "util/cnstream_timer.hpp"

using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

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

  // Accurary depends on the current usage of CPU core.
  // The accuracy can reach to 1 in 100,000 when monopolize the core,
  // but also can less than 1 in 100 when the core is very heavy.
  // So do not test accuracy anymore in below unit tests.
  EXPECT_GE(ts2 - ts1, 1E5);
  EXPECT_GE(std::stoll(ts2_str) - ts1, 1E5);
}

TEST(TimeUtilityTest, TickClockTest) {
  TickClock tick_clock;
  int tick_times = 10;
  auto start_time = steady_clock::now();
  while (1) {
    if (steady_clock::now() - start_time > milliseconds(10)) {
      tick_clock.Tick();
      start_time = steady_clock::now();
      if (--tick_times < 0) break;
    }
  }
  double avg_time = tick_clock.ElapsedAverageAsDouble();
  EXPECT_GE(avg_time, 1E4);
  double total_time = tick_clock.ElapsedTotalAsDouble();
  EXPECT_GE(total_time, 1E5);

  tick_clock.Clear();
  avg_time = tick_clock.ElapsedAverageAsDouble();
  EXPECT_DOUBLE_EQ(avg_time, 0.0);
}

TEST(TimeUtilityTest, TickTockClockTest) {
  TickTockClock duration_recorder;
  for (int i = 0; i < 10; ++i) {
    duration_recorder.Tick();
    auto start_time = steady_clock::now();
    while (steady_clock::now() - start_time < milliseconds(10)) {
    }
    duration_recorder.Tock();
  }

  double avg_duration = duration_recorder.ElapsedAverageAsDouble();
  EXPECT_GE(avg_duration, 1E4);
  double total_time = duration_recorder.ElapsedTotalAsDouble();
  EXPECT_GE(total_time, 1E5);

  duration_recorder.Clear();
  avg_duration = duration_recorder.ElapsedAverageAsDouble();
  EXPECT_DOUBLE_EQ(avg_duration, 0.0);
}

}  // namespace cnstream
