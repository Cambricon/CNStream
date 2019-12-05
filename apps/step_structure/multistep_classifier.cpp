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
#include <fstream>
#include <string>
#include <regex>
#include <utility>
#include <unordered_map>
#include <vector>

#include "multistep_classifier.hpp"
#include "cnstream_eventbus.hpp"
#include "easyinfer/mlu_context.h"
#include "easyinfer/mlu_memory_op.h"


MultiStepClassifier::MultiStepClassifier(const std::string &name) : Module(name) {}

MultiStepClassifierContext *MultiStepClassifier::GetMultiStepClassifierContext(cnstream::CNFrameInfoPtr data) {
  MultiStepClassifierContext *ctx = nullptr;
  auto search = ctxs_.find(data->channel_idx);
  if (search != ctxs_.end()) {
    // context exists
    ctx = search->second;
  } else {
    ctx = new MultiStepClassifierContext;
    ctx->impl = new cnstream::MultiStepClassifierImpl(step1_class_index, batch_size_,
                                            device_id_, model_loaders_, labels_);
    ctx->impl->Init();
    ctxs_[data->channel_idx] = ctx;
  }
  return ctx;
}

MultiStepClassifier::~MultiStepClassifier() { Close(); }

bool MultiStepClassifier::loadModelAndLableList(const std::string& modle_label_file, const std::string& func_name) {
  std::ifstream file(modle_label_file.c_str(), std::ios::in);
  std::string line;
  int class_index;
  std::string model;
  std::string label;
  if (file) {
    getline(file, line);
    std::istringstream first_line(line);
    first_line >> class_index >> model >> label;
    step1_class_index = class_index;
    model_files_.insert(std::make_pair(class_index, model));
    label_files_.insert(std::make_pair(class_index, label));

    while (getline(file, line)) {
      std::istringstream ss(line);
      ss >> class_index >> model >> label;
      model_files_.insert(std::make_pair(class_index, model));
      label_files_.insert(std::make_pair(class_index, label));
    }
  } else {
    LOG(ERROR) << "[MultiStepClassifier] can not open model file, model path: " << modle_label_file;
    return false;
  }
  file.close();

  if (model_files_.size() != label_files_.size()) {
    LOG(ERROR) << "[MultiStepClassifier] get model and label file failed ";
  } else {
    std::unordered_map<int, std::string>::iterator iter;
    for (iter = model_files_.begin(); iter != model_files_.end(); iter++) {
      int class_index = iter->first;
      std::string model_file = iter->second;
      std::string label = label_files_[class_index];

      try {
        model_loaders_.insert(std::make_pair(class_index,
                              std::make_shared<edk::ModelLoader>(model_file, func_name)));
      if (model_loaders_.find(class_index)->second.get() == nullptr) {
        LOG(ERROR) << "[Multiclassifier] load model failed, model path: " << model_file;
        return false;
       }
      model_loaders_.find(class_index)->second->InitLayout();

      std::ifstream file(label.c_str());
      if (!file) {
        LOG(ERROR) << "[MultiStepClassifier] unable to open label file: " << label;
        return false;
      }
      std::vector<std::string> lables;
      std::string line;
      while (std::getline(file, line)) {
        lables.push_back(std::string(line));
      }
      labels_.insert(std::make_pair(class_index, lables));
     } catch (edk::Exception& e) {
      LOG(ERROR) << e.what();
      return false;
     }
    }
  }

  return true;
}

bool MultiStepClassifier::Open(cnstream::ModuleParamSet paramSet) {
  if (paramSet.find("label_indexes") == paramSet.end() ||
      paramSet.find("model_label_list_path") == paramSet.end() ||
      paramSet.find("preproc_name") == paramSet.end() ||
      paramSet.find("postproc_name") == paramSet.end()) {
      LOG(ERROR) << "[MultiStepClassifier] Miss parameters";
    return false;
  }

  model_label_list_path_ = paramSet["model_label_list_path"];
  std::string func_name;
  if (paramSet.find("func_name") != paramSet.end()) {
    func_name = paramSet["func_name"];
  } else {
    func_name = "subnet0";
  }
  std::string postproc_name = paramSet["postproc_name"];
  std::string preproc_name = paramSet["preproc_name"];
  preproc_ = std::shared_ptr<cnstream::Preproc>(cnstream::Preproc::Create(preproc_name));
  postproc_ = std::shared_ptr<cnstream::Postproc>(cnstream::Postproc::Create(postproc_name));
  std::stringstream ss;
  int device_id;
  ss << paramSet["device_id"];
  ss >> device_id;
  device_id_ = device_id;

  if (!loadModelAndLableList(model_label_list_path_, func_name)) {
    LOG(ERROR) << "[MultiStepClassifier] load model list failed";
    return false;
  }

  device_id_ = 0;
  if (paramSet.find("device_id") != paramSet.end()) {
    std::stringstream ss;
    int device_id;
    ss << paramSet["device_id"];
    ss >> device_id;
    device_id_ = device_id;
  }

  batch_size_ = 1;
  if (paramSet.find("batch_size") != paramSet.end()) {
    std::stringstream ss;
    ss << paramSet["batch_size"];
    ss >> batch_size_;
    DLOG(INFO) << GetName() << " batch size:" << batch_size_;
  }

  /* hold this code. when all threads that set the cnrt device id exit, cnrt may release the memory itself */
  edk::MluContext ctx;
  ctx.SetDeviceId(device_id_);
  ctx.ConfigureForThisThread();

  std::regex reg {","};
  std::string label_indexes = paramSet["label_indexes"];
  matches_ = std::vector<std::string> {
    std::sregex_token_iterator(label_indexes.begin(), label_indexes.end(), reg, -1),
    std::sregex_token_iterator()};

  return !matches_.empty();
}

void MultiStepClassifier::Close() {
  if (ctxs_.empty()) {
    return;
  }
  for (auto &pair : ctxs_) {
    delete pair.second;
  }
  ctxs_.clear();
}

int MultiStepClassifier::Process(CNFrameInfoPtr data) {
    MultiStepClassifierContext *ctx = GetMultiStepClassifierContext(data);

    for (const auto& it : data->objs) {
      if (std::find(matches_.begin(), matches_.end(),
            it->id) == matches_.end())
        continue;

      int step_index = step1_class_index;
      for (size_t idx = 0; idx < strAttrsName.size(); idx++) {
        if (model_loaders_.find(step_index) == model_loaders_.end()) {
          continue;
        }
        const auto input_shapes = model_loaders_.find(step_index)->second->InputShapes();
        auto model = model_loaders_.find(step_index)->second;
        void **cpu_input = ctx->impl->cpu_inputs_[step_index];
        std::vector<float*> net_inputs;
        for (size_t input_i = 0; input_i < input_shapes.size(); ++input_i) {
          net_inputs.push_back(reinterpret_cast<float*>(cpu_input[input_i]));
        }
        preproc_->Execute(net_inputs, model, data, it->bbox);

        std::vector<std::pair<float*, uint64_t>> net_outputs =
                        ctx->impl->Classify(step_index);

        std::vector<std::string> label = labels_[step_index];
        std::vector<std::pair<std::string, std::string>> post_result;
        int result_index = postproc_->Execute(net_outputs, &post_result);

        it->AddExtraAttribute(strAttrsName[idx], label[result_index]);
        step_index = result_index;
      }
    }

    return 0;
}
