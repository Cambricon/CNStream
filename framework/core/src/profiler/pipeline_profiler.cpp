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
                                   const std::vector<std::shared_ptr<Module>>& modules)
    : config_(config), pipeline_name_(pipeline_name), tracer_(new PipelineTracer()) {
  for (const auto& module : modules) {
    auto name = module->GetName();
    module_profilers_.emplace(name, std::unique_ptr<ModuleProfiler>(new ModuleProfiler(config, name, tracer_.get())));
    module_profilers_[name]->RegisterProcessName(kPROCESS_PROFILER_NAME);
    if (module->GetContainer() && !module->GetContainer()->IsRootNode(name)) {
      module_profilers_[name]->RegisterProcessName(kINPUT_PROFILER_NAME);
    }
  }

  overall_profiler_.reset(new ProcessProfiler(config, kOVERALL_PROCESS_NAME, tracer_.get()));
  overall_profiler_->SetTraceLevel(TraceEvent::PIPELINE);
}

ModuleProfiler* PipelineProfiler::GetModuleProfiler(const std::string& module_name) const {
  if (module_profilers_.find(module_name) != module_profilers_.end())
    return module_profilers_.find(module_name)->second.get();
  return nullptr;
}

PipelineProfile PipelineProfiler::GetProfile() {
  PipelineProfile profile;
  profile.pipeline_name = GetName();
  for (auto& it : module_profilers_) {
    profile.module_profiles.emplace_back(it.second->GetProfile());
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
  for (const auto& module_trace : trace.module_traces) {
    auto module_profiler = module_profilers_.find(module_trace.first);
    if (module_profiler != module_profilers_.end())
      profile.module_profiles.emplace_back(module_profiler->second->GetProfile(module_trace.second));
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
