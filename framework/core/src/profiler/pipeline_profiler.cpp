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

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "cnstream_logging.hpp"
#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"
#include "profiler/module_profiler.hpp"
#include "profiler/pipeline_profiler.hpp"
#include "profiler/pipeline_tracer.hpp"

namespace cnstream {

PipelineProfiler::PipelineProfiler(const ProfilerConfig& config,
                                   const std::string& pipeline_name,
                                   const std::vector<std::shared_ptr<Module>>& modules,
                                   const std::vector<std::string>& sorted_module_names)
    : config_(config), pipeline_name_(pipeline_name), tracer_(new PipelineTracer()),
    sorted_module_names_(sorted_module_names) {
  for (const auto& module : modules) {
    auto name = module->GetName();
    module_profilers_.emplace(name, std::unique_ptr<ModuleProfiler>(new ModuleProfiler(config, name, tracer_.get())));
    module_profilers_[name]->RegisterProcessName(kPROCESS_PROFILER_NAME);
    if (module->GetContainer() && !module->GetContainer()->IsRootNode(name)) {
      module_profilers_[name]->RegisterProcessName(kINPUT_PROFILER_NAME);
    }
  }

  overall_profiler_.reset(new ProcessProfiler(config, kOVERALL_PROCESS_NAME, tracer_.get()));
  overall_profiler_->SetTraceLevel(TraceEvent::Level::PIPELINE);
}

ModuleProfiler* PipelineProfiler::GetModuleProfiler(const std::string& module_name) const {
  if (module_profilers_.find(module_name) != module_profilers_.end())
    return module_profilers_.find(module_name)->second.get();
  return nullptr;
}

PipelineProfile PipelineProfiler::GetProfile() {
  PipelineProfile profile;
  profile.pipeline_name = GetName();
  for (auto& module_name : sorted_module_names_) {
    auto module_profile = module_profilers_.find(module_name);
    if (module_profile != module_profilers_.end()) {
      profile.module_profiles.emplace_back(module_profile->second->GetProfile());
    }
  }
  profile.overall_profile = overall_profiler_->GetProfile();
  return profile;
}

PipelineProfile PipelineProfiler::GetProfile(const Time& start, const Time& end) {
  if (!config_.enable_tracing) {
    LOGW(PROFILER) << "Over time profiling can not use as tracing is disabled.";
    return {};
  }

  auto trace = tracer_->GetTrace(start, end);

  PipelineProfile profile;
  profile.pipeline_name = GetName();
  for (auto& module_name : sorted_module_names_) {
    auto module_trace = trace.module_traces.find(module_name);
    if (trace.module_traces.find(module_name) != trace.module_traces.end()) {
      auto module_profile = module_profilers_.find(module_name);
      if (module_profile != module_profilers_.end()) {
        profile.module_profiles.emplace_back(module_profile->second->GetProfile(module_trace->second));
      }
    }
  }

  for (const auto& pipeline_process_trace : trace.process_traces) {
    if (pipeline_process_trace.first == kOVERALL_PROCESS_NAME) {
      profile.overall_profile = overall_profiler_->GetProfile(pipeline_process_trace.second);
      break;
    }
  }
  return profile;
}

}  // namespace cnstream
