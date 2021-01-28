/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#ifndef CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_TRACE_HPP_
#define CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_TRACE_HPP_

#include <chrono>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cnstream {

using Clock    = std::chrono::steady_clock;
using Duration = std::chrono::duration<double, std::milli>;
using Time     = Clock::time_point;

/**
 * Unique identification of a frame in tracing and profiling.
 * Usually, first: stream_name(CNFrameInfo::stream_id), second: pts(CNFrameInfo::timestamp).
 **/
using RecordKey = std::pair<std::string, int64_t>;

struct TraceEvent {
  RecordKey key;                 ///< Unique identification of a frame.
  std::string module_name;       ///< Module name.
  std::string process_name;      ///< Process name. A process can be a function call or a piece of code.
  Time time;                     ///< Event time.
  enum Level {
    PIPELINE = 0,                ///< Event in pipeline.
    MODULE                       ///< Event in module.
  } level = PIPELINE;            ///< Event level.
  enum Type {
    START = 1 << 0,              ///< Process start event.
    END = 1 << 1                 ///< Process end event.
  } type = START;                ///< Event type.

  TraceEvent() = default;
  explicit TraceEvent(const RecordKey& key);
  explicit TraceEvent(RecordKey&& key);
  TraceEvent(const TraceEvent& other) = default;
  TraceEvent& operator=(const TraceEvent& other) = default;
  TraceEvent(TraceEvent&& other);
  TraceEvent& operator==(TraceEvent&& other);
  TraceEvent& SetKey(const RecordKey& key);
  TraceEvent& SetKey(RecordKey&& key);
  TraceEvent& SetModuleName(const std::string& module_name);
  TraceEvent& SetModuleName(std::string&& module_name);
  TraceEvent& SetProcessName(const std::string& process_name);
  TraceEvent& SetProcessName(std::string&& process_name);
  TraceEvent& SetTime(const Time& process_name);
  TraceEvent& SetTime(Time&& process_name);
  TraceEvent& SetLevel(const Level& level);
  TraceEvent& SetType(const Type& type);
};  // struct TraceEvent

struct TraceElem {
  RecordKey key;                ///< Unique identification of a frame.
  Time time;                    ///< Event time.
  TraceEvent::Type type;        ///< Event type. Process start or process end.
  TraceElem() = default;
  TraceElem(const TraceElem& other) = default;
  TraceElem& operator=(const TraceElem& other) = default;
  TraceElem(TraceElem&& other);
  TraceElem& operator=(TraceElem&& other);
  explicit TraceElem(const TraceEvent& event);
  explicit TraceElem(TraceEvent&& event);
};  // struct TraceElem

using ProcessTrace = std::vector<TraceElem>;

using ModuleTrace = std::unordered_map<std::string, ProcessTrace>;

struct PipelineTrace {
  std::unordered_map<std::string, ProcessTrace> process_traces;
  std::unordered_map<std::string, ModuleTrace> module_traces;
  PipelineTrace() = default;
  PipelineTrace(const PipelineTrace& other) = default;
  PipelineTrace& operator=(const PipelineTrace& other) = default;
  PipelineTrace(PipelineTrace&& other);
  PipelineTrace& operator=(PipelineTrace&& other);
};  // struct PipelineTrace

inline TraceEvent::TraceEvent(const RecordKey& key) : key(key) {}

inline TraceEvent::TraceEvent(RecordKey&& key) : key(std::forward<RecordKey>(key)) {}

inline TraceEvent::TraceEvent(TraceEvent&& other) {
  *this = std::forward<TraceEvent>(other);
}

inline TraceEvent& TraceEvent::operator==(TraceEvent&& other) {
  key = std::move(other.key);
  module_name = std::move(other.module_name);
  process_name = std::move(other.process_name);
  time = std::move(other.time);
  level = other.level;
  type = other.type;
  return *this;
}

inline TraceEvent& TraceEvent::SetKey(const RecordKey& key) {
  this->key = key;
  return *this;
}

inline TraceEvent& TraceEvent::SetKey(RecordKey&& key) {
  this->key = std::forward<RecordKey>(key);
  return *this;
}

inline TraceEvent& TraceEvent::SetModuleName(const std::string& module_name) {
  this->module_name = module_name;
  return *this;
}

inline TraceEvent& TraceEvent::SetModuleName(std::string&& module_name) {
  this->module_name = std::forward<std::string>(module_name);
  return *this;
}

inline TraceEvent& TraceEvent::SetProcessName(const std::string& process_name) {
  this->process_name = process_name;
  return *this;
}

inline TraceEvent& TraceEvent::SetProcessName(std::string&& process_name) {
  this->process_name = std::forward<std::string>(process_name);
  return *this;
}

inline TraceEvent& TraceEvent::SetTime(const Time& time) {
  this->time = time;
  return *this;
}

inline TraceEvent& TraceEvent::SetTime(Time&& time) {
  this->time = std::forward<Time>(time);
  return *this;
}

inline TraceEvent& TraceEvent::SetLevel(const Level& level) {
  this->level = level;
  return *this;
}

inline TraceEvent& TraceEvent::SetType(const Type& type) {
  this->type = type;
  return *this;
}

inline TraceElem::TraceElem(TraceElem&& other) {
  *this = std::forward<TraceElem>(other);
}

inline TraceElem& TraceElem::operator=(TraceElem&& other) {
  key = std::move(other.key);
  time = std::move(other.time);
  type = other.type;
  return *this;
}

inline TraceElem::TraceElem(const TraceEvent& event) {
  key = event.key;
  time = event.time;
  type = event.type;
}

inline TraceElem::TraceElem(TraceEvent&& event) {
  key = std::move(event.key);
  time = std::move(event.time);
  type = event.type;
}

inline PipelineTrace::PipelineTrace(PipelineTrace&& other) {
  *this = std::forward<PipelineTrace>(other);
}

inline PipelineTrace& PipelineTrace::operator=(PipelineTrace&& other) {
  process_traces = std::move(other.process_traces);
  module_traces = std::move(other.module_traces);
  return *this;
}

}  // namespace cnstream

#endif  // CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_TRACE_HPP_
