#ifndef CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PIPELINE_PROFILER_HPP_
#define CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PIPELINE_PROFILER_HPP_

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "cnstream_common.hpp"
#include "cnstream_config.hpp"
#include "profiler/process_profiler.hpp"
#include "profiler/profile.hpp"
#include "profiler/trace.hpp"

namespace cnstream {

class Module;
class ModuleProfiler;

static constexpr char kOVERALL_PROCESS_NAME[] = "OVERALL";

/**
 * PipelineProfiler is responsible for the performance statistics of a pipeline.
 * PipelineProfiler contains multiple ModuleProfilers for multiple modules profiling.
 * 
 * By default, it will perform two processes of profiling for all modules.
 * The two processes are named `kPROCESS_PROFILER_NAME` and `kINPUT_PROFILER_NAME`.
 * The process named `kPROCESS_PROFILER_NAME` is started before Module::Process called 
 * and ended before Module::Transmit called.
 * The process named `kINPUT_PROFILER_NAME` is started when datas go into the 
 * data queue of module and ended when datas start to be processed by module.
 * 
 * It also does profiling of the data processing process from entering to exiting the pipeline.
 * 
 * The start and end trace events of each process are recorded when the `config.enable_tracing` is true.
 * 
 * This class is thread-safe.
 **/
class PipelineProfiler : private NonCopyable {
 public:
  /**
   * @brief Constructor of ModuleProfiler.
   * 
   * @param config Profiler config.
   * @param pipeline_name Pipeline name.
   * @param modules modules in the pipeline named `pipeline_name`.
   **/
  PipelineProfiler(const ProfilerConfig& config,
                   const std::string& pipeline_name,
                   const std::vector<std::shared_ptr<Module>>& modules);

  /**
   * @brief Gets name of pipeline.
   **/
  std::string GetName() const;

  /**
   * @brief Gets tracer.
   *
   * @return Returns tracer.
   **/
  PipelineTracer* GetTracer() const;

  /**
   * @brief Gets module profiler by module name.
   * 
   * @param module_name Name of module.
   * 
   * @return Returns module profiler.
   **/
  ModuleProfiler* GetModuleProfiler(const std::string& module_name) const;

  /**
   * @brief Gets profiling results of the whole run time.
   * 
   * @return Returns the profiling results.
   **/
  PipelineProfile GetProfile();

  /**
   * @brief Gets profiling results from `start` to `end`.
   * 
   * @param start Start time.
   * @param end End time.
   * 
   * @return Returns the profiling results.
   **/
  PipelineProfile GetProfile(const Time& start, const Time& end);

  /**
   * @brief Gets profiling results for a specified period time.
   * 
   * @param end End time.
   * @param duration Length of time before `end`.
   * 
   * @return Returns the profiling results.
   **/
  PipelineProfile GetProfileBefore(const Time& end, const Duration& duration);

  /**
   * @brief Gets profiling results for a specified period time.
   * 
   * @param start Start time.
   * @param duration Length of time after `start`.
   * 
   * @return Returns the profiling results.
   **/
  PipelineProfile GetProfileAfter(const Time& start, const Duration& duration);

  /**
   * @brief Record the time when the data enters the pipeline.
   * 
   * @param key Unique identifier of a CNFrameInfo instance.
   * 
   * @return void.
   *
   * @see RecordKey
   **/
  void RecordInput(const RecordKey& key);

  /**
   * @brief Record the time when the data exits the pipeline.
   * 
   * @param key Unique identifier of a CNFrameInfo instance.
   * 
   * @return void.
   *
   * @see RecordKey
   **/
  void RecordOutput(const RecordKey& key);

  /**
   * @brief Tells the profiler to clear datas of stream named by `stream_name`.
   * 
   * @param stream_name Stream name. Usually it is comes from `CNFrameInfo::stream_id`.
   * 
   * @return void.
   **/
  void OnStreamEos(const std::string& stream_name);

 private:
  ProfilerConfig config_;
  std::string pipeline_name_;
  std::unordered_map<std::string, std::unique_ptr<ModuleProfiler>> module_profilers_;
  std::unique_ptr<ProcessProfiler> overall_profiler_;
  std::unique_ptr<PipelineTracer> tracer_;
};  // class PipelineProfiler

inline std::string PipelineProfiler::GetName() const {
  return pipeline_name_;
}

inline PipelineTracer* PipelineProfiler::GetTracer() const {
  return tracer_.get();
}

inline PipelineProfile PipelineProfiler::GetProfileBefore(const Time& end, const Duration& duration) {
  return GetProfile(std::chrono::time_point_cast<Clock::duration>(end - duration), end);
}

inline PipelineProfile PipelineProfiler::GetProfileAfter(const Time& start, const Duration& duration) {
  return GetProfile(start, std::chrono::time_point_cast<Clock::duration>(start + duration));
}

inline void PipelineProfiler::RecordInput(const RecordKey& key) {
  overall_profiler_->RecordStart(key);
}

inline void PipelineProfiler::RecordOutput(const RecordKey& key) {
  overall_profiler_->RecordEnd(key);
}

inline void PipelineProfiler::OnStreamEos(const std::string& stream_name) {
  overall_profiler_->OnStreamEos(stream_name);
}

}  // namespace cnstream

#endif  // CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PIPELINE_PROFILER_HPP_
