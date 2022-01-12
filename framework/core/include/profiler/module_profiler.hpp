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

#ifndef CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_MODULE_PROFILER_HPP_
#define CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_MODULE_PROFILER_HPP_

#include <memory>
#include <string>
#include <map>

#include "cnstream_common.hpp"
#include "cnstream_config.hpp"
#include "profiler/process_profiler.hpp"
#include "profiler/profile.hpp"
#include "profiler/trace.hpp"

/*!
 *  @file module_profiler.hpp
 *
 *  This file contains a declaration of the ModuleProfiler class.
 */
namespace cnstream {

static constexpr char kPROCESS_PROFILER_NAME[] = "PROCESS";
static constexpr char kINPUT_PROFILER_NAME[]   = "INPUT_QUEUE";

class PipelineTracer;

/*!
 * @class ModuleProfiler
 *
 * @brief ModuleProfiler is a class of the performance statistics of a module. It contains multiple
 * cnstream::ProcessProfiler instances to support multiple process profilings.
 *
 * The trace events of each process will be recorded when ProfilerConfig::enable_tracing is true. Profiling and tracing of customized process is supported. See ModuleProfiler::RegisterProcessName for details.
 *
 * @note This class is thread safe.
 */
class ModuleProfiler : private NonCopyable {
 public:
  /*!
   * @brief Constructs a ModuleProfiler object.
   *
   * @param[in] config The configuration of the profiler.
   * @param[in] module_name The name of the module.
   * @param[in] tracer The tracer for tracing events.
   *
   * @return No return value.
   */
  explicit ModuleProfiler(const ProfilerConfig& config,
                          const std::string& module_name,
                          PipelineTracer* tracer);

  /*!
   * @brief Registers process named by ``process_name`` for this profiler.
   *
   * @param[in] process_name The process name is the unique identification of a function or a piece of code that needs
   *                         to do profiling.
   *
   * @return Returns true if the registration is successful. Returns false if the process name has been registered.
   */
  bool RegisterProcessName(const std::string& process_name);

  /*!
   * @brief Records the start of a process named ``process_name``.
   *
   * @param[in] process_name The name of the process. It should be registed by ``RegisterProcessName``.
   * @param[in] key The unique identifier of a CNFrameInfo instance.
   *
   * @return Returns true if recording is successful. Returns false if the process named by ``process_name`` is not
   *         registered by ``RegisterProcessName``.
   *
   * @see cnstream::ModuleProfiler::RegisterProcessName
   * @see cnstream::ModuleProfiler::RecordKey
   */
  bool RecordProcessStart(const std::string& process_name, const RecordKey& key);

  /*!
   * @brief Records the end of a process named ``process_name``.
   *
   * @param[in] process_name The name of the process. It should be registed by ``RegisterProcessName``.
   * @param[in] key The unique identifier of a CNFrameInfo instance.
   *
   * @return Returns true if record successfully. Returns false if the process named by ``process_name`` has not been
   *         registered by ``RegisterProcessName``.
   *
   * @see cnstream::ModuleProfiler::RegisterProcessName
   * @see cnstream::ModuleProfiler::RecordKey
   */
  bool RecordProcessEnd(const std::string& process_name, const RecordKey& key);

  /*!
   * @brief Clears profiling data of the stream named by ``stream_name``, as the end of the stream is reached.
   *
   * @param[in] stream_name The name of the stream, usually the ``CNFrameInfo::stream_id``.
   *
   * @return No return value.
   */
  void OnStreamEos(const std::string& stream_name);

  /*!
   * @brief Gets the name of the module.
   *
   * @return Returns the name of the module.
   */
  std::string GetName() const;

  /*!
   * @brief Gets profiling results of the module during the execution of the program.
   *
   * @return Returns the profiling results.
   */
  ModuleProfile GetProfile();

  /*!
   * @brief Gets profiling results according to the trace data.
   *
   * @param[in] trace Gets profiling results according to the trace data.
   *
   * @return Returns the profiling results.
   */
  ModuleProfile GetProfile(const ModuleTrace& trace);

 private:
  // Gets process profiler by ``process_name``.
  ProcessProfiler* GetProcessProfiler(const std::string& process_name);

 private:
  ProfilerConfig config_;
  std::string module_name_ = "";
  PipelineTracer* tracer_ = nullptr;
  std::map<std::string, std::unique_ptr<ProcessProfiler>> process_profilers_;
};  // class ModuleProfiler

inline std::string ModuleProfiler::GetName() const {
  return module_name_;
}

}  // namespace cnstream

#endif  // CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_MODULE_PROFILER_HPP_
