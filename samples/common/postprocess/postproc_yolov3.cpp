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

#include <algorithm>
#include <memory>
#include <vector>

#include "cnstream_postproc.hpp"

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

class PostprocYolov3 : public cnstream::Postproc {
 public:
  int Execute(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
              const std::vector<cnstream::CNFrameInfoPtr>& packages,
              const cnstream::LabelStrings& labels) override;

 private:
  DECLARE_REFLEX_OBJECT_EX(PostprocYolov3, cnstream::Postproc);
};  // class PostprocYolov3

IMPLEMENT_REFLEX_OBJECT_EX(PostprocYolov3, cnstream::Postproc);

int PostprocYolov3::Execute(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                            const std::vector<cnstream::CNFrameInfoPtr>& packages,
                            const cnstream::LabelStrings& labels) {
  LOGF_IF(PostprocYolov3, net_outputs.size() != 2) << "PostprocYolov3 net outputs size is not valid";
  LOGF_IF(PostprocYolov3, model_info.OutputNum() != 2) << "PostprocYolov3 output number is not valid";

  cnedk::BufSurfWrapperPtr output0 = net_outputs[0].first;  // data
  cnedk::BufSurfWrapperPtr output1 = net_outputs[1].first;  // bbox
  if (!output0->GetHostData(0)) {
    LOGE(PostprocYolov3) << " copy data to host first.";
    return -1;
  }
  if (!output1->GetHostData(0)) {
    LOGE(PostprocYolov3) << " copy data to host first.";
    return -1;
  }
  CnedkBufSurfaceSyncForCpu(output0->GetBufSurface(), -1, -1);
  CnedkBufSurfaceSyncForCpu(output1->GetBufSurface(), -1, -1);

  infer_server::DimOrder input_order = model_info.InputLayout(0).order;
  auto s = model_info.InputShape(0);
  int model_input_w, model_input_h;
  if (input_order == infer_server::DimOrder::NCHW) {
    model_input_w = s[3];
    model_input_h = s[2];
  } else if (input_order == infer_server::DimOrder::NHWC) {
    model_input_w = s[2];
    model_input_h = s[1];
  } else {
    LOGE(PostprocYolov3) << "not supported dim order";
    return -1;
  }

  for (size_t batch_idx = 0; batch_idx < packages.size(); batch_idx++) {
    float* data = static_cast<float*>(output0->GetHostData(0, batch_idx));
    int box_num = static_cast<int*>(output1->GetHostData(0, batch_idx))[0];
    if (!box_num) {
      continue;  // no bboxes
    }

    cnstream::CNFrameInfoPtr package = packages[batch_idx];
    const auto frame = package->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag);
    cnstream::CNInferObjsPtr objs_holder = nullptr;
    if (package->collection.HasValue(cnstream::kCNInferObjsTag)) {
      objs_holder = package->collection.Get<cnstream::CNInferObjsPtr>(cnstream::kCNInferObjsTag);
    }

    if (!objs_holder) {
      return -1;
    }

    const float scaling_w = 1.0f * model_input_w / frame->buf_surf->GetWidth();
    const float scaling_h = 1.0f * model_input_h / frame->buf_surf->GetHeight();
    const float scaling = std::min(scaling_w, scaling_h);
    float scaling_factor_w, scaling_factor_h;
    scaling_factor_w = scaling_w / scaling;
    scaling_factor_h = scaling_h / scaling;

    std::lock_guard<std::mutex> lk(objs_holder->mutex_);
    for (int bi = 0; bi < box_num; ++bi) {
      if (threshold_ > 0 && data[2] < threshold_) {
        data += 7;
        continue;
      }

      float l = CLIP(data[3]);
      float t = CLIP(data[4]);
      float r = CLIP(data[5]);
      float b = CLIP(data[6]);
      l = CLIP((l - 0.5f) * scaling_factor_w + 0.5f);
      t = CLIP((t - 0.5f) * scaling_factor_h + 0.5f);
      r = CLIP((r - 0.5f) * scaling_factor_w + 0.5f);
      b = CLIP((b - 0.5f) * scaling_factor_h + 0.5f);
      if (r <= l || b <= t) {
        data += 7;
        continue;
      }

      auto obj = std::make_shared<cnstream::CNInferObject>();
      uint32_t id = static_cast<uint32_t>(data[1]);
      obj->id = std::to_string(id);
      obj->score = data[2];
      obj->bbox.x = l;
      obj->bbox.y = t;
      obj->bbox.w = std::min(1.0f - l, r - l);
      obj->bbox.h = std::min(1.0f - t, b - t);

      if (!labels.empty() && id < labels[0].size()) {
        obj->AddExtraAttribute("Category", labels[0][id]);
      }

      objs_holder->objs_.push_back(obj);
      data += 7;
    }
  }  // for(batch_idx)
  return 0;
}
