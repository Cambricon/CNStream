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

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_frame.hpp"
#include "cnstream_pipeline.hpp"
#include "test_base.hpp"

static constexpr char kCNDataFrameTag[] = "CNDataFrame";

namespace cnstream {

class TPTestModule : public Module, public ModuleCreator<TPTestModule> {
 public:
  explicit TPTestModule(const std::string& name) : Module(name) {}
  bool Open(ModuleParamSet params) override {return true;}
  void Close() override {}
  int Process(std::shared_ptr<CNFrameInfo> frame_info) override {return 0;}
};  // class TPTestModule

class TPTestStreamMsgObserver : public StreamMsgObserver {
 public:
  void Update(const StreamMsg& msg) override {}
};  // class TPTestStreamMsgObserver

TEST(CorePipeline, GetName) {
  Pipeline pipeline("test_pipeline");
  EXPECT_EQ("test_pipeline", pipeline.GetName());
}

TEST(CorePipeline, BuildPipelineByModuleConfig) {
  // case1: right configs
  CNModuleConfig config1;
  config1.name = "modulea";
  config1.className = "cnstream::TPTestModule";
  config1.parallelism = 1;
  config1.maxInputQueueSize = 20;
  config1.next = {"moduleb"};
  CNModuleConfig config2;
  config2.name = "moduleb";
  config2.className = "cnstream::TPTestModule";
  config2.parallelism = 1;
  config2.maxInputQueueSize = 20;
  Pipeline pipeline("test_pipeline");
  EXPECT_TRUE(pipeline.BuildPipeline({config1, config2}));
  // case2: wrong configs
  config2.className = "wrong_class_name";
  EXPECT_FALSE(pipeline.BuildPipeline({config1, config2}));
}

TEST(CorePipeline, BuildPipelineByGraphConfig) {
  // case1: right graph config
  CNModuleConfig config1;
  config1.name = "modulea";
  config1.className = "cnstream::TPTestModule";
  config1.parallelism = 1;
  config1.maxInputQueueSize = 20;
  config1.next = {"moduleb"};
  CNModuleConfig config2;
  config2.name = "moduleb";
  config2.className = "cnstream::TPTestModule";
  config2.parallelism = 1;
  config2.maxInputQueueSize = 20;
  CNGraphConfig graph_config;
  graph_config.module_configs = {config1, config2};
  Pipeline pipeline("test_pipeline");
  EXPECT_TRUE(pipeline.BuildPipeline(graph_config));
  // case2: wrong graph config(duplicated module name)
  graph_config.module_configs[1].name = "modulea";
  EXPECT_FALSE(pipeline.BuildPipeline(graph_config));
  // case3: create modules failed(wrong class name)
  graph_config.module_configs[1].name = "moduleb";
  graph_config.module_configs[1].className = "wrong_class_name";
  EXPECT_FALSE(pipeline.BuildPipeline(graph_config));
  // case4: parallelism is zero
  graph_config.module_configs[1] = config2;
  graph_config.module_configs[1].parallelism = 0;
  EXPECT_FALSE(pipeline.BuildPipeline(graph_config));
  // case4: max_input_queue_size is zero
  graph_config.module_configs[1] = config2;
  graph_config.module_configs[1].maxInputQueueSize = 0;
  EXPECT_FALSE(pipeline.BuildPipeline(graph_config));
}

TEST(CorePipeline, BuildPipelineByJSONFile) {
  // case1: right graph config
  std::pair<int, std::string> temp_file_desc = CreateTempFile("test_buildpipeline_config");
  std::string config_str = "{\n"
      "\"modulea\" : {\n"
         "\"class_name\" : \"cnstream::TPTestModule\",\n"
         "\"parallelism\" : 1,\n"
         "\"max_input_queue_size\" : 20,\n"
         "\"next_modules\" : [\"moduleb\"]\n"
      "},\n"
      "\"moduleb\" : {\n"
         "\"class_name\" : \"cnstream::TPTestModule\",\n"
         "\"parallelism\" : 1,\n"
         "\"max_input_queue_size\" : 20\n"
      "}\n"
    "}\n";
  EXPECT_EQ(config_str.size(), write(temp_file_desc.first, config_str.c_str(), config_str.size()))
      << "Write cofnig str to temp file for BuildPipelineByJSONFile test case failed! "
      << strerror(errno);
  Pipeline pipeline("test_pipeline");
  EXPECT_TRUE(pipeline.BuildPipelineByJSONFile(temp_file_desc.second));
  // case2: wrong json format
  config_str[config_str.size() - 2] = ',';
  EXPECT_NE(-1, ftruncate(temp_file_desc.first, 0)) << "Clear temp file content failed. "
      << strerror(errno);
  EXPECT_EQ(config_str.size(), write(temp_file_desc.first, config_str.c_str(), config_str.size()))
      << "Write cofnig str to temp file for BuildPipelineByJSONFile test case failed! "
      << strerror(errno);
  EXPECT_FALSE(pipeline.BuildPipelineByJSONFile(temp_file_desc.second));
  // case3: wrong graph config
  config_str = "{\n"
      "\"modulea\" : {\n"
         "\"class_name\" : \"wrong_class_name\",\n"
         "\"parallelism\" : 1,\n"
         "\"max_input_queue_size\" : 20\n"
      "}\n"
    "}\n";
  EXPECT_NE(-1, ftruncate(temp_file_desc.first, 0)) << "Clear temp file content failed. "
      << strerror(errno);
  EXPECT_EQ(config_str.size(), write(temp_file_desc.first, config_str.c_str(), config_str.size()))
      << "Write cofnig str to temp file for BuildPipelineByJSONFile test case failed! "
      << strerror(errno);
  EXPECT_FALSE(pipeline.BuildPipelineByJSONFile(temp_file_desc.second));

  // remove temp file
  close(temp_file_desc.first);
  unlink(temp_file_desc.second.c_str());
}

namespace __test_module_open_failed__ {
class TestModuleOpenFailed : public Module, public ModuleCreator<TestModuleOpenFailed> {
 public:
  explicit TestModuleOpenFailed(const std::string& name) : Module(name) {}
  bool Open(ModuleParamSet params) override {return false;}
  void Close() override {}
  int Process(std::shared_ptr<CNFrameInfo> frame_info) override {return 0;}
};  // class TestModuleOpenFailed
}  // namespace __test_module_open_failed__

TEST(CorePipeline, Start) {
  // case1: start twice
  Pipeline pipeline("test_pipeline");
  CNModuleConfig config1;
  config1.name = "modulea";
  config1.className = "cnstream::TPTestModule";
  config1.parallelism = 1;
  config1.maxInputQueueSize = 20;
  config1.next = {"moduleb"};
  CNModuleConfig config2;
  config2.name = "moduleb";
  config2.className = "cnstream::TPTestModule";
  config2.parallelism = 1;
  config2.maxInputQueueSize = 20;
  CNGraphConfig graph_config;
  graph_config.module_configs = {config1, config2};
  EXPECT_TRUE(pipeline.BuildPipeline(graph_config));
  EXPECT_TRUE(pipeline.Start());
  EXPECT_FALSE(pipeline.Start());
  EXPECT_TRUE(pipeline.IsRunning());
  pipeline.Stop();
  // case2: open module failed.
  graph_config.module_configs[1].className = "cnstream::__test_module_open_failed__::TestModuleOpenFailed";
  EXPECT_TRUE(pipeline.BuildPipeline(graph_config));
  EXPECT_FALSE(pipeline.Start());
  EXPECT_FALSE(pipeline.IsRunning());
}

TEST(CorePipeline, Stop) {
  // case1: stop before start
  Pipeline pipeline("test_pipeline");
  CNModuleConfig config1;
  config1.name = "modulea";
  config1.className = "cnstream::TPTestModule";
  config1.parallelism = 1;
  config1.maxInputQueueSize = 20;
  config1.next = {"moduleb"};
  CNModuleConfig config2;
  config2.name = "moduleb";
  config2.className = "cnstream::TPTestModule";
  config2.parallelism = 1;
  config2.maxInputQueueSize = 20;
  CNGraphConfig graph_config;
  graph_config.module_configs = {config1, config2};
  EXPECT_TRUE(pipeline.BuildPipeline(graph_config));
  EXPECT_TRUE(pipeline.Stop());
  // case2: stop success
  EXPECT_TRUE(pipeline.Start());
  EXPECT_TRUE(pipeline.Stop());
}

TEST(CorePipeline, IsRunning) {
  // case1: before start, not running
  Pipeline pipeline("test_pipeline");
  EXPECT_FALSE(pipeline.IsRunning());
  // case2: after start, running
  EXPECT_TRUE(pipeline.Start());
  EXPECT_TRUE(pipeline.IsRunning());
  // case3: after stop, not running
  EXPECT_TRUE(pipeline.Stop());
  EXPECT_FALSE(pipeline.IsRunning());
}

TEST(CorePipeline, GetModule) {
  Pipeline pipeline("test_pipeline");
  CNModuleConfig config;
  config.name = "modulea";
  config.className = "cnstream::TPTestModule";
  config.parallelism = 1;
  config.maxInputQueueSize = 20;
  CNGraphConfig graph_config;
  graph_config.module_configs = {config};
  EXPECT_TRUE(pipeline.BuildPipeline(graph_config));
  // case1: right module name
  auto module = pipeline.GetModule("modulea");
  EXPECT_NE(nullptr, module);
  // case1: wrong module name
  module = pipeline.GetModule("wrong_module_name");
  EXPECT_EQ(nullptr, module);
}

TEST(CorePipeline, GetModuleConfig) {
  Pipeline pipeline("test_pipeline");
  CNModuleConfig config;
  config.name = "modulea";
  config.className = "cnstream::TPTestModule";
  config.parallelism = 1;
  config.maxInputQueueSize = 20;
  CNGraphConfig graph_config;
  graph_config.module_configs = {config};
  EXPECT_TRUE(pipeline.BuildPipeline(graph_config));
  // case1: right module name
  auto module_config = pipeline.GetModuleConfig("modulea");
  EXPECT_EQ(module_config.name, config.name);
  // case1: wrong module name
  module_config = pipeline.GetModuleConfig("wrong_module_name");
  EXPECT_TRUE(module_config.name.empty());
}

TEST(CorePipeline, IsProfilingEnabled) {
  // case1: true
  Pipeline pipeline("test_pipeline");
  ProfilerConfig profiler_config;
  profiler_config.enable_profiling = true;
  EXPECT_TRUE(pipeline.BuildPipeline({}, profiler_config));
  EXPECT_TRUE(pipeline.IsProfilingEnabled());
  // case1: false
  profiler_config.enable_profiling = false;
  EXPECT_TRUE(pipeline.BuildPipeline({}, profiler_config));
  EXPECT_FALSE(pipeline.IsProfilingEnabled());
}

TEST(CorePipeline, IsTracingEnabled) {
  // case1: true
  Pipeline pipeline("test_pipeline");
  ProfilerConfig profiler_config;
  profiler_config.enable_tracing = true;
  EXPECT_TRUE(pipeline.BuildPipeline({}, profiler_config));
  EXPECT_TRUE(pipeline.IsTracingEnabled());
  // case1: false
  profiler_config.enable_tracing = false;
  EXPECT_TRUE(pipeline.BuildPipeline({}, profiler_config));
  EXPECT_FALSE(pipeline.IsTracingEnabled());
}

TEST(CorePipeline, ProvideData) {
  Pipeline pipeline("test_pipeline");
  CNModuleConfig config1;
  config1.name = "modulea";
  config1.className = "cnstream::TPTestModule";
  config1.parallelism = 1;
  config1.maxInputQueueSize = 20;
  config1.next = {"moduleb"};
  CNModuleConfig config2;
  config2.name = "moduleb";
  config2.className = "cnstream::TPTestModule";
  config2.parallelism = 1;
  config2.maxInputQueueSize = 20;
  CNGraphConfig graph_config;
  graph_config.module_configs = {config1, config2};
  EXPECT_TRUE(pipeline.BuildPipeline(graph_config));
  auto module = pipeline.GetModule("modulea");
  auto data = CNFrameInfo::Create("1");
  // case1: provide data before pipeline running
  EXPECT_FALSE(pipeline.ProvideData(module, data));
  EXPECT_TRUE(pipeline.Start());
  // case2: provide data with an invalid module
  EXPECT_FALSE(pipeline.ProvideData(nullptr, data));
  // case3: provide data with an module not created by current pipeline
  TPTestModule orphan("orphan");
  EXPECT_FALSE(pipeline.ProvideData(&orphan, data));
  // case4: provide data with an module which is not a root node
  EXPECT_FALSE(pipeline.ProvideData(pipeline.GetModule("moduleb"), data));
  // case5: provide success
  EXPECT_TRUE(pipeline.ProvideData(module, data));
  pipeline.Stop();
}

TEST(CorePipeline, GetEventBus) {
  Pipeline pipeline("test_pipeline");
  EXPECT_NE(nullptr, pipeline.GetEventBus());
}

TEST(CorePipeline, SetStreamMsgObserver) {
  Pipeline pipeline("test_pipeline");
  TPTestStreamMsgObserver observer;
  pipeline.SetStreamMsgObserver(&observer);
  EXPECT_EQ(&observer, pipeline.GetStreamMsgObserver());
}

TEST(CorePipeline, GetStreamMsgObserver) {
  Pipeline pipeline("test_pipeline");
  TPTestStreamMsgObserver observer;
  EXPECT_EQ(nullptr, pipeline.GetStreamMsgObserver());
  pipeline.SetStreamMsgObserver(&observer);
  EXPECT_EQ(&observer, pipeline.GetStreamMsgObserver());
}

TEST(CorePipeline, GetProfiler) {
  Pipeline pipeline("test_pipeline");
  EXPECT_EQ(nullptr, pipeline.GetProfiler());
  ProfilerConfig profiler_config;
  profiler_config.enable_profiling = false;
  pipeline.BuildPipeline(std::vector<CNModuleConfig>(), profiler_config);
  EXPECT_EQ(nullptr, pipeline.GetProfiler());
  profiler_config.enable_profiling = true;
  pipeline.BuildPipeline(std::vector<CNModuleConfig>(), profiler_config);
  EXPECT_NE(nullptr, pipeline.GetProfiler());
}

TEST(CorePipeline, GetTracer) {
  Pipeline pipeline("test_pipeline");
  EXPECT_EQ(nullptr, pipeline.GetTracer());
  ProfilerConfig profiler_config;
  profiler_config.enable_tracing = false;
  pipeline.BuildPipeline(std::vector<CNModuleConfig>(), profiler_config);
  EXPECT_EQ(nullptr, pipeline.GetTracer());
  profiler_config.enable_tracing = true;
  pipeline.BuildPipeline(std::vector<CNModuleConfig>(), profiler_config);
  EXPECT_NE(nullptr, pipeline.GetTracer());
}

TEST(CorePipeline, IsRootNode) {
  Pipeline pipeline("test_pipeline");
  CNModuleConfig config1;
  config1.name = "modulea";
  config1.className = "cnstream::TPTestModule";
  config1.parallelism = 1;
  config1.maxInputQueueSize = 20;
  config1.next = {"moduleb"};
  CNModuleConfig config2;
  config2.name = "moduleb";
  config2.className = "cnstream::TPTestModule";
  config2.parallelism = 1;
  config2.maxInputQueueSize = 20;
  CNGraphConfig graph_config;
  graph_config.module_configs = {config1, config2};
  EXPECT_TRUE(pipeline.BuildPipeline(graph_config));
  // case1: wrong module name
  EXPECT_FALSE(pipeline.IsRootNode("wrong_module_name"));
  // case2: not a root node
  EXPECT_FALSE(pipeline.IsRootNode("moduleb"));
  // case3: is a root node
  EXPECT_TRUE(pipeline.IsRootNode("modulea"));
}

TEST(CorePipeline, IsLeafNode) {
  Pipeline pipeline("test_pipeline");
  CNModuleConfig config1;
  config1.name = "modulea";
  config1.className = "cnstream::TPTestModule";
  config1.parallelism = 1;
  config1.maxInputQueueSize = 20;
  config1.next = {"moduleb"};
  CNModuleConfig config2;
  config2.name = "moduleb";
  config2.className = "cnstream::TPTestModule";
  config2.parallelism = 1;
  config2.maxInputQueueSize = 20;
  CNGraphConfig graph_config;
  graph_config.module_configs = {config1, config2};
  EXPECT_TRUE(pipeline.BuildPipeline(graph_config));
  // case1: wrong module name
  EXPECT_FALSE(pipeline.IsLeafNode("wrong_module_name"));
  // case2: not a leaf node
  EXPECT_FALSE(pipeline.IsLeafNode("modulea"));
  // case3: is a leaf node
  EXPECT_TRUE(pipeline.IsLeafNode("moduleb"));
}

}  // namespace cnstream
