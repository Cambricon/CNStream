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
#ifndef EXAMPLE_MODULE_COMPLEX_HPP_
#define EXAMPLE_MODULE_COMPLEX_HPP_

#include <memory>
#include <string>

#include "module_simple.hpp"

class ModulePipeline : public cnstream::Pipeline {
  using super = cnstream::Pipeline;

 public:
  explicit ModulePipeline(const std::string &name) : super(name) {}
};

class ComplexModule : public cnstream::ModuleEx,
                      public cnstream::ModuleCreator<ComplexModule>,
                      public cnstream::IModuleObserver {
  using super = cnstream::ModuleEx;
  using FrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

 public:
  explicit ComplexModule(const std::string &name) : super(name) {}
  bool Open(cnstream::ModuleParamSet paramSet) override {
    std::cout << this->GetName() << " Open called" << std::endl;
    for (auto &v : paramSet) {
      std::cout << "\t" << v.first << " : " << v.second << std::endl;
    }
    cnstream::CNModuleConfig s_config = {"InnerFakeSource", /*name*/
                                         {
                                             {"param", "fakeSource"},
                                         },
                                         0,               /*parallelism*/
                                         0,               /*maxInputQueueSize*/
                                         "ExampleModuleSource", /*className*/
                                         {
                                             /* next,*/
                                             "InnerA",
                                         }};
    cnstream::CNModuleConfig a_config = {"InnerA", /*name*/
                                         {
                                             {"param", "innerA"},
                                         },
                                         8,               /*parallelism*/
                                         20,              /*maxInputQueueSize*/
                                         "ExampleModule", /*className*/
                                         {
                                             /* next,*/
                                             "InnerB",
                                         }};
    cnstream::CNModuleConfig b_config = {"InnerB", /*name*/
                                         {
                                             {"param", "innerB"},
                                         },
                                         8,               /*parallelism*/
                                         20,              /*maxInputQueueSize*/
                                         "ExampleModule", /*className*/
                                         {
                                             /* next, the last stage */
                                         }};
    pipeline_ = std::make_shared<ModulePipeline>("InnerPipeline");
    if (!pipeline_) {
      return false;
    }
    pipeline_->BuildPipeline({s_config, a_config, b_config});

    source_ = pipeline_->GetModule(s_config.name);
    sink_ = pipeline_->GetModule(b_config.name);
    sink_->SetObserver(this);

    if (!pipeline_->Start()) {
      LOGE(DEMO) << "Complex module " << this->GetName() << " starts nested pipeline failed.";
      return false;
  }
    return true;
  }
  void Close() override {
    LOGI(DEMO) << this->GetName() << " Close called";
    if (pipeline_) {
      sink_->SetObserver(nullptr);
      pipeline_->Stop();
    }
  }
  int Process(FrameInfoPtr data) override {
    if (pipeline_) {
      // create "data" for the inner pipeline
      std::string inner_stream_id = pipeline_->GetName() + "_" + data->stream_id;
      auto data_inner = cnstream::CNFrameInfo::Create(inner_stream_id, data->IsEos(), data);
      data_inner->SetStreamIndex(data->GetStreamIndex());
      data_inner->timestamp = data->timestamp;  // for debug ...
      if (!data->IsEos()) {
        // datas used in inner-pipeline
        auto frame = cnstream::any_cast<std::shared_ptr<CNDataFrame>>(data->datas[CNDataFramePtrKey]);
        data_inner->datas[CNDataFramePtrKey] = frame;
      } else {
        // LOGI(DEMO) << this->GetName() << " Module(InnerPipeline) Process: " << data->stream_id << "--EOS";
      }
      pipeline_->ProvideData(source_, data_inner);
    } else {
      this->TransmitData(data);
    }
    /*notify that data handle by the module*/
    return 1;
  }

  void notify(FrameInfoPtr data) override {
    // update data of outer-pipeline and forward...
    auto data_ = data->payload;
    if (data_->IsEos()) {
      // LOGI(DEMO) << "*****inner Observer :" << data_->stream_id << "---" << "--EOS";
    } else {
      // auto frame = cnstream::any_cast<std::shared_ptr<CNDataFrame>>(data_->datas[CNDataFramePtrKey]);
      // LOGI(DEMO) << "*****inner Observer :" << data_->stream_id << "---" << frame->frame_id;
    }
    this->TransmitData(data_);
  }

 private:
  std::shared_ptr<ModulePipeline> pipeline_;
  cnstream::Module *source_;
  cnstream::Module *sink_;
};

#endif  // EXAMPLE_MODULE_COMPLEX_HPP_
