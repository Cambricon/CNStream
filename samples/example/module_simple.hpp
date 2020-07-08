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
#include <glog/logging.h>

#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "util/cnstream_queue.hpp"
#include "cnstream_core.hpp"

// user-defined data struct

static constexpr int CNDataFramePtrKey = 0;
struct CNDataFrame {
  int frame_id;
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

 private:
  ExampleModuleSource(const ExampleModuleSource &) = delete;
  ExampleModuleSource &operator=(ExampleModuleSource const &) = delete;
};

class ExampleSourceHandler : public cnstream::SourceHandler {
 public:
  explicit ExampleSourceHandler(cnstream::SourceModule *source, const std::string &stream_id)
      : cnstream::SourceHandler(source, stream_id) {}
  bool Open() override {
    thread_id_ = std::thread([&]() {
      std::string stream_id = this->GetStreamId();
      for (int i = 0; i < 10; i++) {
        auto data = this->CreateFrameInfo();
        // user-defined data structure CNFrameData instance to cnstream::any...
        std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame);
        frame->frame_id = i;
        data->datas[CNDataFramePtrKey] = frame;
        this->SendData(data);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      auto data_eos = this->CreateFrameInfo(true);
      this->SendData(data_eos);
    });
    return true;
  }
  void Close() override { thread_id_.join(); }

 private:
  std::thread thread_id_;
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
    auto frame = cnstream::any_cast<std::shared_ptr<CNDataFrame>>(data->datas[CNDataFramePtrKey]);
    std::cout << this->GetName() << " process: " << data->stream_id << "********anycast********* " << frame->frame_id;
    std::cout << std::endl;
    return 0;
  }

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
      if (data->IsEos()) {
        std::cout << this->GetName() << " process: " << data->stream_id << "--EOS" << std::endl;
      } else {
        auto frame = cnstream::any_cast<std::shared_ptr<CNDataFrame>>(data->datas[CNDataFramePtrKey]);
        std::cout << this->GetName() << " process: " << data->stream_id << "--" << frame->frame_id;
      }
      std::cout << " : " << std::this_thread::get_id() << std::endl;
    }
    // handle data in background threads...
    q_.Push(data);
    return 0;
  }

 private:
  void BackgroundProcess() {
    /*NOTE: EOS data has no invalid context,
     *    All data recevied including EOS must be forwarded.i
     */
    std::vector<FrameInfoPtr> eos_datas;
    std::vector<FrameInfoPtr> datas;
    FrameInfoPtr data;
    while (running_.load()) {
      bool value = q_.WaitAndTryPop(data, std::chrono::milliseconds(100));
      if (!value) continue;

      /*gather data*/
      if (!(data->IsEos())) {
        datas.push_back(data);
      } else {
        eos_datas.push_back(data);
      }

      if (datas.size() == 4 || (data->flags & cnstream::CN_FRAME_FLAG_EOS)) {
        /*process data...and then forward
         */
        for (auto &v : datas) {
          this->TransmitData(v);
          auto frame = cnstream::any_cast<std::shared_ptr<CNDataFrame>>(v->datas[CNDataFramePtrKey]);
          std::cout << this->GetName() << " forward: " << v->stream_id << "--" << frame->frame_id;
          std::cout << " : " << std::this_thread::get_id() << std::endl;
        }
        datas.clear();
      }

      /*forward EOS*/
      for (auto &v : eos_datas) {
        this->TransmitData(v);
        std::cout << this->GetName() << " forward: " << v->stream_id << "--EOS ";
        std::cout << " : " << std::this_thread::get_id() << std::endl;
      }
      eos_datas.clear();
    }  // while
  }

 private:
  cnstream::ThreadSafeQueue<FrameInfoPtr> q_;
  std::vector<std::thread> threads_;
  std::atomic<int> running_{0};

 private:
  ExampleModuleEx(const ExampleModuleEx &) = delete;
  ExampleModuleEx &operator=(ExampleModuleEx const &) = delete;
};

#endif  // EXAMPLE_MODULES_HPP_
