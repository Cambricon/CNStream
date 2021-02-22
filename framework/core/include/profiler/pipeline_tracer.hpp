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
