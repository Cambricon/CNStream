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

#include <memory>
#include <random>
#include <string>

#include "module_complex.hpp"
#include "module_simple.hpp"

/* This example demonstrates how to use cnstream::Module and cnstream::Pipeline framework,
 *              |------ModuleB------>|
 *  ModuleA---->|                    |----> ModuleD
 *              |------ModuleC------>|
 *  Please be noted,
 *   ModuleA is a source module
 */

class MyPipeline : public cnstream::Pipeline, public cnstream::StreamMsgObserver {
  using super = cnstream::Pipeline;

 public:
  explicit MyPipeline(const std::string &name) : super(name) {
    this->SetStreamMsgObserver(this);
  }
  static const int TEST_STREAM_NUM = 64;
 private:
  /*StremMsgObserver*/
  void Update(const cnstream::StreamMsg &smsg) override {
    if (smsg.type == cnstream::StreamMsgType::EOS_MSG) {
      LOGI(DEMO) << "Update[Observer] " << smsg.stream_id << " received EOS";
    } else if (smsg.type == cnstream::StreamMsgType::ERROR_MSG) {
      LOGI(DEMO) << "Update[Observer] " << smsg.stream_id << " received ERROR_MSG";
    }
  }

 private:
  MyPipeline(const MyPipeline &) = delete;
  MyPipeline &operator=(MyPipeline const &) = delete;
};

class Observer : public cnstream::IModuleObserver {
  using FrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

 public:
  Observer() {}
  void notify(FrameInfoPtr data) override {
    if (data->IsEos()) {
      LOGI(DEMO) << "notify*****Observer :" << data->stream_id << "---" << "use_count = " << data.use_count() << "--EOS";
    } else {
      // auto frame = cnstream::any_cast<std::shared_ptr<CNDataFrame>>(data->datas[CNDataFramePtrKey]);
      // LOGI(DEMO) << "notify*****Observer :" << data->stream_id << "---" << frame->frame_id;
    }
  }
};

int main(int argc, char **argv) {
  cnstream::InitCNStreamLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, false);

  std::cout << "\033[01;31m"
            << "CNSTREAM VERSION:" << cnstream::VersionString() << "\033[0m" << std::endl;

  /*module configs*/
  cnstream::CNModuleConfig module_a_config = {
      "ModuleA", /*name*/
      {
          /*paramSet */
          {"param", "A"},
      },
      0,                     /*parallelism*/
      0,                     /*maxInputQueueSize, source module does not have input-queue at this moment*/
      "ExampleModuleSource", /*className*/
      {
          /* next, downstream module names */
          "ModuleB",
          "ModuleC",
      }};
  cnstream::CNModuleConfig module_b_config = {"ModuleB", /*name*/
                                              {
                                                  /*paramSet */
                                                  {"param", "B"},
                                              },
                                              8,                 /*parallelism*/
                                              20,                /*maxInputQueueSize*/
                                              "ExampleModuleEx", /*className*/
                                              // "ExampleModule", /*className*/
                                              {
                                                  /* next, downstream module names */
                                                  "ModuleD",
                                              }};
  cnstream::CNModuleConfig module_c_config = {"ModuleC", /*name*/
                                              {
                                                  /*paramSet */
                                                  {"param", "C"},
                                              },
                                              8,               /*parallelism*/
                                              20,              /*maxInputQueueSize*/
                                              "ComplexModule", /*className*/
                                              // "ExampleModule", /*className*/
                                              // "ExampleModuleEx", /*className*/
                                              {
                                                  /* next,*/
                                                  "ModuleD",
                                              }};
  cnstream::CNModuleConfig module_d_config = {"ModuleD", /*name*/
                                              {
                                                  /*paramSet */
                                                  {"param", "D"},
                                              },
                                              8,               /*parallelism*/
                                              20,              /*maxInputQueueSize*/
                                              // "ComplexModule", /*className*/
                                              "ExampleModule", /*className*/
                                              {
                                                  /* next,*/
                                              }};

  /*create pipeline*/
  MyPipeline pipeline("pipeline");
  pipeline.BuildPipeline({module_a_config, module_b_config, module_c_config, module_d_config});

  cnstream::Module *sink = pipeline.GetModule(module_d_config.name);
  Observer observer;
  sink->SetObserver(&observer);

  /*start pipeline*/
  if (!pipeline.Start()) {
    LOGE(DEMO) << "Pipeline start failed.";
    return EXIT_FAILURE;
  }

  cnstream::Module *source = pipeline.GetModule(module_a_config.name);
  cnstream::SourceModule *source_ = dynamic_cast<cnstream::SourceModule *>(source);

  std::default_random_engine random_engine;
  std::uniform_int_distribution<unsigned> u(0, 5);
  /*send data to pipeline*/
#if 1
  for (int i = 0; i < 10; i++) {
    LOGI(DEMO) << i << "test1_______add stream_id_0, feed data for random ms (0..10000), then remove it\n\n";
    std::shared_ptr<cnstream::SourceHandler> handler(new (std::nothrow) ExampleSourceHandler(source_, "stream_id_0"));
    source_->AddSource(handler);
    unsigned int value = u(random_engine);
    std::this_thread::sleep_for(std::chrono::milliseconds(value * 1000));
    source_->RemoveSource(handler, true);  // block until stream_id_0 eos reached
    LOGI(DEMO) << i << "________source stream_id_0 forced removed,  feed data for " << value * 1000 << "ms\n\n";
  }
#endif

#if 1
  for (int i = 0; i < 10; i++) {
    std::string desc = std::to_string(i) + "________test2_______add stream_id_0..";
    desc += std::to_string(MyPipeline::TEST_STREAM_NUM - 1);
    desc += ", feed data for random ms (0 .. 5000), then remove them\n\n";
    LOGI(DEMO) << desc;
    for (int i = 0; i < MyPipeline::TEST_STREAM_NUM; i ++) {
      std::string stream_id = "stream_id_" + std::to_string(i);
      std::shared_ptr<cnstream::SourceHandler> handler(new (std::nothrow) ExampleSourceHandler(source_, stream_id));
      source_->AddSource(handler);
    }
    unsigned int value = u(random_engine);
    std::this_thread::sleep_for(std::chrono::milliseconds(value * 1000));
    source_->RemoveSources(true);  // block until all stream eos reached
    LOGI(DEMO) << i << "________source all streams removed (feeding data for " << value  * 1000 << " ms)\n\n";
  }
#endif

  LOGI(DEMO) << "_______Press any key to exit ...";
  getchar();
  pipeline.Stop();
  cnstream::ShutdownCNStreamLogging();
  return EXIT_SUCCESS;
}
