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

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

#include "multistep_classifier_impl.hpp"
#include "cnstream_frame.hpp"

namespace cnstream {
MultiStepClassifierImpl::MultiStepClassifierImpl(int step1_classid, int bsize,
      int device_id, std::unordered_map<int, std::shared_ptr<edk::ModelLoader>> modelloader,
      std::unordered_map<int, std::vector<std::string>> labels)
      : step1_class_index_(step1_classid), batch_size_(bsize),
        model_loaders_(modelloader),  device_id_(device_id),
        labels_(labels) { }

MultiStepClassifierImpl::~MultiStepClassifierImpl() { Destory(); }

bool MultiStepClassifierImpl::Init() {
  if (model_loaders_.size() == 0) {
    LOG(ERROR) << "[MultiStepClassifierImpl] can not get model loader";
    return false;
  } else {
    std::unordered_map<int, std::shared_ptr<edk::ModelLoader>>::iterator iter;
    for (iter = model_loaders_.begin(); iter != model_loaders_.end(); iter++) {
      int class_index_ = iter->first;
      std::shared_ptr<edk::ModelLoader> model_load_ = iter->second;
      // input information flags
      bool model_yuv_input = model_load_->WithYUVInput();
      bool model_rgb0_output = model_load_->WithRGB0Output(nullptr);
      if (!model_yuv_input && model_rgb0_output) {
        LOG(ERROR) << "[MultiStepClassifierImpl] Model has wrong IO shape ";
      }

      edk::EasyInfer* infer = new edk::EasyInfer;
      edk::MluMemoryOp* memop = new edk::MluMemoryOp;
      edk::MluContext* env = new edk::MluContext;
      env->SetDeviceId(device_id_);
      env->ConfigureForThisThread();
      memop->SetLoader(model_load_);
      infer->Init(model_load_, batch_size_, device_id_);

      void **cpu_input = memop->AllocCpuInput(batch_size_);
      void **mlu_output = memop->AllocMluOutput(batch_size_);
      void **cpu_output = memop->AllocCpuOutput(batch_size_);
      void **mlu_input = memop->AllocMluInput(batch_size_);

      envs_.insert(std::make_pair(class_index_, env));
      memops_.insert(std::make_pair(class_index_, memop));
      infers_.insert(std::make_pair(class_index_, infer));
      cpu_inputs_.insert(std::make_pair(class_index_, cpu_input));
      mlu_inputs_.insert(std::make_pair(class_index_, mlu_input));
      cpu_outputs_.insert(std::make_pair(class_index_, cpu_output));
      mlu_outputs_.insert(std::make_pair(class_index_, mlu_output));
    }
  }
  return true;
}

void MultiStepClassifierImpl::Destory() {
  std::unordered_map<int, void**>::iterator iter;
  for (iter = cpu_inputs_.begin(); iter != cpu_inputs_.end(); iter++) {
    memops_[iter->first]->FreeCpuOutput(iter->second);
  }
  for (iter = cpu_outputs_.begin(); iter != cpu_outputs_.end(); iter++) {
    memops_[iter->first]->FreeCpuInput(iter->second);
  }
  for (iter = mlu_inputs_.begin(); iter != mlu_inputs_.end(); iter++) {
    memops_[iter->first]->FreeArrayMlu(iter->second, memops_[iter->first]->Loader()->InputNum());
  }
  for (iter = mlu_outputs_.begin(); iter != mlu_outputs_.end(); iter++) {
    memops_[iter->first]->FreeArrayMlu(iter->second, memops_[iter->first]->Loader()->OutputNum());
  }

  std::unordered_map<int, edk::MluMemoryOp*> ::iterator iter1;
  for (iter1 = memops_.begin(); iter1 != memops_.end(); iter1++) {
    delete memops_[iter->first];
    delete envs_[iter->first];
    delete infers_[iter->first];
  }
}

std::vector<std::pair<float*, uint64_t>>
 MultiStepClassifierImpl::Classify(const int& class_idx) {
  // set cpu input
  void **cpu_input = cpu_inputs_[class_idx];
  void **mlu_input = mlu_inputs_[class_idx];
  void **cpu_output = cpu_outputs_[class_idx];
  void **mlu_output = mlu_outputs_[class_idx];
  edk::MluMemoryOp* memop = memops_[class_idx];
  edk::EasyInfer* infer = infers_[class_idx];

  //  multiclassify by offline model
  memop->MemcpyInputH2D(mlu_input, cpu_input, 1);
  infer->Run(mlu_input, mlu_output);
  memop->MemcpyOutputD2H(cpu_output, mlu_output, 1);

  auto shapes = model_loaders_.find(class_idx)->second->OutputShapes();
  std::vector<std::pair<float*, uint64_t>> results;
  for (size_t batch_index = 0; batch_index < batch_size_; ++batch_index) {
    std::pair<float*, uint64_t> net_res;
    for (size_t i = 0; i < shapes.size(); ++i) {
      float *data = static_cast<float *>(cpu_output[i]);
      uint64_t data_count = shapes[i].DataCount();
      net_res.first = data + data_count * batch_index;
      net_res.second = data_count;

      results.push_back(net_res);
    }
  }

  return results;
  }

}  // namespace cnstream
