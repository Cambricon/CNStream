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

#include <memory>
#include <string>
#include <utility>

#include "profiler/module_profiler.hpp"
#include "profiler/process_profiler.hpp"
#include "profiler/trace.hpp"

namespace cnstream {

ModuleProfiler::ModuleProfiler(const ProfilerConfig& config,
                               const std::string& module_name,
                               PipelineTracer* tracer)
    : config_(config), module_name_(module_name), tracer_(tracer) {}

bool ModuleProfiler::RegisterProcessName(const std::string& process_name) {
  if (process_profilers_.find(process_name) != process_profilers_.end()) return false;
  process_profilers_.insert(std::make_pair(process_name,
                            std::unique_ptr<ProcessProfiler>(new ProcessProfiler(config_, process_name, tracer_))));
  process_profilers_[process_name]->SetModuleName(module_name_).SetTraceLevel(TraceEvent::Level::MODULE);
  return true;
}

bool ModuleProfiler::RecordProcessStart(const std::string& process_name, const RecordKey& key) {
  ProcessProfiler* process_profiler = GetProcessProfiler(process_name);
  if (!process_profiler) return false;
  process_profiler->RecordStart(key);
  return true;
}

bool ModuleProfiler::RecordProcessEnd(const std::string& process_name, const RecordKey& key) {
  ProcessProfiler* process_profiler = GetProcessProfiler(process_name);
  if (!process_profiler) return false;
  process_profiler->RecordEnd(key);
  return true;
}

void ModuleProfiler::OnStreamEos(const std::string& stream_name) {
  for (auto& it : process_profilers_)
    it.second->OnStreamEos(stream_name);
}

ModuleProfile ModuleProfiler::GetProfile() {
  ModuleProfile profile;
  profile.module_name = GetName();
  for (const auto& it : process_profilers_)
    profile.process_profiles.emplace_back(it.second->GetProfile());
  return profile;
}

ModuleProfile ModuleProfiler::GetProfile(const ModuleTrace& trace) {
  ModuleProfile profile;
  profile.module_name = GetName();
  for (const auto& process_trace : trace) {
    ProcessProfiler* process_profiler = GetProcessProfiler(process_trace.first);
    if (process_profiler)
      profile.process_profiles.emplace_back(process_profiler->GetProfile(process_trace.second));
  }
  return profile;
}

ProcessProfiler* ModuleProfiler::GetProcessProfiler(const std::string& process_name) {
  if (process_profilers_.find(process_name) == process_profilers_.end())
    return nullptr;
  return process_profilers_[process_name].get();
}

}  // namespace cnstream
