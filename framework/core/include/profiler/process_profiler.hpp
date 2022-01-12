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

#ifndef CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PROCESS_PROFILER_HPP_
#define CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PROCESS_PROFILER_HPP_

#include <algorithm>
#include <string>
#include <map>
#include <vector>

#include "cnstream_common.hpp"
#include "cnstream_config.hpp"
#include "profiler/pipeline_tracer.hpp"
#include "profiler/profile.hpp"
#include "profiler/stream_profiler.hpp"
#include "profiler/trace.hpp"

/*!
 *  @file process_profiler.hpp
 *
 *  This file contains a declaration of the ProcessProfiler class.
 */
namespace cnstream {

class RecordPolicy;

/*!
 * @class ProcessProfiler
 *
 * @brief ProcessProfiler is the profiler for a process. A process can be a function call or a piece of code.
 *
 * @note This class is thread safe.
 */
class ProcessProfiler : private NonCopyable {
 public:
  /*!
   * @brief Constructs a ProcessProfiler object.
   *
   * @param[in] config The configuration of the profiler.
   * @param[in] process_name The name of the process.
   * @param[in] tracer The tracer for tracing events.
   *
   * @return No return value.
   */
  explicit ProcessProfiler(const ProfilerConfig& config,
                           const std::string& process_name,
                           PipelineTracer* tracer);
  /*!
   * @brief Destructs a ProcessProfiler object.
   *
   * @return No return value.
   */
  ~ProcessProfiler();

  /*!
   * @brief Sets the module name to identify which module this profiler belongs to.
   *        The module name takes effect when the trace level is TraceEvent::MODULE.
   *        The trace level can be set by cnstream::ProcessProfiler::SetTraceLevel.
   *
   * @param[in] module_name The name of the module.
   *
   * @return Returns this profiler itself.
   */
  ProcessProfiler& SetModuleName(const std::string& module_name);

  /*!
   * @brief Set the trace level for this profiler.
   *        Trace level identifies whether this profiler belongs to a module or a pipeline.
   *
   * @param[in] level Trace level.
   *
   * @return Returns the ProcessProfiler object itself.
   *
   * @see cnstream::TraceEvent::Level.
   */
  ProcessProfiler& SetTraceLevel(const TraceEvent::Level& level);

  /*!
   * @brief Records the start of the process.
   *
   * @param[in] key The unique identifier of a CNFrameInfo instance.
   *
   * @return No return value.
   *
   * @see cnstream::RecordKey.
   */
  void RecordStart(const RecordKey& key);

  /*!
   * @brief Records the end of the process.
   *
   * @param[in] key The unique identifier of a CNFrameInfo instance.
   *
   * @return No return value.
   *
   * @see cnstream::RecordKey.
   */
  void RecordEnd(const RecordKey& key);

  /*!
   * @brief Gets the name of the process.
   *
   * @return The name of the process.
   */
  std::string GetName() const;

  /*!
   * @brief Gets profiling results of the process during the execution of the program.
   *
   * @return Returns the profiling results.
   */
  ProcessProfile GetProfile();

  /*!
   * @brief Gets profiling results according to the trace data.
   *
   * @param[in] trace The trace data of the process.
   *
   * @return Returns the profiling results.
   */
  ProcessProfile GetProfile(const ProcessTrace& trace) const;

  /*!
   * @brief Clears profiling data of the stream named by ``stream_name``, as the end of the stream is reached.
   *
   * @param[in] stream_name The name of the stream, usually the ``CNFrameInfo::stream_id``.
   *
   * @return No return value.
   */
  void OnStreamEos(const std::string& stream_name);

 private:
  // Records start time, called by RecordStart(const RecordKey&).
  void RecordStart(const RecordKey& key, const Time& time);

  // Records end time, called by RecordEnd(const RecordKey&).
  void RecordEnd(const RecordKey& key, const Time& time);

  // Increases the physical time used by the process named by ``process_name``.
  void AddPhysicalTime(const Time& now);

  // Statistics latency during profiling.
  void AddLatency(const std::string& stream_name, const Duration& latency);

  // Statistics the number of dropped datas during profiling.
  void AddDropped(const std::string& stream_name, uint64_t dropped);

  // Tell this profiler the stream named by ``stream_name`` is going to be profiled.
  // Prepares resources that needed by profiler for profiling the stream named by ``stream_name``.
  // Called when the first record of the stream named by ``stream_name`` arrives.
  void OnStreamStart(const std::string& stream_name);

  // Gets profiling results for streams.
  std::vector<StreamProfiler> GetStreamProfilers();

  // Tracing. Called by RecordStart and RecordEnd when config_.enable_tracing is true.
  void Tracing(const RecordKey& key, const Time& time, const TraceEvent::Type& type);

 private:
  ProfilerConfig config_;
  std::mutex lk_;
  // Processing data counter.
  // The data that only records the start time but not the end time is called an ongoing-data.
  uint64_t ongoing_ = 0;
  // Dropped frame counter.
  uint64_t dropped_              = 0;
  // Completed frame counter. It is incremented by 1 when an end time is recorded.
  uint64_t completed_            = 0;
  // The number of latencies counted.
  // The latency will be be counted only when the start and end times of the data are recorded.
  uint64_t latency_add_times_    = 0;
  // The last time recorded by RecordStart or RecordEnd.
  Time last_record_time_         = Time::min();
  Duration total_latency_        = Duration::zero();
  Duration maximum_latency_      = Duration::zero();
  Duration minimum_latency_      = Duration::max();
  // Physical time used for the process named by ``process_name``.
  Duration total_phy_time_       = Duration::zero();
  std::string module_name_       = "";
  std::string process_name_      = "";
  PipelineTracer* tracer_        = nullptr;
  // Start time record tool.
  RecordPolicy* record_policy_   = nullptr;
  TraceEvent::Level trace_level_;
  // Stream profilers for each stream.
  std::map<std::string, StreamProfiler> stream_profilers_;
};  // class ProcessProfiler

inline ProcessProfiler& ProcessProfiler::SetModuleName(const std::string& module_name) {
  module_name_ = module_name;
  return *this;
}

inline ProcessProfiler& ProcessProfiler::SetTraceLevel(const TraceEvent::Level& level) {
  trace_level_ = level;
  return *this;
}

inline std::string ProcessProfiler::GetName() const {
  return process_name_;
}

inline void ProcessProfiler::AddLatency(const std::string& stream_name, const Duration& latency) {
  total_latency_ += latency;
  maximum_latency_ = std::max(latency, maximum_latency_);
  minimum_latency_ = std::min(latency, minimum_latency_);
  latency_add_times_++;
  stream_profilers_.find(stream_name)->second.AddLatency(latency);
}

inline void ProcessProfiler::AddDropped(const std::string& stream_name, uint64_t dropped) {
  dropped_ += dropped;
  stream_profilers_.find(stream_name)->second.AddDropped(dropped);
}

inline void ProcessProfiler::Tracing(const RecordKey& key, const Time& time, const TraceEvent::Type& type) {
  tracer_->RecordEvent(TraceEvent(key).SetModuleName(module_name_)
                                      .SetProcessName(process_name_)
                                      .SetLevel(trace_level_)
                                      .SetTime(time)
                                      .SetType(type));
}

}  // namespace cnstream

#endif  // CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PROCESS_PROFILER_HPP_
