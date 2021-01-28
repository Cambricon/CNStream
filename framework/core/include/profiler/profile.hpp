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

namespace cnstream {

// Performance statistics of stream.
struct StreamProfile {
  std::string stream_name;         ///< stream name.
  uint64_t counter = 0;            ///< frame counter, it is equal to `completed` plus `dropped`.
  uint64_t completed = 0;          ///< completed frame counter.
  int64_t dropped = 0;             ///< dropped frame counter.
  double latency = 0.0;            ///< average latency. (ms)
  double maximum_latency = 0.0;    ///< maximum latency. (ms)
  double minimum_latency = 0.0;    ///< minimum latency. (ms)
  double fps = 0.0;                ///< fps.

  StreamProfile() = default;
  StreamProfile(const StreamProfile& it) = default;
  StreamProfile& operator=(const StreamProfile& it) = default;
  inline StreamProfile(StreamProfile&& it) {
    *this = std::forward<StreamProfile>(it);
  }
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

// Performance statistics of process.
struct ProcessProfile {
  std::string process_name;                      ///< process name.
  uint64_t counter = 0;                          ///< frame counter, it is equal to `completed` plus `dropped`.
  uint64_t completed = 0;                        ///< completed frame counter.
  int64_t dropped = 0;                           ///< dropped frame counter.
  int64_t ongoing = 0;                           ///< number of frame being processed.
  double latency = 0.0;                          ///< average latency. (ms)
  double maximum_latency = 0.0;                  ///< maximum latency. (ms)
  double minimum_latency = 0.0;                  ///< minimum latency. (ms)
  double fps = 0.0;                              ///< fps.
  std::vector<StreamProfile> stream_profiles;    ///< stream profiles.

  ProcessProfile() = default;
  ProcessProfile(const ProcessProfile& it) = default;
  ProcessProfile& operator=(const ProcessProfile& it) = default;
  inline ProcessProfile(ProcessProfile&& it) {
    *this = std::forward<ProcessProfile>(it);
  }
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

// Performance statistics of module.
struct ModuleProfile {
  std::string module_name;                         ///< module name.
  std::vector<ProcessProfile> process_profiles;    ///< process profiles.

  ModuleProfile() = default;
  ModuleProfile(const ModuleProfile& it) = default;
  ModuleProfile& operator=(const ModuleProfile& it) = default;
  inline ModuleProfile(ModuleProfile&& it) {
    *this = std::forward<ModuleProfile>(it);
  }
  inline ModuleProfile& operator=(ModuleProfile&& it) {
    module_name = std::move(it.module_name);
    process_profiles = std::move(it.process_profiles);
    return *this;
  }
};  // struct ModuleProfile

// Performance statistics of pipeline.
struct PipelineProfile {
  std::string pipeline_name;                       ///< pipeline name.
  std::vector<ModuleProfile> module_profiles;      ///< module profiles.
  ProcessProfile overall_profile;                  ///< profile of the whole pipeline.

  PipelineProfile() = default;
  PipelineProfile(const PipelineProfile& it) = default;
  PipelineProfile& operator=(const PipelineProfile& it) = default;
  inline PipelineProfile(PipelineProfile&& it) {
    *this = std::forward<PipelineProfile>(it);
  }
  inline PipelineProfile& operator=(PipelineProfile&& it) {
    pipeline_name = std::move(it.pipeline_name);
    module_profiles = std::move(it.module_profiles);
    overall_profile = std::move(it.overall_profile);
    return *this;
  }
};  // struct PipelineProfile

}  // namespace cnstream

#endif  // CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_PROFILE_HPP_
