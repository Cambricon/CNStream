/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
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

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"
#include "profiler/module_profiler.hpp"
#include "profiler/pipeline_profiler.hpp"

namespace cnstream {

class TestModule : public Module {
 public:
  explicit TestModule(const std::string& name) : Module(name) {}
  int Process(std::shared_ptr<CNFrameInfo>) override { return 0; }
  bool Open(ModuleParamSet params) override { return true; }
  void Close() override { return; }
};  // class TestModule

static std::vector<std::shared_ptr<Module>>
CreateModules() {
  std::vector<std::shared_ptr<Module>> modules = {
    std::make_shared<TestModule>("module1"),
    std::make_shared<TestModule>("module2")
  };
  return modules;
}
static std::vector<std::string> GetModuleNames(std::vector<std::shared_ptr<Module>> modules) {
  std::vector<std::string> module_names;
  module_names.reserve(modules.size());
  for (auto &it : modules) {
    module_names.emplace_back(it->GetName());
  }
  return module_names;
}

TEST(CorePipelineProfiler, GetName) {
  ProfilerConfig config;
  config.enable_tracing = true;
  config.enable_profiling = true;
  const std::string pipeline_name = "pipeline";
  PipelineProfiler profiler(config, pipeline_name, {}, {});
  EXPECT_EQ(profiler.GetName(), pipeline_name);
}

TEST(CorePipelineProfiler, GetTracer) {
  ProfilerConfig config;
  config.enable_tracing = true;
  config.enable_profiling = true;
  const std::string pipeline_name = "pipeline";
  PipelineProfiler profiler(config, pipeline_name, {}, {});
  EXPECT_NE(nullptr, profiler.GetTracer());
}

TEST(CorePipelineProfiler, GetModuleProfiler) {
  ProfilerConfig config;
  config.enable_tracing = true;
  config.enable_profiling = true;
  const std::string pipeline_name = "pipeline";
  const std::string module_name = "module";
  std::vector<std::shared_ptr<Module>> modules;
  modules.push_back(std::shared_ptr<Module>(new TestModule(module_name)));
  PipelineProfiler profiler(config, pipeline_name, modules, {module_name});
  EXPECT_NE(nullptr, profiler.GetModuleProfiler(module_name));
}

TEST(CorePipelineProfiler, GetProfile) {
  ProfilerConfig config;
  config.enable_tracing = true;
  config.enable_profiling = true;
  auto modules = CreateModules();
  PipelineProfiler profiler(config, "test_pipeline", modules, GetModuleNames(modules));
  const std::string stream_name = "stream0";
  Time start_time = Clock::now();
  profiler.RecordInput(std::make_pair(stream_name, 0));
  profiler.RecordInput(std::make_pair(stream_name, 1));
  profiler.RecordOutput(std::make_pair(stream_name, 0));
  profiler.RecordOutput(std::make_pair(stream_name, 1));
  profiler.GetModuleProfiler(modules[0]->GetName())
      ->RecordProcessStart(kPROCESS_PROFILER_NAME, std::make_pair(stream_name, 0));
  profiler.GetModuleProfiler(modules[0]->GetName())
      ->RecordProcessEnd(kPROCESS_PROFILER_NAME, std::make_pair(stream_name, 0));
  PipelineProfile profile = profiler.GetProfile();
  EXPECT_EQ(profile.pipeline_name, "test_pipeline");
  EXPECT_EQ(profile.overall_profile.completed, 2);

  profile = profiler.GetProfile(Time::min(), Time::max());
  EXPECT_EQ(profile.overall_profile.completed, 2);

  profile = profiler.GetProfileBefore(Clock::now(), Duration(1e10));
  EXPECT_EQ(profile.overall_profile.completed, 2);

  profile = profiler.GetProfileAfter(start_time, Duration(1e10));
  EXPECT_EQ(profile.overall_profile.completed, 2);
}

TEST(CorePipelineProfiler, GetProfile_Disable_Tracing) {
  ProfilerConfig config;
  config.enable_tracing = false;
  config.enable_profiling = true;
  auto modules = CreateModules();
  PipelineProfiler profiler(config, "test_pipeline", modules, GetModuleNames(modules));
  const std::string stream_name = "stream0";
  profiler.RecordInput(std::make_pair(stream_name, 0));
  profiler.RecordInput(std::make_pair(stream_name, 1));
  profiler.RecordOutput(std::make_pair(stream_name, 0));
  profiler.RecordOutput(std::make_pair(stream_name, 1));

  PipelineProfile profile = profiler.GetProfile(Time::min(), Time::max());
  EXPECT_EQ(profile.overall_profile.completed, 0);
}

TEST(CorePipelineProfiler, RecordInputOutput) {
  ProfilerConfig config;
  config.enable_tracing = true;
  config.enable_profiling = true;
  auto modules = CreateModules();
  PipelineProfiler profiler(config, "test_pipeline", modules, GetModuleNames(modules));
  const std::string stream_name = "stream0";
  Time start_time = Clock::now();
  profiler.RecordInput(std::make_pair(stream_name, 0));
  profiler.RecordOutput(std::make_pair(stream_name, 0));
  PipelineTrace trace = profiler.GetTracer()->GetTrace(start_time, Clock::now());
  for (const auto& process_trace : trace.process_traces)
    if (process_trace.first == kOVERALL_PROCESS_NAME) {
      EXPECT_EQ(process_trace.second.size(), 2);
      EXPECT_EQ(process_trace.second[0].type, TraceEvent::Type::START);
      EXPECT_EQ(process_trace.second[1].type, TraceEvent::Type::END);
    }
}

TEST(CorePipelineProfiler, OnStreamEos) {
  ProfilerConfig config;
  config.enable_tracing = true;
  config.enable_profiling = true;
  auto modules = CreateModules();
  PipelineProfiler profiler(config, "test_pipeline", modules, GetModuleNames(modules));
  const std::string stream_name = "stream0";
  profiler.RecordInput(std::make_pair(stream_name, 0));
  profiler.RecordOutput(std::make_pair(stream_name, 0));
  PipelineProfile profile = profiler.GetProfile();
  EXPECT_EQ(profile.overall_profile.stream_profiles.size(), 1);
  profiler.OnStreamEos(stream_name);
  profile = profiler.GetProfile();
  EXPECT_EQ(profile.overall_profile.stream_profiles.size(), 0);
}

}  // namespace cnstream
