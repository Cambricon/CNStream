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
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <atomic>
#include <iostream>
#include "blockingconcurrentqueue.h"
#include "cnstream_core.hpp"

/* This example demostrates how to use cnstream::Module and cnstream::Pipeline framework,
 *              |------ModuleB------>|
 *  ModuleA---->|                    |----> ModuleD
 *              |------ModuleC------>|
 *  Please be noted, ModuleA is a source module, ModuleA::Process() will not be invoked,
 *     and Parallelism should be set zero.
 */
static std::mutex print_mutex;
class ExampleModuleSource : public cnstream::Module, public cnstream::ModuleCreator<ExampleModuleSource> {
  using super = cnstream::Module;

 public:
  explicit ExampleModuleSource(const std::string &name) : super(name) {}
  bool Open(cnstream::ModuleParamSet paramSet) override {
    std::cout << this->GetName() << " Open called" << std::endl;
    for (auto &v : paramSet) {
      std::cout << "\t" << v.first << " : " << v.second << std::endl;
    }
    return true;
  }
  void Close() override { std::cout << this->GetName() << " Close called" << std::endl; }
  int Process(std::shared_ptr<cnstream::CNFrameInfo> data) override {
    std::cout << "For a source module, Process() will not be invoked\n";
    return 0;
  }

 private:
  ExampleModuleSource(const ExampleModuleSource &) = delete;
  ExampleModuleSource &operator=(ExampleModuleSource const &) = delete;
};

class ExampleModule : public cnstream::Module, public cnstream::ModuleCreator<ExampleModule> {
  using super = cnstream::Module;

 public:
  explicit ExampleModule(const std::string &name) : super(name) {}
  bool Open(cnstream::ModuleParamSet paramSet) override {
    std::cout << this->GetName() << " Open called" << std::endl;
    for (auto &v : paramSet) {
      std::cout << "\t" << v.first << " : " << v.second << std::endl;
    }
    return true;
  }
  void Close() override { std::cout << this->GetName() << " Close called" << std::endl; }
  int Process(std::shared_ptr<cnstream::CNFrameInfo> data) override {
    // do something ...
    std::unique_lock<std::mutex> lock(print_mutex);
    std::cout << this->GetName() << " process: " << data->frame.stream_id << "--" << data->frame.frame_id;
    std::cout << " : " << std::this_thread::get_id() << std::endl;
    /*continue by the framework*/
    return 0;
  }

 private:
  //
 private:
  ExampleModule(const ExampleModule &) = delete;
  ExampleModule &operator=(ExampleModule const &) = delete;
};

class ExampleModuleEx : public cnstream::ModuleEx, public cnstream::ModuleCreator<ExampleModuleEx> {
  using super = cnstream::ModuleEx;
  using FrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

 public:
  explicit ExampleModuleEx(const std::string &name) : super(name) {}
  bool Open(cnstream::ModuleParamSet paramSet) override {
    std::cout << this->GetName() << " Open called" << std::endl;
    for (auto &v : paramSet) {
      std::cout << "\t" << v.first << " : " << v.second << std::endl;
    }
    running_.store(1);
    threads_.push_back(std::thread(&ExampleModuleEx::BackgroundProcess, this));
    return true;
  }
  void Close() override {
    running_.store(0);
    for (auto &thread : threads_) {
      thread.join();
    }
    std::cout << this->GetName() << " Close called" << std::endl;
  }
  int Process(FrameInfoPtr data) override {
    {
      std::unique_lock<std::mutex> lock(print_mutex);
      if (data->frame.flags & cnstream::CN_FRAME_FLAG_EOS) {
        std::cout << this->GetName() << " process: " << data->frame.stream_id << "--EOS";
      } else {
        std::cout << this->GetName() << " process: " << data->frame.stream_id << "--" << data->frame.frame_id;
      }
      std::cout << " : " << std::this_thread::get_id() << std::endl;
    }
    // handle data in background threads...
    q_.enqueue(data);

    /*notify that data handle by the module*/
    return 1;
  }

 private:
  void BackgroundProcess() {
    /*NOTE: EOS data has no invalid context,
     *    All data recevied including EOS must be forwarded.
     */
    std::vector<FrameInfoPtr> eos_datas;
    std::vector<FrameInfoPtr> datas;
    FrameInfoPtr data;
    while (running_.load()) {
      bool value = q_.wait_dequeue_timed(data, 1000 * 100);
      if (!value) continue;

      /*gather data*/
      if (!(data->frame.flags & cnstream::CN_FRAME_FLAG_EOS)) {
        datas.push_back(data);
      } else {
        eos_datas.push_back(data);
      }

      if (datas.size() == 4 || (data->frame.flags & cnstream::CN_FRAME_FLAG_EOS)) {
        /*process data...and then forward
         */
        for (auto &v : datas) {
          this->container_->ProvideData(this, v);
          std::unique_lock<std::mutex> lock(print_mutex);
          std::cout << this->GetName() << " forward: " << v->frame.stream_id << "--" << v->frame.frame_id;
          std::cout << " : " << std::this_thread::get_id() << std::endl;
        }
        datas.clear();
      }

      /*forward EOS*/
      for (auto &v : eos_datas) {
        this->container_->ProvideData(this, v);
        std::unique_lock<std::mutex> lock(print_mutex);
        std::cout << this->GetName() << " forward: " << v->frame.stream_id << "--EOS ";
        std::cout << " : " << std::this_thread::get_id() << std::endl;
      }
      eos_datas.clear();
    }  // while
  }

 private:
  moodycamel::BlockingConcurrentQueue<FrameInfoPtr> q_;
  std::vector<std::thread> threads_;
  std::atomic<int> running_{0};

 private:
  ExampleModuleEx(const ExampleModuleEx &) = delete;
  ExampleModuleEx &operator=(ExampleModuleEx const &) = delete;
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
  std::mutex count_mutex_;
  uint32_t count_mask_ = 0;
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
                                              "ExampleModule", /*className*/
                                              {
                                                  /* next, the last stage */
                                              }};

  /*create pipeline*/
  MyPipeline pipeline("pipeline");
  pipeline.BuildPipeline({module_a_config, module_b_config, module_c_config, module_d_config});

  /*start pipeline*/
  if (!pipeline.Start()) {
    LOG(ERROR) << "Pipeline start failed.";
    return EXIT_FAILURE;
  }

  /*send data to pipeline*/
  cnstream::Module *source = pipeline.GetModule(module_a_config.name);
  std::vector<std::thread> threads;

  for (int j = 0; j < 2; j++) {
    threads.push_back(std::thread([&, j]() {
      std::string stream_id("stream_id_");
      stream_id += std::to_string(j);
      for (int i = 0; i < 100; i++) {
        auto data = cnstream::CNFrameInfo::Create(stream_id);
        data->frame.frame_id = i;
        data->channel_idx = j;
        pipeline.ProvideData(source, data);
        usleep(1000);
      }

      auto data_eos = cnstream::CNFrameInfo::Create(stream_id, true);
      data_eos->channel_idx = j;
      pipeline.ProvideData(source, data_eos);
    }));
  }

  for (auto &thread : threads) {
    thread.join();
  }
  /*close pipeline*/
  pipeline.WaitForStop();
  return EXIT_SUCCESS;
}
