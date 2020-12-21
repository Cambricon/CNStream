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

#ifndef EXAMPLE_MODULES_HPP_
#define EXAMPLE_MODULES_HPP_

#include <gflags/gflags.h>

#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "util/cnstream_queue.hpp"
#include "cnstream_core.hpp"
#include "cnstream_logging.hpp"

// user-defined data struct

static constexpr int CNDataFramePtrKey = 0;
struct CNDataFrame {
  uint64_t frame_id;
};

class ExampleModuleSource : public cnstream::SourceModule, public cnstream::ModuleCreator<ExampleModuleSource> {
  using super = cnstream::SourceModule;

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
};

class ExampleSourceHandler : public cnstream::SourceHandler {
 public:
  explicit ExampleSourceHandler(cnstream::SourceModule *source, const std::string &stream_id)
      : cnstream::SourceHandler(source, stream_id) {}
  bool Open() override {
    thread_id_ = std::thread([=]() {
      uint64_t i = 0;
      while (!exit_flag_) {
        auto data = this->CreateFrameInfo();
        // user-defined data structure CNFrameData instance to cnstream::any...
        std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame);
        frame->frame_id = i++;
        data->datas[CNDataFramePtrKey] = frame;
        data->timestamp = frame->frame_id;  // for debug ...
        data->SetStreamIndex(stream_index_);
        if (this->SendData(data) != true) {
          break;
        }
        std::this_thread::yield();
      }
      LOGI(DEMO) << "Source Send EOS..." << GetStreamId();
      auto data_eos = this->CreateFrameInfo(true);
      data_eos->SetStreamIndex(stream_index_);
      this->SendData(data_eos);
      LOGI(DEMO) << "Source Send EOS..." << GetStreamId() << " Done";
    });
    return true;
  }
  void Close() override {
    // LOGI(DEMO) << "ExampleSourceHandler::Close() called..." << GetStreamId();
    exit_flag_ = 1;
    if (thread_id_.joinable()) {
      thread_id_.join();
    }
  }

 private:
  std::thread thread_id_;
  volatile int exit_flag_ = 0;
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
    // auto frame = cnstream::any_cast<std::shared_ptr<CNDataFrame>>(data->datas[CNDataFramePtrKey]);
    // std::cout << this->GetName() << " process: " << data->stream_id
    // std::cout << "********anycast********* " << frame->frame_id;
    // std::cout << std::endl;
    if (cnstream::IsStreamRemoved(data->stream_id)) {
      LOGE(DEMO) << "SHOULD NOT BE SHOWN_____Process ---- stream removed";
      return 0;
    }
    // LOGI(DEMO) << "Process  ...Done" << GetName() << ": " << data->stream_id << ", " << data->timestamp;
    usleep(1000 * 1000);
    // LOGI(DEMO) << "Process  ...Done" << GetName() << ": " << data->stream_id << ", " << data->timestamp;
    return 0;
  }

  void OnEos(const std::string &stream_id) override {
    LOGI(DEMO) << this->GetName() << " OnEos flow-EOS arrived:  " << stream_id;
  }
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
    LOGI(DEMO) << this->GetName() << " Close called";
  }
  int Process(FrameInfoPtr data) override {
    if (data->IsEos()) {
      LOGI(DEMO) << this->GetName() << " process: " << data->stream_id << "--EOS";
    } else {
      // auto frame = cnstream::any_cast<std::shared_ptr<CNDataFrame>>(data->datas[CNDataFramePtrKey]);
      // std::cout << this->GetName() << " process: " << data->stream_id << "--" << frame->frame_id;
     // std::cout << " : " << std::this_thread::get_id() << std::endl;

      if (data->IsRemoved()) {
        return 0;
      }
    }
    // handle data in background threads...
    q_.Push(data);
    return 0;
  }

 private:
  void BackgroundProcess() {
    /*NOTE: EOS data has no invalid context,
     *    All data recevied including EOS must be forwarded.
     */
    std::vector<FrameInfoPtr> eos_datas;
    std::vector<FrameInfoPtr> datas;
    while (running_.load()) {
      FrameInfoPtr data = nullptr;  // NOTE: FrameInfoPtr is shared_ptr !!!
      bool value = q_.WaitAndTryPop(data, std::chrono::milliseconds(20));
      if (!value) {
        continue;
      }

      if (data->IsEos()) {
        LOGI(DEMO) << this->GetName() << " BackgroundProcess: " << data->stream_id << "--EOS";
      }

      /*gather data*/
      if (!(data->IsEos())) {
        datas.push_back(data);
      } else {
        eos_datas.push_back(data);
      }

      // drop packets for removed-stream
      //    flush the buffered packets
      if (cnstream::IsStreamRemoved(data->stream_id)) {
        datas.clear();
      }

      if (datas.size() == 4 || (data->IsEos())) {
        /*process data...and then forward
         */
        for (auto &v : datas) {
          this->TransmitData(v);
          // auto frame = cnstream::any_cast<std::shared_ptr<CNDataFrame>>(v->datas[CNDataFramePtrKey]);
          // std::cout << this->GetName() << " forward: " << v->stream_id << "--" << frame->frame_id;
          // std::cout << " : " << std::this_thread::get_id() << std::endl;
        }
        datas.clear();
      }

      /*forward EOS*/
      for (auto &v : eos_datas) {
        this->TransmitData(v);
        LOGI(DEMO) << this->GetName() << " forward: " << v->stream_id << "--EOS " << " : " << std::this_thread::get_id();
      }
      eos_datas.clear();
    }  // while
  }

 private:
  cnstream::ThreadSafeQueue<FrameInfoPtr> q_;
  std::vector<std::thread> threads_;
  std::atomic<int> running_{0};
};

#endif  // EXAMPLE_MODULES_HPP_
