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

#include "profiler/pipeline_tracer.hpp"

namespace cnstream {

TEST(CorePipelineTracer, Capacity) {
  size_t capacity = 100;
  PipelineTracer tracer(capacity);
  const std::string stream_name = "stream0";
  const std::string process_name = "process";
  RecordKey key = std::make_pair(stream_name, 0);
  TraceEvent event(key);
  event.SetKey(key).SetLevel(TraceEvent::Level::PIPELINE)
       .SetProcessName(process_name).SetTime(Clock::now())
       .SetType(TraceEvent::Type::START);
  for (size_t i = 0; i < capacity * 2; ++i)
    tracer.RecordEvent(event);
  PipelineTrace trace = tracer.GetTrace(Time::min(), Time::max());
  ASSERT_EQ(trace.process_traces.size(), 1);
  ASSERT_NE(trace.process_traces.find(process_name), trace.process_traces.end());
  EXPECT_EQ(trace.process_traces[process_name].size(), capacity);
}

TEST(CorePipelineTracer, RecordEvent) {
  PipelineTracer tracer;
  const std::string stream_name = "stream0";
  const std::string process_name = "process";
  RecordKey key = std::make_pair(stream_name, 0);
  TraceEvent event(key);
  event.SetKey(key).SetLevel(TraceEvent::Level::PIPELINE)
       .SetProcessName(process_name).SetTime(Clock::now())
       .SetType(TraceEvent::Type::START);
  tracer.RecordEvent(event);
  tracer.RecordEvent(std::move(event));

  PipelineTrace trace = tracer.GetTrace(Time::min(), Time::max());
  ASSERT_EQ(trace.process_traces.size(), 1);
  ASSERT_NE(trace.process_traces.find(process_name), trace.process_traces.end());
  EXPECT_EQ(trace.process_traces[process_name].size(), 2);
}

TEST(CorePipelineTracer, GetTrace) {
  PipelineTracer tracer;
  const std::string stream_name = "stream0";
  const std::string process_name = "process";
  RecordKey key = std::make_pair(stream_name, 0);
  TraceEvent event(key);
  auto start_time = Clock::now();
  event.SetKey(key).SetLevel(TraceEvent::Level::PIPELINE)
       .SetProcessName(process_name).SetTime(Clock::now())
       .SetType(TraceEvent::Type::START);
  auto event_tmp = event;
  tracer.RecordEvent(event);
  tracer.RecordEvent(std::move(event));

  PipelineTrace trace;
  trace = tracer.GetTrace(Time::min(), Time::max());

  ASSERT_EQ(trace.process_traces.size(), 1);
  ASSERT_NE(trace.process_traces.find(process_name), trace.process_traces.end());
  EXPECT_EQ(trace.process_traces[process_name].size(), 2);
  for (TraceElem it : trace.process_traces[process_name]) {
    EXPECT_EQ(it.key, event_tmp.key);
    EXPECT_EQ(it.time, event_tmp.time);
    EXPECT_EQ(it.type, event_tmp.type);
  }

  trace = tracer.GetTraceBefore(Clock::now(), Duration(1e10));

  ASSERT_EQ(trace.process_traces.size(), 1);
  ASSERT_NE(trace.process_traces.find(process_name), trace.process_traces.end());
  EXPECT_EQ(trace.process_traces[process_name].size(), 2);
  for (TraceElem it : trace.process_traces[process_name]) {
    EXPECT_EQ(it.key, event_tmp.key);
    EXPECT_EQ(it.time, event_tmp.time);
    EXPECT_EQ(it.type, event_tmp.type);
  }

  trace = tracer.GetTraceAfter(start_time, Duration(1e10));

  ASSERT_EQ(trace.process_traces.size(), 1);
  ASSERT_NE(trace.process_traces.find(process_name), trace.process_traces.end());
  EXPECT_EQ(trace.process_traces[process_name].size(), 2);
  for (TraceElem it : trace.process_traces[process_name]) {
    EXPECT_EQ(it.key, event_tmp.key);
    EXPECT_EQ(it.time, event_tmp.time);
    EXPECT_EQ(it.type, event_tmp.type);
  }
}


}  // namespace cnstream
