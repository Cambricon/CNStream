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
#include <utility>

#include "profiler/module_profiler.hpp"

namespace cnstream {

TEST(CoreModuleProfiler, RegisterProcessName) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string module_name = "module";
  ModuleProfiler profiler(config, module_name, &tracer);
  const std::string process_name = "process";
  EXPECT_TRUE(profiler.RegisterProcessName(process_name));
  EXPECT_FALSE(profiler.RegisterProcessName(process_name));
}

TEST(CoreModuleProfiler, RecordProcessStart) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string module_name = "module";
  ModuleProfiler profiler(config, module_name, &tracer);
  const std::string process_name = "process";
  ASSERT_TRUE(profiler.RegisterProcessName(process_name));
  RecordKey key = std::make_pair("stream0", 0);
  profiler.RecordProcessStart(process_name, key);
  PipelineTrace trace = tracer.GetTrace(Time::min(), Time::max());
  EXPECT_EQ(trace.module_traces[module_name][process_name].size(), 1);
}

TEST(CoreModuleProfiler, RecordProcessEnd) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string module_name = "module";
  ModuleProfiler profiler(config, module_name, &tracer);
  const std::string process_name = "process";
  ASSERT_TRUE(profiler.RegisterProcessName(process_name));
  RecordKey key = std::make_pair("stream0", 0);
  profiler.RecordProcessEnd(process_name, key);
  PipelineTrace trace = tracer.GetTrace(Time::min(), Time::max());
  EXPECT_EQ(trace.module_traces[module_name][process_name].size(), 1);
}

TEST(CoreModuleProfiler, OnStreamEos) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string module_name = "module";
  ModuleProfiler profiler(config, module_name, &tracer);
  const std::string process_name = "process";
  ASSERT_TRUE(profiler.RegisterProcessName(process_name));
  const std::string stream_name = "stream0";
  RecordKey key = std::make_pair(stream_name, 0);
  profiler.RecordProcessStart(process_name, key);
  profiler.RecordProcessEnd(process_name, key);
  ModuleProfile profile = profiler.GetProfile();
  for (const auto& process_profile : profile.process_profiles)
    if (process_profile.process_name == process_name) {
      EXPECT_EQ(process_profile.stream_profiles.size(), 1);
    }
  profiler.OnStreamEos(stream_name);
  profile = profiler.GetProfile();
  for (const auto& process_profile : profile.process_profiles)
    if (process_profile.process_name == process_name) {
      EXPECT_EQ(process_profile.stream_profiles.size(), 0);
    }
}

TEST(CoreModuleProfiler, GetName) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string module_name = "module";
  ModuleProfiler profiler(config, module_name, &tracer);
  EXPECT_EQ(profiler.GetName(), module_name);
}

TEST(CoreModuleProfiler, GetProfile0) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string module_name = "module";
  ModuleProfiler profiler(config, module_name, &tracer);
  const std::string process_name = "process";
  ASSERT_TRUE(profiler.RegisterProcessName(process_name));
  const std::string stream_name = "stream0";
  RecordKey key = std::make_pair(stream_name, 0);
  profiler.RecordProcessStart(process_name, key);
  profiler.RecordProcessEnd(process_name, key);
  ModuleProfile profile = profiler.GetProfile();
  EXPECT_EQ(profile.process_profiles.size(), 1);
}

TEST(CoreModuleProfiler, GetProfile1) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string module_name = "module";
  ModuleProfiler profiler(config, module_name, &tracer);
  const std::string process_name = "process";
  ASSERT_TRUE(profiler.RegisterProcessName(process_name));
  const std::string stream_name = "stream0";
  RecordKey key1 = std::make_pair(stream_name, 0);
  RecordKey key2 = std::make_pair(stream_name, 1);
  ModuleTrace trace;
  ProcessTrace process_trace;
  TraceElem elem_start1;
  elem_start1.key = key1;
  elem_start1.time = Time(std::chrono::duration_cast<Time::duration>(std::chrono::duration<double, std::milli>(50)));
  elem_start1.type = TraceEvent::Type::START;
  TraceElem elem_start2;
  elem_start2.key = key2;
  elem_start2.time = Time(std::chrono::duration_cast<Time::duration>(std::chrono::duration<double, std::milli>(100)));
  elem_start2.type = TraceEvent::Type::START;
  TraceElem elem_end1;
  elem_end1.key = key1;
  elem_end1.time = Time(std::chrono::duration_cast<Time::duration>(std::chrono::duration<double, std::milli>(200)));
  elem_end1.type = TraceEvent::Type::END;
  TraceElem elem_end2;
  elem_end2.key = key2;
  elem_end2.time = Time(std::chrono::duration_cast<Time::duration>(std::chrono::duration<double, std::milli>(300)));
  elem_end2.type = TraceEvent::Type::END;
  process_trace.push_back(elem_start1);
  process_trace.push_back(elem_start2);
  process_trace.push_back(elem_end1);
  process_trace.push_back(elem_end2);
  trace[process_name] = process_trace;
  ModuleProfile profile = profiler.GetProfile(trace);

  ProcessProfile process_profile;
  for (const auto& it : profile.process_profiles)
    if (it.process_name == process_name)
      process_profile = it;

  EXPECT_EQ(process_profile.completed, 2);
  EXPECT_EQ(process_profile.fps, 1e3 / 250 * 2);
  EXPECT_EQ(process_profile.dropped, 0);
  EXPECT_EQ(process_profile.latency, 175);
  EXPECT_EQ(process_profile.minimum_latency, 150);
  EXPECT_EQ(process_profile.maximum_latency, 200);
  EXPECT_EQ(process_profile.ongoing, 0);
  EXPECT_EQ(process_profile.stream_profiles.size(), 1);
  EXPECT_EQ(process_profile.stream_profiles[0].stream_name, stream_name);
  EXPECT_EQ(process_profile.stream_profiles[0].completed, 2);
  EXPECT_EQ(process_profile.stream_profiles[0].dropped, 0);
  EXPECT_EQ(process_profile.stream_profiles[0].fps, 1e3 / 250 * 2);
  EXPECT_EQ(process_profile.stream_profiles[0].minimum_latency, 150);
  EXPECT_EQ(process_profile.stream_profiles[0].maximum_latency, 200);
}

}  // namespace cnstream
