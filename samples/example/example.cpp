/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <atomic>
#include <iostream>
#include "cnstream_core.hpp"

/* This example demostrates how to use cnstream::Module and cnstream::Pipeline framework,
*              |------ModuleB------>|
*  ModuleA---->|                    |----> ModuleD
*              |------ModuleC------>|
*  Please be noted, ModuleA is a source module, ModuleA::Process() will not be invoked,
*     and Parallelism should be set zero.
*/
static std::mutex print_mutex;
class ExampelModuleSource : public cnstream::Module {
  using super = cnstream::Module;

 public:
  explicit ExampelModuleSource(const std::string &name) : super(name) {}
  bool Open(cnstream::ModuleParamSet paramSet) override {
    std::cout << "Module " << this->GetName() << " Open called" << std::endl;
    for (auto &v : paramSet) {
      std::cout << "\t" << v.first << " : " << v.second << std::endl;
    }
    return true;
  }
  void Close() override { std::cout << "Module " << this->GetName() << " Close called" << std::endl; }
  int Process(std::shared_ptr<cnstream::CNFrameInfo> data) override {
    std::cout << "For a source module, Process() will not be invoked\n";
    return 0;
  }

 private:
  ExampelModuleSource(const ExampelModuleSource &) = delete;
  ExampelModuleSource &operator=(ExampelModuleSource const &) = delete;
};

class ExampleModule : public cnstream::Module {
  using super = cnstream::Module;

 public:
  explicit ExampleModule(const std::string &name) : super(name) {}
  bool Open(cnstream::ModuleParamSet paramSet) override {
    std::cout << "Module " << this->GetName() << " Open called" << std::endl;
    for (auto &v : paramSet) {
      std::cout << "\t" << v.first << " : " << v.second << std::endl;
    }
    return true;
  }
  void Close() override { std::cout << "Module " << this->GetName() << " Close called" << std::endl; }
  int Process(std::shared_ptr<cnstream::CNFrameInfo> data) override {
    // do something ...
    if (this->GetName() == "ModuleB") {
      usleep(1000);
    } else if (this->GetName() == "ModuleC") {
      usleep(500);
    }
    std::unique_lock<std::mutex> lock(print_mutex);
    std::cout << "Module " << this->GetName() << " Process called: " << std::this_thread::get_id() << std::endl;
    std::cout << "\t" << data->frame.stream_id << ":" << data->frame.frame_id << std::endl;
    return 0;
  }

 private:
  //
 private:
  ExampleModule(const ExampleModule &) = delete;
  ExampleModule &operator=(ExampleModule const &) = delete;
};

class MyPipeline : public cnstream::Pipeline, public cnstream::StreamMsgObserver {
  using super = cnstream::Pipeline;

 public:
  explicit MyPipeline(const std::string &name) : super(name) {
    exit_flag_ = 0;
    this->SetStreamMsgObserver(this);
  }
  void WaitForStop() {
    while (!exit_flag_.load()) {
      usleep(1000 * 10);
    }
    this->Stop();
  }

 private:
  /*StremMsgObserver*/
  void Update(const cnstream::StreamMsg &smsg) override {
    if (smsg.type == cnstream::StreamMsgType::EOS_MSG) {
      /*when all stream eos reached, set exit_flag,
      * we only have one channel for this example
      */
      if (smsg.stream_id == "stream_id_0") {
        exit_flag_.store(1);
      }
      std::unique_lock<std::mutex> lock(print_mutex);
      std::cout << "[Observer] " << smsg.stream_id << " received EOS" << std::endl;
    } else if (smsg.type == cnstream::StreamMsgType::ERROR_MSG) {
      std::unique_lock<std::mutex> lock(print_mutex);
      std::cout << "[Observer] " << smsg.stream_id << " received ERROR_MSG" << std::endl;
    }
  }

 private:
  MyPipeline(const MyPipeline &) = delete;
  MyPipeline &operator=(MyPipeline const &) = delete;

 private:
  std::atomic<int> exit_flag_{0};
};

int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  std::cout << "\033[01;31m"
            << "CNSTREAM VERSION:" << cnstream::VersionString() << "\033[0m" << std::endl;

  /*create pipeline*/
  MyPipeline pipeline("pipeline");

  /*module configs*/
  cnstream::CNModuleConfig module_a_config = {"ModuleA",             /*name*/
                                              "ExampleModuleSource", /*className*/
                                              {
                                                  /*paramSet */
                                                  {"param", "A"},
                                              },
                                              {
                                                  /* next, downstream module names */
                                                  "ModuleB", "ModuleC",
                                              }};
  cnstream::CNModuleConfig module_b_config = {"ModuleB",       /*name*/
                                              "ExampleModule", /*className*/
                                              {
                                                  /*paramSet */
                                                  {"param", "B"},
                                              },
                                              {
                                                  /* next, downstream module names */
                                                  "ModuleD",
                                              }};
  cnstream::CNModuleConfig module_c_config = {"ModuleC",       /*name*/
                                              "ExampleModule", /*className*/
                                              {
                                                  /*paramSet */
                                                  {"param", "C"},
                                              },
                                              {
                                                  /* next,*/
                                                  "ModuleD",
                                              }};
  cnstream::CNModuleConfig module_d_config = {"ModuleD",       /*name*/
                                              "ExampleModule", /*className*/
                                              {
                                                  /*paramSet */
                                                  {"param", "D"},
                                              },
                                              {
                                                  /* next, the last stage */
                                              }};
  pipeline.AddModuleConfig(module_a_config);
  pipeline.AddModuleConfig(module_b_config);
  pipeline.AddModuleConfig(module_c_config);
  pipeline.AddModuleConfig(module_d_config);

  /*create modules.*/
  auto moduleA = std::make_shared<ExampleModule>(module_a_config.name);
  auto moduleB = std::make_shared<ExampleModule>(module_b_config.name);
  auto moduleC = std::make_shared<ExampleModule>(module_c_config.name);
  auto moduleD = std::make_shared<ExampleModule>(module_d_config.name);

  /*register modules to pipeline*/
  if (!pipeline.AddModule({moduleA, moduleB, moduleC, moduleD})) {
    LOG(ERROR) << "Add modules failed";
    return -1;
  }

  /*
   * default value 1 will take effect if module parallelism not set.
   * the recommended value of inferencer's thread is the number of stream source.
  */
  pipeline.SetModuleParallelism(moduleA, 0);
  pipeline.SetModuleParallelism(moduleB, 1);
  pipeline.SetModuleParallelism(moduleC, 1);
  pipeline.SetModuleParallelism(moduleD, 1);

  /*link modules*/
  {
    if (pipeline.LinkModules(moduleA, moduleB).empty()) {
      LOG(ERROR) << "link decoder with detector failed.";
      return EXIT_FAILURE;
    }
    if (pipeline.LinkModules(moduleA, moduleC).empty()) {
      LOG(ERROR) << "link decoder with detector failed.";
      return EXIT_FAILURE;
    }
    if (pipeline.LinkModules(moduleB, moduleD).empty()) {
      LOG(ERROR) << "link decoder with detector failed.";
      return EXIT_FAILURE;
    }
    if (pipeline.LinkModules(moduleC, moduleD).empty()) {
      LOG(ERROR) << "link decoder with detector failed.";
      return EXIT_FAILURE;
    }
  }

  /*start pipeline*/
  if (!pipeline.Start()) {
    LOG(ERROR) << "Pipeline start failed.";
    return EXIT_FAILURE;
  }

  /*send data to pipeline*/
  do {
    auto data = std::make_shared<cnstream::CNFrameInfo>();
    data->frame.stream_id = "stream_id_0";
    data->frame.frame_id = 100;
    pipeline.ProvideData(moduleA.get(), data);

    auto data1 = std::make_shared<cnstream::CNFrameInfo>();
    data1->frame.stream_id = "stream_id_0";
    data1->frame.frame_id = 101;
    pipeline.ProvideData(moduleA.get(), data1);

    auto data_eos = std::make_shared<cnstream::CNFrameInfo>();
    data_eos->frame.stream_id = "stream_id_0";
    data_eos->frame.flags |= cnstream::CN_FRAME_FLAG_EOS;
    pipeline.ProvideData(moduleA.get(), data_eos);
  } while (0);

  /*close pipeline*/
  pipeline.WaitForStop();
  return EXIT_SUCCESS;
}
