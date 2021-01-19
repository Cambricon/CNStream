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
#include <string>

#include "module_complex.hpp"
#include "module_simple.hpp"

/* This example demonstrates how to use cnstream::Module and cnstream::Pipeline framework,
 *              |------ModuleB------>|
 *  ModuleA---->|                    |----> ModuleD
 *              |------ModuleC------>|
 *  Please be noted,
 *   ModuleA is a source module,
 *   ModuleD is a complex module (there is a pipeline inside)
 */

class MyPipeline : public cnstream::Pipeline, public cnstream::StreamMsgObserver {
  using super = cnstream::Pipeline;

 public:
  explicit MyPipeline(const std::string &name) : super(name) {
    exit_flag_ = 0;
    this->SetStreamMsgObserver(this);
  }
  void WaitForStop() {
    while (!exit_flag_.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    this->Stop();
  }

 private:
  /*StremMsgObserver*/
  void Update(const cnstream::StreamMsg &smsg) override {
    if (smsg.type == cnstream::StreamMsgType::EOS_MSG) {
      /*when all stream eos reached, set exit_flag,
       * we only have two channels for this example
       */
      if (smsg.stream_id == "stream_id_0") {
        std::unique_lock<std::mutex> lock(count_mutex_);
        count_mask_ |= 0x01;
      }
      if (smsg.stream_id == "stream_id_1") {
        std::unique_lock<std::mutex> lock(count_mutex_);
        count_mask_ |= 0x02;
      }
      {
        std::unique_lock<std::mutex> lock(count_mutex_);
        if ((count_mask_ & 0x03) == 0x03) {
          exit_flag_.store(1);
        }
      }
      std::cout << "[Observer] " << smsg.stream_id << " received EOS" << std::endl;
    } else if (smsg.type == cnstream::StreamMsgType::ERROR_MSG) {
      std::cout << "[Observer] " << smsg.stream_id << " received ERROR_MSG" << std::endl;
    }
  }

 private:
  MyPipeline(const MyPipeline &) = delete;
  MyPipeline &operator=(MyPipeline const &) = delete;

 private:
  std::atomic<int> exit_flag_{0};
  std::mutex count_mutex_;
  uint32_t count_mask_ = 0;
};

class Observer : public cnstream::IModuleObserver {
  using FrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

 public:
  Observer() {}
  void notify(FrameInfoPtr data) override {
    if (data->IsEos()) {
      std::cout << "*****Observer :" << data->stream_id << "---"
                << "--EOS" << std::endl;
    } else {
      auto frame = cnstream::any_cast<std::shared_ptr<CNDataFrame>>(data->datas[CNDataFramePtrKey]);
      std::cout << "*****Observer :" << data->stream_id << "---" << frame->frame_id << std::endl;
    }
  }
};

int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
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
                                              2,                 /*parallelism*/
                                              20,                /*maxInputQueueSize*/
                                              "ExampleModuleEx", /*className*/
                                              {
                                                  /* next, downstream module names */
                                                  "ModuleD",
                                              }};
  cnstream::CNModuleConfig module_c_config = {"ModuleC", /*name*/
                                              {
                                                  /*paramSet */
                                                  {"param", "C"},
                                              },
                                              2,               /*parallelism*/
                                              20,              /*maxInputQueueSize*/
                                              "ExampleModule", /*className*/
                                              {
                                                  /* next,*/
                                                  "ModuleD",
                                              }};
  cnstream::CNModuleConfig module_d_config = {"ModuleD", /*name*/
                                              {
                                                  /*paramSet */
                                                  {"param", "D"},
                                              },
                                              2,               /*parallelism*/
                                              20,              /*maxInputQueueSize*/
                                              "ComplexModule", /*className*/
                                              {
                                                  /* next, the last stage */
                                              }};

  /*create pipeline*/
  MyPipeline pipeline("pipeline");
  pipeline.BuildPipeline({module_a_config, module_b_config, module_c_config, module_d_config});

  cnstream::Module *sink = pipeline.GetModule(module_d_config.name);
  Observer observer;
  sink->SetObserver(&observer);

  /*start pipeline*/
  if (!pipeline.Start()) {
    LOG(ERROR) << "Pipeline start failed.";
    return EXIT_FAILURE;
  }

  /*send data to pipeline*/
  cnstream::Module *source = pipeline.GetModule(module_a_config.name);
  cnstream::SourceModule *source_ = dynamic_cast<cnstream::SourceModule *>(source);
  std::shared_ptr<cnstream::SourceHandler> handler0(new (std::nothrow) ExampleSourceHandler(source_, "stream_id_0"));
  source_->AddSource(handler0);
  std::shared_ptr<cnstream::SourceHandler> handler1(new (std::nothrow) ExampleSourceHandler(source_, "stream_id_1"));
  source_->AddSource(handler1);

  /*close pipeline*/
  pipeline.WaitForStop();
  google::ShutdownGoogleLogging();
  return EXIT_SUCCESS;
}
