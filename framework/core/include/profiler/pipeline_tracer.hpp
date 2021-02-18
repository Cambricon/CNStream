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

#ifndef CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PIPELINE_TRACER_HPP_
#define CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PIPELINE_TRACER_HPP_

#include <chrono>
#include <memory>
#include <string>

#include "cnstream_common.hpp"
#include "profiler/trace.hpp"

namespace cnstream {

template<typename T>
class CircularBuffer;

/**
 * PipelineTracer can be used to record trace events for pipeline.
 */
class PipelineTracer : private NonCopyable {
 public:
  /**
   * @brief Constructor of PipelineTracer
   * 
   * It used to do tracing and store trace events.
   * 
   * @param capacity Capacity to store trace events.
   **/
  explicit PipelineTracer(size_t capacity = 100000);

  ~PipelineTracer();

  /**
   * @brief Records trace event.
   * 
   * @param event Trace event.
   * 
   * @return void.
   **/
  void RecordEvent(const TraceEvent& event);

  /**
   * @brief Records trace event.
   * 
   * @param event Trace event.
   * 
   * @return void.
   **/
  void RecordEvent(TraceEvent&& event);

  /**
   * @brief Gets trace data of pipeline for a specified period of time.
   * 
   * @param start Start time.
   * @param end End time.
   * 
   * @return Returns trace data of pipeline.
   **/
  PipelineTrace GetTrace(const Time& start, const Time& end) const;

  /**
   * @brief Gets trace data of pipeline for a specified period of time.
   * 
   * @param end End time
   * @param duration Length of time before `end`.
   * 
   * @return Returns trace data of pipeline.
   **/
  PipelineTrace GetTraceBefore(const Time& end, const Duration& duration) const;

  /**
   * @brief Gets trace data of pipeline for a specified period of time.
   * 
   * @param start Start time.
   * @param duration Length of time after `start`.
   * 
   * @return Returns trace data of pipeline.
   **/
  PipelineTrace GetTraceAfter(const Time& start, const Duration& duration) const;

 private:
  CircularBuffer<TraceEvent>* buffer_ = nullptr;
};  // class PipelineTracer

inline PipelineTrace PipelineTracer::GetTraceBefore(const Time& end, const Duration& duration) const {
  return GetTrace(std::chrono::time_point_cast<Clock::duration>(end - duration), end);
}

inline PipelineTrace PipelineTracer::GetTraceAfter(const Time& start, const Duration& duration) const {
  return GetTrace(start, std::chrono::time_point_cast<Clock::duration>(start + duration));
}

}  // namespace cnstream

#endif  // CNSTREAM_FRAMEWORK_CORE_INCLUDE_TRACER_PIPELINE_TRACER_HPP_
