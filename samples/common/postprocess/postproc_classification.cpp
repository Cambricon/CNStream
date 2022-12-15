/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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
#include <vector>

#include "cnstream_postproc.hpp"

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

class PostprocClassification : public cnstream::Postproc {
 public:
  int Execute(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
              const std::vector<cnstream::CNFrameInfoPtr>& packages,
              const cnstream::LabelStrings& labels) override;
  int Execute(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
              const std::vector<cnstream::CNFrameInfoPtr>& packages,
              const std::vector<cnstream::CNInferObjectPtr>& objects,
              const cnstream::LabelStrings& labels) override;

 private:
  DECLARE_REFLEX_OBJECT_EX(PostprocClassification, cnstream::Postproc);
};  // class PostprocClassification

IMPLEMENT_REFLEX_OBJECT_EX(PostprocClassification, cnstream::Postproc);

int PostprocClassification::Execute(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                                    const std::vector<cnstream::CNFrameInfoPtr>& packages,
                                    const cnstream::LabelStrings& labels) {
  LOGF_IF(PostprocClassification, net_outputs.size() != 1) << "PostprocClassification model output size is not valid";
  LOGF_IF(PostprocClassification, model_info.OutputNum() != 1)
      << "PostprocClassification model output number is not valid";

  cnedk::BufSurfWrapperPtr output = net_outputs[0].first;  // data
  if (!output->GetHostData(0)) {
    LOGE(PostprocClassification) << " copy data to host first.";
    return -1;
  }
  CnedkBufSurfaceSyncForCpu(output->GetBufSurface(), -1, -1);

  auto len = model_info.OutputShape(0).DataCount();

  for (size_t batch_idx = 0; batch_idx < packages.size(); batch_idx++) {
    float* data = static_cast<float*>(output->GetHostData(0, batch_idx));
    auto score_ptr = data;

    float max_score = 0;
    uint32_t label = 0;
    for (decltype(len) i = 0; i < len; ++i) {
      auto score = *(score_ptr + i);
      if (score > max_score) {
        max_score = score;
        label = i;
      }
    }
    if (threshold_ > 0 && max_score < threshold_) continue;

    cnstream::CNFrameInfoPtr package = packages[batch_idx];
    cnstream::CNInferObjsPtr objs_holder = nullptr;
    if (package->collection.HasValue(cnstream::kCNInferObjsTag)) {
      objs_holder = package->collection.Get<cnstream::CNInferObjsPtr>(cnstream::kCNInferObjsTag);
    }

    if (!objs_holder) {
      LOGE(PostprocClassification) << " object holder is nullptr.";
      return -1;
    }

    auto obj = std::make_shared<cnstream::CNInferObject>();
    obj->id = std::to_string(label);
    obj->score = max_score;

    if (!labels.empty() && label < labels[0].size()) {
      obj->AddExtraAttribute("Category", labels[0][label]);
    }

    std::lock_guard<std::mutex> lk(objs_holder->mutex_);
    objs_holder->objs_.push_back(obj);
  }  // for(batch_idx)

  return 0;
}

int PostprocClassification::Execute(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                                    const std::vector<cnstream::CNFrameInfoPtr>& packages,
                                    const std::vector<cnstream::CNInferObjectPtr>& objects,
                                    const cnstream::LabelStrings& labels) {
  LOGF_IF(PostprocClassification, net_outputs.size() != 1) << "PostprocClassification model output size is not valid";
  LOGF_IF(PostprocClassification, model_info.OutputNum() != 1)
      << "PostprocClassification model output number is not valid";

  cnedk::BufSurfWrapperPtr output = net_outputs[0].first;  // data
  if (!output->GetHostData(0)) {
    LOGE(PostprocClassification) << " copy data to host first.";
    return -1;
  }
  CnedkBufSurfaceSyncForCpu(output->GetBufSurface(), -1, -1);

  auto len = model_info.OutputShape(0).DataCount();

  for (size_t batch_idx = 0; batch_idx < packages.size(); batch_idx++) {
    float* data = static_cast<float*>(output->GetHostData(0, batch_idx));
    auto score_ptr = data;

    float max_score = 0;
    uint32_t label = 0;
    for (decltype(len) i = 0; i < len; ++i) {
      auto score = *(score_ptr + i);
      if (score > max_score) {
        max_score = score;
        label = i;
      }
    }
    if (threshold_ > 0 && max_score < threshold_) continue;

    cnstream::CNInferObjectPtr obj = objects[batch_idx];

    cnstream::CNInferAttr attr;
    attr.id = 0;
    attr.value = label;
    attr.score = max_score;

    obj->AddAttribute("classification", attr);
  }  // for(batch_idx)

  return 0;
}
