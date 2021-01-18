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

#include "profiler/process_profiler.hpp"

namespace cnstream {

TEST(CoreProcessProfiler, GetName) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string profiler_name = "profiler";
  ProcessProfiler profiler(config, profiler_name, &tracer);
  EXPECT_EQ(profiler_name, profiler.GetName());
}

TEST(CoreProcessProfiler, SetModuleName) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string profiler_name = "profiler";
  ProcessProfiler profiler(config, profiler_name, &tracer);
  const std::string module_name = "module";
  profiler.SetModuleName(module_name).SetTraceLevel(TraceEvent::Level::MODULE);
  profiler.RecordStart(std::make_pair("stream0", 100));
  PipelineTrace trace = tracer.GetTrace(Time::min(), Time::max());
  EXPECT_NE(trace.module_traces.find(module_name), trace.module_traces.end());
  EXPECT_EQ(trace.module_traces[module_name].size(), 1);
  EXPECT_NE(trace.module_traces[module_name].find(profiler_name), trace.module_traces[module_name].end());
  EXPECT_EQ(trace.module_traces[module_name][profiler_name].size(), 1);
}

TEST(CoreProcessProfiler, SetTraceLevel) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string profiler_name = "profiler";
  ProcessProfiler profiler(config, profiler_name, &tracer);
  const std::string module_name = "module";
  profiler.SetModuleName(module_name).SetTraceLevel(TraceEvent::Level::PIPELINE);
  profiler.RecordStart(std::make_pair("stream0", 100));
  PipelineTrace trace = tracer.GetTrace(Time::min(), Time::max());
  EXPECT_EQ(trace.module_traces.find(module_name), trace.module_traces.end());
  EXPECT_NE(trace.process_traces.find(profiler_name), trace.process_traces.end());
  EXPECT_EQ(trace.process_traces[profiler_name].size(), 1);
}

TEST(CoreProcessProfiler, RecordStartModule) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string profiler_name = "profiler";
  ProcessProfiler profiler(config, profiler_name, &tracer);
  const std::string module_name = "module";
  profiler.SetModuleName(module_name).SetTraceLevel(TraceEvent::Level::MODULE);
  RecordKey key = std::make_pair("stream0", 100);
  profiler.RecordStart(key);
  PipelineTrace trace = tracer.GetTrace(Time::min(), Time::max());
  EXPECT_NE(trace.module_traces.find(module_name), trace.module_traces.end());
  EXPECT_EQ(trace.module_traces[module_name].size(), 1);
  EXPECT_NE(trace.module_traces[module_name].find(profiler_name), trace.module_traces[module_name].end());
  EXPECT_EQ(trace.module_traces[module_name][profiler_name].size(), 1);
  TraceElem elem = trace.module_traces[module_name][profiler_name][0];
  EXPECT_EQ(elem.type, TraceEvent::Type::START);
  EXPECT_EQ(elem.key, key);
}

TEST(CoreProcessProfiler, RecordEndModule) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string profiler_name = "profiler";
  ProcessProfiler profiler(config, profiler_name, &tracer);
  const std::string module_name = "module";
  profiler.SetModuleName(module_name).SetTraceLevel(TraceEvent::Level::MODULE);
  RecordKey key = std::make_pair("stream0", 100);
  profiler.RecordEnd(key);
  PipelineTrace trace = tracer.GetTrace(Time::min(), Time::max());
  ASSERT_NE(trace.module_traces.find(module_name), trace.module_traces.end());
  ASSERT_EQ(trace.module_traces[module_name].size(), 1);
  ASSERT_NE(trace.module_traces[module_name].find(profiler_name), trace.module_traces[module_name].end());
  ASSERT_EQ(trace.module_traces[module_name][profiler_name].size(), 1);
  TraceElem elem = trace.module_traces[module_name][profiler_name][0];
  EXPECT_EQ(elem.type, TraceEvent::Type::END);
  EXPECT_EQ(elem.key, key);
}

TEST(CoreProcessProfiler, RecordStartPipeline) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string profiler_name = "profiler";
  ProcessProfiler profiler(config, profiler_name, &tracer);
  profiler.SetTraceLevel(TraceEvent::Level::PIPELINE);
  RecordKey key = std::make_pair("stream0", 100);
  profiler.RecordStart(key);
  PipelineTrace trace = tracer.GetTrace(Time::min(), Time::max());
  ASSERT_NE(trace.process_traces.find(profiler_name), trace.process_traces.end());
  ASSERT_EQ(trace.process_traces[profiler_name].size(), 1);
  TraceElem elem = trace.process_traces[profiler_name][0];
  EXPECT_EQ(elem.type, TraceEvent::Type::START);
  EXPECT_EQ(elem.key, key);
}

TEST(CoreProcessProfiler, RecordEndPipeline) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string profiler_name = "profiler";
  ProcessProfiler profiler(config, profiler_name, &tracer);
  profiler.SetTraceLevel(TraceEvent::Level::PIPELINE);
  RecordKey key = std::make_pair("stream0", 100);
  profiler.RecordEnd(key);
  PipelineTrace trace = tracer.GetTrace(Time::min(), Time::max());
  ASSERT_NE(trace.process_traces.find(profiler_name), trace.process_traces.end());
  ASSERT_EQ(trace.process_traces[profiler_name].size(), 1);
  TraceElem elem = trace.process_traces[profiler_name][0];
  EXPECT_EQ(elem.type, TraceEvent::Type::END);
  EXPECT_EQ(elem.key, key);
}

TEST(CoreProcessProfiler, RecordEndRecordEnd) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string profiler_name = "profiler";
  ProcessProfiler profiler(config, profiler_name, &tracer);
  profiler.SetTraceLevel(TraceEvent::Level::PIPELINE);
  const std::string stream_name = "stream0";

  RecordKey key = std::make_pair("stream0", 100);

  profiler.RecordEnd(key);
  profiler.RecordEnd(key);

  ProcessProfile profile = profiler.GetProfile();
  EXPECT_EQ(profile.completed, 2);
}

TEST(CoreProcessProfiler, DropData) {
  static constexpr uint64_t kDEFAULT_MAX_DPB_SIZE = 16;
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string profiler_name = "profiler";
  ProcessProfiler profiler(config, profiler_name, &tracer);
  profiler.SetTraceLevel(TraceEvent::Level::PIPELINE);
  const std::string stream_name = "stream0";
  uint64_t dropped = 2;
  uint64_t ts = 0;
  for (; ts < dropped; ++ts) {
    RecordKey key = std::make_pair(stream_name, ts);
    profiler.RecordStart(key);
  }
  for (; ts < kDEFAULT_MAX_DPB_SIZE + dropped + 1; ++ts) {
    RecordKey key = std::make_pair(stream_name, ts);
    profiler.RecordStart(key);
    profiler.RecordEnd(key);
  }
  ProcessProfile profile = profiler.GetProfile();
  EXPECT_EQ(profile.dropped, dropped);
  EXPECT_EQ(profile.completed, kDEFAULT_MAX_DPB_SIZE + 1);
  EXPECT_EQ(profile.ongoing, 0);
  EXPECT_EQ(profile.stream_profiles.size(), 1);
  EXPECT_EQ(profile.stream_profiles[0].stream_name, stream_name);
  EXPECT_EQ(profile.stream_profiles[0].dropped, dropped);
  EXPECT_EQ(profile.stream_profiles[0].completed, kDEFAULT_MAX_DPB_SIZE + 1);
}

TEST(CoreProcessProfiler, GetProfile0) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string profiler_name = "profiler";
  ProcessProfiler profiler(config, profiler_name, &tracer);
  profiler.SetTraceLevel(TraceEvent::Level::PIPELINE);
  const std::string stream_name = "stream0";

  RecordKey key = std::make_pair("stream0", 100);

  profiler.RecordEnd(key);
  ProcessProfile profile = profiler.GetProfile();
  EXPECT_EQ(profile.dropped, 0);
  EXPECT_EQ(profile.completed, 1);
  EXPECT_EQ(profile.ongoing, 0);
  EXPECT_EQ(profile.stream_profiles.size(), 1);
  EXPECT_EQ(profile.stream_profiles[0].completed, 1);
  EXPECT_EQ(profile.stream_profiles[0].dropped, 0);
}

TEST(CoreProcessProfiler, GetProfile1) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string profiler_name = "profiler";
  ProcessProfiler profiler(config, profiler_name, &tracer);
  profiler.SetTraceLevel(TraceEvent::Level::PIPELINE);
  const std::string stream_name = "stream0";

  RecordKey key1 = std::make_pair(stream_name, 100);
  RecordKey key2 = std::make_pair(stream_name, 200);

  ProcessTrace trace;
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
  trace.push_back(elem_start1);
  trace.push_back(elem_start2);
  trace.push_back(elem_end1);
  trace.push_back(elem_end2);

  ProcessProfile profile = profiler.GetProfile(trace);

  EXPECT_EQ(profile.completed, 2);
  EXPECT_EQ(profile.fps, 1e3 / 250 * 2);
  EXPECT_EQ(profile.dropped, 0);
  EXPECT_EQ(profile.latency, 175);
  EXPECT_EQ(profile.minimum_latency, 150);
  EXPECT_EQ(profile.maximum_latency, 200);
  EXPECT_EQ(profile.ongoing, 0);
  EXPECT_EQ(profile.process_name, profiler_name);
  EXPECT_EQ(profile.stream_profiles.size(), 1);
  EXPECT_EQ(profile.stream_profiles[0].stream_name, stream_name);
  EXPECT_EQ(profile.stream_profiles[0].completed, 2);
  EXPECT_EQ(profile.stream_profiles[0].dropped, 0);
  EXPECT_EQ(profile.stream_profiles[0].fps, 1e3 / 250 * 2);
  EXPECT_EQ(profile.stream_profiles[0].minimum_latency, 150);
  EXPECT_EQ(profile.stream_profiles[0].maximum_latency, 200);
}

TEST(CoreProcessProfiler, OnStreamEos) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string profiler_name = "profiler";
  ProcessProfiler profiler(config, profiler_name, &tracer);
  profiler.SetTraceLevel(TraceEvent::Level::PIPELINE);
  const std::string stream_name = "stream0";

  RecordKey key = std::make_pair(stream_name, 100);
  profiler.RecordStart(key);
  profiler.RecordEnd(key);
  EXPECT_EQ(profiler.GetProfile().stream_profiles.size(), 1);
  profiler.OnStreamEos(stream_name);
  EXPECT_EQ(profiler.GetProfile().stream_profiles.size(), 0);
}

TEST(CoreProcessProfiler, OnStreamEosBorderCase) {
  PipelineTracer tracer;
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string profiler_name = "profiler";
  ProcessProfiler profiler(config, profiler_name, &tracer);
  profiler.SetTraceLevel(TraceEvent::Level::PIPELINE);
  const std::string stream_name = "stream0";

  EXPECT_EQ(profiler.GetProfile().stream_profiles.size(), 0);
  profiler.OnStreamEos(stream_name);
  EXPECT_EQ(profiler.GetProfile().stream_profiles.size(), 0);
}

TEST(CoreProcessProfiler, NullTracer) {
  ProfilerConfig config;
  config.enable_profiling = true;
  config.enable_tracing = true;
  const std::string profiler_name = "profiler";
  ProcessProfiler profiler(config, profiler_name, nullptr);
  RecordKey key = std::make_pair("stream0", 100);
  profiler.RecordStart(key);
}

}  // namespace cnstream
