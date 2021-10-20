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

#ifndef CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PROFILE_HPP_
#define CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PROFILE_HPP_

#include <string>
#include <utility>
#include <vector>

/*!
 *  @file profile.hpp
 *
 *  This file contains the declarations of the StreamProfile, ProcessProfile, ModuleProfile and PipelineProfile struct.
 */
namespace cnstream {

/*!
 * @struct StreamProfile
 *
 * @brief The StreamProfile is a structure describing the performance statistics of streams.
 */
struct StreamProfile {
  std::string stream_name;         /*!< The stream name. */
  uint64_t counter = 0;            /*!< The frame counter, it is equal to ``completed`` plus ``dropped``. */
  uint64_t completed = 0;          /*!< The completed frame counter. */
  int64_t dropped = 0;             /*!< The dropped frame counter. */
  double latency = 0.0;            /*!< The average latency. (unit:ms) */
  double maximum_latency = 0.0;    /*!< The maximum latency. (unit:ms) */
  double minimum_latency = 0.0;    /*!< The minimum latency. (unit:ms) */
  double fps = 0.0;                /*!< The throughput. */

  /*!
   * @brief Constructs a StreamProfile object with default constructor.
   *
   * @return No return value.
   */
  StreamProfile() = default;
  /*!
   * @brief Constructs a StreamProfile object with the copy of the contents of another object.
   *
   * @param[in] it Another object used to initialize an object.
   *
   * @return No return value.
   */
  StreamProfile(const StreamProfile& it) = default;
  /*!
   * @brief Replaces the contents with a copy of the contents of another StreamProfile object.
   *
   * @param[in] it Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  StreamProfile& operator=(const StreamProfile& it) = default;
  /*!
   * @brief Constructs a StreamProfile object with the contents of another object using move semantics.
   *
   * @param[in] it Another object used to initialize an object.
   *
   * @return No return value.
   */
  inline StreamProfile(StreamProfile&& it) {
    *this = std::forward<StreamProfile>(it);
  }
  /*!
   * @brief Replaces the contents with those of another StreamProfile object using move semantics.
   *
   * @param[in] it Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  inline StreamProfile& operator=(StreamProfile&& it) {
    stream_name = std::move(it.stream_name);
    counter = it.counter;
    completed = it.completed;
    dropped = it.dropped;
    latency = it.latency;
    maximum_latency = it.maximum_latency;
    minimum_latency = it.minimum_latency;
    fps = it.fps;
    return *this;
  }
};  // struct StreamProfile

/*!
 * @struct ProcessProfile
 *
 * @brief The ProcessProfile is a structure describing the performance statistics of process.
 */
struct ProcessProfile {
  std::string process_name;                    /*!< The process name. */
  uint64_t counter = 0;                        /*!< The frame counter, it is equal to completed plus dropped frames. */
  uint64_t completed = 0;                      /*!< The completed frame counter. */
  int64_t dropped = 0;                         /*!< The dropped frame counter. */
  int64_t ongoing = 0;                         /*!< The number of frame being processed. */
  double latency = 0.0;                        /*!< The average latency. (unit:ms) */
  double maximum_latency = 0.0;                /*!< The maximum latency. (unit:ms) */
  double minimum_latency = 0.0;                /*!< The minimum latency. (unit:ms) */
  double fps = 0.0;                            /*!< The throughput. */
  std::vector<StreamProfile> stream_profiles;  /*!< The stream profiles. */

  /*!
   * @brief Constructs a ProcessProfile object with default constructor.
   *
   * @return No return value.
   */
  ProcessProfile() = default;
  /*!
   * @brief Constructs a ProcessProfile object with the copy of the contents of another object.
   *
   * @param[in] it Another object used to initialize an object.
   *
   * @return No return value.
   */
  ProcessProfile(const ProcessProfile& it) = default;
  /*!
   * @brief Replaces the contents with a copy of the contents of another ProcessProfile object.
   *
   * @param[in] it Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  ProcessProfile& operator=(const ProcessProfile& it) = default;
  /*!
   * @brief Constructs a ProcessProfile object with the contents of another object using move semantics.
   *
   * @param[in] it Another object used to initialize an object.
   *
   * @return No return value.
   */
  inline ProcessProfile(ProcessProfile&& it) {
    *this = std::forward<ProcessProfile>(it);
  }
  /*!
   * @brief Replaces the contents with those of another ProcessProfile object using move semantics.
   *
   * @param[in] it Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  inline ProcessProfile& operator=(ProcessProfile&& it) {
    process_name = std::move(it.process_name);
    stream_profiles = std::move(it.stream_profiles);
    counter = it.counter;
    completed = it.completed;
    ongoing = it.ongoing;
    dropped = it.dropped;
    latency = it.latency;
    maximum_latency = it.maximum_latency;
    minimum_latency = it.minimum_latency;
    fps = it.fps;
    return *this;
  }
};  // struct ProcessProfile

/*!
 * @struct ModuleProfile
 *
 * @brief The ModuleProfile is a structure describing the performance statistics of module.
 */
struct ModuleProfile {
  std::string module_name;                       /*!< The module name. */
  std::vector<ProcessProfile> process_profiles;  /*!< The process profiles. */

  /*!
   * @brief Constructs a ModuleProfile object with default constructor.
   *
   * @return No return value.
   */
  ModuleProfile() = default;
  /*!
   * @brief Constructs a ModuleProfile object with the copy of the contents of another object.
   *
   * @param[in] it Another object used to initialize an object.
   *
   * @return No return value.
   */
  ModuleProfile(const ModuleProfile& it) = default;
  /*!
   * @brief Replaces the contents with a copy of the contents of another ModuleProfile object.
   *
   * @param[in] it Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  ModuleProfile& operator=(const ModuleProfile& it) = default;
  /*!
   * @brief Constructs a ModuleProfile object with the contents of another object using move semantics.
   *
   * @param[in] it Another object used to initialize an object.
   *
   * @return No return value.
   */
  inline ModuleProfile(ModuleProfile&& it) {
    *this = std::forward<ModuleProfile>(it);
  }
  /*!
   * @brief Replaces the contents with those of another ModuleProfile object using move semantics.
   *
   * @param[in] it Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  inline ModuleProfile& operator=(ModuleProfile&& it) {
    module_name = std::move(it.module_name);
    process_profiles = std::move(it.process_profiles);
    return *this;
  }
};  // struct ModuleProfile

/*!
 * @struct PipelineProfile
 *
 * @brief The PipelineProfile is a structure describing the performance statistics of pipeline.
 */
struct PipelineProfile {
  std::string pipeline_name;                   /*!< The pipeline name. */
  std::vector<ModuleProfile> module_profiles;  /*!< The module profiles. */
  ProcessProfile overall_profile;              /*!< The profile of the whole pipeline. */

  /*!
   * @brief Constructs a PipelineProfile object with default constructor.
   *
   * @return No return value.
   */
  PipelineProfile() = default;
  /*!
   * @brief Constructs a PipelineProfile object with the copy of the contents of another object.
   *
   * @param[in] it Another object used to initialize an object.
   *
   * @return No return value.
   */
  PipelineProfile(const PipelineProfile& it) = default;
  /*!
   * @brief Replaces the contents with a copy of the contents of another PipelineProfile object.
   *
   * @param[in] it Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  PipelineProfile& operator=(const PipelineProfile& it) = default;
  /*!
   * @brief Constructs a PipelineProfile object with the contents of another object using move semantics.
   *
   * @param[in] it Another object used to initialize an object.
   *
   * @return No return value.
   */
  inline PipelineProfile(PipelineProfile&& it) {
    *this = std::forward<PipelineProfile>(it);
  }
  /*!
   * @brief Replaces the contents with those of another PipelineProfile object using move semantics.
   *
   * @param[in] it Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  inline PipelineProfile& operator=(PipelineProfile&& it) {
    pipeline_name = std::move(it.pipeline_name);
    module_profiles = std::move(it.module_profiles);
    overall_profile = std::move(it.overall_profile);
    return *this;
  }
};  // struct PipelineProfile

}  // namespace cnstream

#endif  // CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PROFILE_HPP_
