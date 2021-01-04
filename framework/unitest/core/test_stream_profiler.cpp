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

#include <string>

#include "profiler/stream_profiler.hpp"

namespace cnstream {

TEST(CoreStreamProfiler, GetName) {
  const std::string name = "profiler";
  StreamProfiler profiler(name);
  EXPECT_EQ(name, profiler.GetName());
  EXPECT_EQ(name, profiler.GetProfile().stream_name);
}

TEST(CoreStreamProfiler, AddLatency) {
  StreamProfiler profiler("profiler");
  profiler.AddLatency(std::chrono::duration<double, std::milli>(2.0))
          .AddLatency(std::chrono::duration<double, std::milli>(3.0));
  StreamProfile profile = profiler.GetProfile();
  EXPECT_EQ(profiler.GetProfile().latency, (2.0 + 3.0) / 2);
}

TEST(CoreStreamProfiler, UpdatePhysicalTime) {
  StreamProfiler profiler("profiler");
  profiler.UpdatePhysicalTime(std::chrono::duration<double, std::milli>(1.0)).AddCompleted()
          .UpdatePhysicalTime(std::chrono::duration<double, std::milli>(3.0)).AddCompleted();
  EXPECT_EQ(profiler.GetProfile().fps, 1e3 / (3.0) * 2);
}

TEST(CoreStreamProfiler, AddDropped) {
  StreamProfiler profiler("profiler");
  profiler.AddDropped(2).AddDropped(20);
  EXPECT_EQ(profiler.GetProfile().dropped, 2 + 20);
}

TEST(CoreStreamProfiler, AddCompleted) {
  StreamProfiler profiler("profiler");
  profiler.AddCompleted().AddCompleted();
  EXPECT_EQ(profiler.GetProfile().completed, 2);
}

TEST(CoreStreamProfiler, GetProfile_NoDrop) {
  StreamProfiler profiler("profiler");
  profiler.UpdatePhysicalTime(std::chrono::duration<double, std::milli>(2.0))
          .AddLatency(std::chrono::duration<double, std::milli>(2.0))
          .AddCompleted();
  profiler.UpdatePhysicalTime(std::chrono::duration<double, std::milli>(4.0))
          .AddLatency(std::chrono::duration<double, std::milli>(2.0))
          .AddCompleted();
  const StreamProfile profile = profiler.GetProfile();
  EXPECT_EQ(profile.completed, 2);
  EXPECT_EQ(profile.counter, 2);
  EXPECT_EQ(profile.dropped, 0);
  EXPECT_EQ(profile.fps, 1e3 / 4.0 * 2);
  EXPECT_EQ(profile.latency, 2.0);
  EXPECT_EQ(profile.maximum_latency, 2.0);
  EXPECT_EQ(profile.minimum_latency, 2.0);
}

TEST(CoreStreamProfiler, GetProfile_WithDrop) {
  StreamProfiler profiler("profiler");
  profiler.UpdatePhysicalTime(std::chrono::duration<double, std::milli>(2.0))
          .AddLatency(std::chrono::duration<double, std::milli>(2.0))
          .AddCompleted();
  profiler.UpdatePhysicalTime(std::chrono::duration<double, std::milli>(4.0))
          .AddLatency(std::chrono::duration<double, std::milli>(2.0))
          .AddCompleted();
  profiler.AddDropped(2);
  const StreamProfile profile = profiler.GetProfile();
  EXPECT_EQ(profile.completed, 2);
  EXPECT_EQ(profile.counter, 4);
  EXPECT_EQ(profile.dropped, 2);
  EXPECT_EQ(profile.fps, 1e3 / 4.0 * 4);
  EXPECT_EQ(profile.latency, 2.0);
  EXPECT_EQ(profile.maximum_latency, 2.0);
  EXPECT_EQ(profile.minimum_latency, 2.0);
}

TEST(CoreStreamProfiler, GetProfile_MinMaxLatency) {
  StreamProfiler profiler("profiler");
  profiler.AddLatency(std::chrono::duration<double, std::milli>(2.0))
          .AddLatency(std::chrono::duration<double, std::milli>(3.0))
          .AddLatency(std::chrono::duration<double, std::milli>(3.0))
          .AddLatency(std::chrono::duration<double, std::milli>(1.0));
  const StreamProfile profile = profiler.GetProfile();
  EXPECT_EQ(profile.maximum_latency, 3.0);
  EXPECT_EQ(profile.minimum_latency, 1.0);
}

}  // namespace cnstream
