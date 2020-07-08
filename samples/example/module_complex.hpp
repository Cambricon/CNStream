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

 private:
  ModulePipeline(const ModulePipeline &) = delete;
  ModulePipeline &operator=(ModulePipeline const &) = delete;
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

    cnstream::CNModuleConfig a_config = {"ModuleInnerA", /*name*/
                                         {
                                             {"param", "innerA"},
                                         },
                                         2,               /*parallelism*/
                                         20,              /*maxInputQueueSize*/
                                         "ExampleModule", /*className*/
                                         {
                                             /* next,*/
                                             "ModuleInnerB",
                                         }};
    cnstream::CNModuleConfig b_config = {"ModuleInnerB", /*name*/
                                         {
                                             {"param", "innerB"},
                                         },
                                         2,               /*parallelism*/
                                         20,              /*maxInputQueueSize*/
                                         "ExampleModule", /*className*/
                                         {
                                             /* next, the last stage */
                                         }};
    pipeline_ = std::make_shared<ModulePipeline>("InnerPipeline");
    if (!pipeline_) {
      return false;
    }
    pipeline_->BuildPipeline({a_config, b_config});

    source_ = pipeline_->GetModule(a_config.name);
    sink_ = pipeline_->GetModule(b_config.name);
    sink_->SetObserver(this);
    pipeline_->Start();
    return true;
  }
  void Close() override {
    std::cout << this->GetName() << " Close called" << std::endl;
    if (pipeline_) {
      sink_->SetObserver(nullptr);
      pipeline_->Stop();
    }
  }
  int Process(FrameInfoPtr data) override {
    if (pipeline_) {
      /*use the same datastructure, so just forward data to inner-pipeline*/
      pipeline_->ProvideData(source_, data);
    }
    /*notify that data handle by the module*/
    return 1;
  }

  void notify(FrameInfoPtr data) override {
    if (data->IsEos()) {
      std::cout << "*****inner Observer :" << data->stream_id << "---"
                << "--EOS" << std::endl;
    } else {
      auto frame = cnstream::any_cast<std::shared_ptr<CNDataFrame>>(data->datas[CNDataFramePtrKey]);
      std::cout << "*****inner Observer :" << data->stream_id << "---" << frame->frame_id << std::endl;
    }

    /*use the same datastructure, so just forward data downstream modules*/
    this->TransmitData(data);
  }

 private:
  ComplexModule(const ComplexModule &) = delete;
  ComplexModule &operator=(ComplexModule const &) = delete;

 private:
  std::shared_ptr<ModulePipeline> pipeline_;
  cnstream::Module *source_;
  cnstream::Module *sink_;
};

#endif  // EXAMPLE_MODULE_COMPLEX_HPP_
