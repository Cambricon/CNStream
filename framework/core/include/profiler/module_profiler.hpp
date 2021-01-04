#ifndef CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_MODULE_PROFILER_HPP_
#define CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_MODULE_PROFILER_HPP_

#include <memory>
#include <string>
#include <unordered_map>

#include "cnstream_common.hpp"
#include "cnstream_config.hpp"
#include "profiler/process_profiler.hpp"
#include "profiler/profile.hpp"
#include "profiler/trace.hpp"

namespace cnstream {

static constexpr char kPROCESS_PROFILER_NAME[] = "PROCESS";
static constexpr char kINPUT_PROFILER_NAME[]   = "INPUT_QUEUE";

class PipelineTracer;

/**
 * ModuleProfiler is responsible for the performance statistics of a module.
 * ModuleProfiler contains multiple ProcessProfilers for multiple process proiling.
 * The trace event of the processes will be recorded when `ProfilerConfig::enable_tracing` is true.
 * Profiling and tracing of custom process is supported, see `RegisterProcessName` for detail.
 * This class is thread-safe.
 **/
class ModuleProfiler : private NonCopyable {
 public:
  /**
   * @brief Constructor of ModuleProfiler.
   * 
   * @param config Profiler config.
   * @param module_name Module name.
   * @param tracer Tool for tracing.
   **/
  explicit ModuleProfiler(const ProfilerConfig& config,
                          const std::string& module_name,
                          PipelineTracer* tracer);

  /**
   * @brief Registers process named by `process_name` for this profiler.
   *
   * @param process_name The process name is the unique identification of a
   * function or a piece of code that needs to do profiling.
   *
   * @return True for Register succeessed.
   * False will be returned when the process named by `process_name` has already been registered.
   **/
  bool RegisterProcessName(const std::string& process_name);

  /**
   * @brief Records the start of a process named `process_name`.
   * 
   * @param process_name The name of a process. process_name is registed by `RegisterProcessName`.
   * @param key Unique identifier of a CNFrameInfo instance.
   * 
   * @return Ture for record successed.
   * False will be returned when the process named by `process_name` has not been registered by `RegisterProcessName`.
   * 
   * @see RegisterProcessName
   * @see RecordKey
   **/
  bool RecordProcessStart(const std::string& process_name, const RecordKey& key);

  /**
   * @brief Records the end of a process named `process_name`.
   * 
   * @param process_name The name of a process. process_name is registed by `RegisterProcessName`.
   * @param key Unique identifier of a CNFrameInfo instance.
   * 
   * @return Ture for record successed.
   * False will be returned when the process named by `process_name` has not been registered by `RegisterProcessName`.
   * 
   * @see RegisterProcessName
   * @see RecordKey
   **/
  bool RecordProcessEnd(const std::string& process_name, const RecordKey& key);

  /**
   * @brief Tells the profiler to clear datas of stream named by `stream_name`.
   * 
   * @param stream_name Stream name. Usually it is comes from `CNFrameInfo::stream_id`.
   * 
   * @return void.
   **/
  void OnStreamEos(const std::string& stream_name);

  /**
   * @brief Gets name of module.
   **/
  std::string GetName() const;

  /**
   * @brief Gets profiling results of the whole run time.
   * 
   * @return Returns the profiling results.
   **/
  ModuleProfile GetProfile();

  /**
   * @brief Gets profiling results according to the trace datas.
   * 
   * @param trace Trace datas.
   * 
   * @return Returns the profiling results.
   **/
  ModuleProfile GetProfile(const ModuleTrace& trace);

 private:
  // Gets process profiler by `process_name`.
  ProcessProfiler* GetProcessProfiler(const std::string& process_name);

 private:
  ProfilerConfig config_;
  std::string module_name_ = "";
  PipelineTracer* tracer_ = nullptr;
  std::unordered_map<std::string, std::unique_ptr<ProcessProfiler>> process_profilers_;
};  // class ModuleProfiler

inline std::string ModuleProfiler::GetName() const {
  return module_name_;
}

}  // namespace cnstream

#endif  // CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_MODULE_PROFILER_HPP_
