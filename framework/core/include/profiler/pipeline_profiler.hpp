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

#ifndef CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PIPELINE_PROFILER_HPP_
#define CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PIPELINE_PROFILER_HPP_

#include <memory>
#include <string>
#include <vector>
#include <map>

#include "cnstream_common.hpp"
#include "cnstream_config.hpp"
#include "profiler/process_profiler.hpp"
#include "profiler/profile.hpp"
#include "profiler/trace.hpp"

/*!
 *  @file pipeline_profiler.hpp
 *
 *  This file contains a declaration of the PipelineProfiler class.
 */
namespace cnstream {

class Module;
class ModuleProfiler;

static constexpr char kOVERALL_PROCESS_NAME[] = "OVERALL";

/*!
 * @class PipelineProfiler
 *
 * @brief PipelineProfiler is responsible for the performance statistics of a pipeline. It contains multiple
 * cnstream::ModuleProfiler instances to support multiple module profilings.
 *
 * By default, it will perform profiling of two processes for all modules. They are named ``kPROCESS_PROFILER_NAME``
 * and ``kINPUT_PROFILER_NAME``. The start of the first process is before cnstream::Module::Process being called, and
 * the end is before cnstream::Module::Transmit being called. The time when data is pushed into the data queue of the
 * module is the start of the second process and the end is when data starts to be processed by the module.
 *
 * It also does profiling of the data processing process from entering to exiting the pipeline.
 *
 * The start and end trace events of each process are recorded when the ``config.enable_tracing`` is true.
 *
 * @note This class is thread safe.
 */
class PipelineProfiler : private NonCopyable {
 public:
  /*!
   * @brief Constructs a PipelineProfiler object.
   *
   * @param[in] config The configuration of the profiler.
   * @param[in] pipeline_name The name of the pipeline.
   * @param[in] modules All modules of the pipeline named ``pipeline_name``.
   *
   * @return No return value.
   */
  PipelineProfiler(const ProfilerConfig& config,
                   const std::string& pipeline_name,
                   const std::vector<std::shared_ptr<Module>>& modules,
                   const std::vector<std::string>& sorted_module_names);

  /*!
   * @brief Gets the name of the pipeline.
   *
   * @return Returns the name of the pipeline.
   */
  std::string GetName() const;

  /**
   * @brief Gets profiler configuration.
   *
   * @return Returns profiler configuration.
   **/
  ProfilerConfig GetConfig() const;

  /**
   * @brief Gets tracer.
   *
   * @return Returns the tracer of the pipeline.
   */
  PipelineTracer* GetTracer() const;

  /*!
   * @brief Gets the module profiler by the name of the module.
   *
   * @param[in] module_name The name of the module.
   *
   * @return Returns the module profiler.
   */
  ModuleProfiler* GetModuleProfiler(const std::string& module_name) const;

  /*!
   * @brief Gets profiling results of the pipeline during the execution of the program.
   *
   * @return Returns the profiling results.
   */
  PipelineProfile GetProfile();

  /*!
   * @brief Gets profiling results between the start time and the end time.
   *
   * @param[in] start The start time.
   * @param[in] end The end time.
   *
   * @return Returns the profiling results.
   */
  PipelineProfile GetProfile(const Time& start, const Time& end);

  /*!
   * @brief Gets profiling results during a specified period time.
   *
   * @param[in] end The end time.
   * @param[in] duration The duration in milliseconds. The start time is the end time minus duration.
   *
   * @return Returns the profiling results.
   */
  PipelineProfile GetProfileBefore(const Time& end, const Duration& duration);

  /*!
   * @brief Gets profiling results for a specified period time.
   *
   * @param[in] start The start time.
   * @param[in] duration The duration in milliseconds. The end time is the start time plus duration.
   *
   * @return Returns the profiling results.
   */
  PipelineProfile GetProfileAfter(const Time& start, const Duration& duration);

  /*!
   * @brief Records the time when the data enters the pipeline.
   *
   * @param[in] key The unique identifier of a CNFrameInfo instance.
   *
   * @return No return value.
   *
   * @see cnstream::RecordKey
   */
  void RecordInput(const RecordKey& key);

  /*!
   * @brief Records the time when the data exits the pipeline.
   *
   * @param[in] key The unique identifier of a CNFrameInfo instance.
   *
   * @return No return value.
   *
   * @see cnstream::RecordKey
   */
  void RecordOutput(const RecordKey& key);

  /*!
   * @brief Clears profiling data of the stream named by ``stream_name``, as the end of the stream is reached.
   *
   * @param[in] stream_name The name of the stream, usually the ``CNFrameInfo::stream_id``.
   *
   * @return No return value.
   */
  void OnStreamEos(const std::string& stream_name);

 private:
  ProfilerConfig config_;
  std::string pipeline_name_;
  std::map<std::string, std::unique_ptr<ModuleProfiler>> module_profilers_;
  std::unique_ptr<ProcessProfiler> overall_profiler_;
  std::unique_ptr<PipelineTracer> tracer_;
  std::vector<std::string> sorted_module_names_;
};  // class PipelineProfiler

inline std::string PipelineProfiler::GetName() const {
  return pipeline_name_;
}

inline ProfilerConfig PipelineProfiler::GetConfig() const {
  return config_;
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
