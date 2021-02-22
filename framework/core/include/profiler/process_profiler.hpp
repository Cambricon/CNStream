#ifndef CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PROCESS_PROFILER_HPP_
#define CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PROCESS_PROFILER_HPP_

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "cnstream_common.hpp"
#include "cnstream_config.hpp"
#include "util/cnstream_spinlock.hpp"
#include "profiler/pipeline_tracer.hpp"
#include "profiler/profile.hpp"
#include "profiler/stream_profiler.hpp"
#include "profiler/trace.hpp"

namespace cnstream {

class RecordPolicy;

/**
 * A profiler for a process. A process can be a function call or a piece of code.
 * This class is thread-safe.
 **/
class ProcessProfiler : private NonCopyable {
 public:
  /**
   * @brief Constructor of ProcessProfiler.
   *
   * @param config Profiler config.
   * @param process_name The name of a process.
   * @param tracer The tracer.
   **/
  explicit ProcessProfiler(const ProfilerConfig& config,
                           const std::string& process_name,
                           PipelineTracer* tracer);

  ~ProcessProfiler();

  /**
   * @brief Set the module name to identify which module this profiler belongs to.
   *        The module name takes effect when trace level is TraceEvent::MODULE.
   *        Trace level can be set by SetTraceLevel.
   *
   * @param module_name The name of module.
   * 
   * @return Returns this profiler itself.
   **/
  ProcessProfiler& SetModuleName(const std::string& module_name);

  /**
   * @brief Set the trace level for this profiler.
   *        Trace level identifies whether this profiler belongs to a module or a pipeline.
   *
   * @param level Trace level.
   * 
   * @return Returns this profiler itself.
   * 
   * @see TraceEvent::Level.
   **/
  ProcessProfiler& SetTraceLevel(const TraceEvent::Level& level);

  /**
   * @brief Records process start.
   * 
   * @param key Unique identifier of a CNFrameInfo instance.
   * 
   * @return void.
   * 
   * @see RecordKey.
   **/
  void RecordStart(const RecordKey& key);

  /**
   * @brief Records process end.
   * 
   * @param key Unique identifier of a CNFrameInfo instance.
   * 
   * @return void.
   * 
   * @see RecordKey.
   **/
  void RecordEnd(const RecordKey& key);

  /**
   * @brief Gets process name set by constructor.
   * 
   * @return The name of process set by constructor.
   **/
  std::string GetName() const;

  /**
   * @brief Gets profiling results of the whole run time.
   * 
   * @return Returns the profiling results.
   **/
  ProcessProfile GetProfile();

  /**
   * @brief Gets profiling results according to the trace datas.
   * 
   * @param trace Trace datas.
   * 
   * @return Returns the profiling results.
   **/
  ProcessProfile GetProfile(const ProcessTrace& trace) const;

  /**
   * @brief Tells the profiler to clear datas of stream named by `stream_name`.
   * 
   * @param stream_name Stream name. Usually it is comes from `CNFrameInfo::stream_id`.
   * 
   * @return void.
   **/
  void OnStreamEos(const std::string& stream_name);

 private:
  // Records start time, called by RecordStart(const RecordKey&).
  void RecordStart(const RecordKey& key, const Time& time);

  // Records end time, called by RecordEnd(const RecordKey&).
  void RecordEnd(const RecordKey& key, const Time& time);

  // Increases the physical time used by the process named by `process_name`.
  void AddPhysicalTime(const Time& now);

  // Statistics latency during profiling.
  void AddLatency(const std::string& stream_name, const Duration& latency);

  // Statistics the number of dropped datas during profiling.
  void AddDropped(const std::string& stream_name, uint64_t dropped);

  // Tell this profiler the stream named by `stream_name` is going to be profiled.
  // Prepares resources that needed by profiler for profiling the stream named by `stream_name`.
  // Called when the first record of the stream named by `stream_name` arrives.
  void OnStreamStart(const std::string& stream_name);

  // Gets profiling results for streams.
  std::vector<StreamProfiler> GetStreamProfilers();

  // Tracing. Called by RecordStart and RecordEnd when config_.enable_tracing is true.
  void Tracing(const RecordKey& key, const Time& time, const TraceEvent::Type& type);

 private:
  ProfilerConfig config_;
  SpinLock lk_;
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
  // Physical time used for the process named by `process_name`.
  Duration total_phy_time_       = Duration::zero();
  std::string module_name_       = "";
  std::string process_name_      = "";
  PipelineTracer* tracer_        = nullptr;
  // Start time record tool.
  RecordPolicy* record_policy_   = nullptr;
  TraceEvent::Level trace_level_;
  // Stream profilers for each stream.
  std::unordered_map<std::string, StreamProfiler> stream_profilers_;
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
