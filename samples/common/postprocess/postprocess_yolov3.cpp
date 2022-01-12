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

#include <algorithm>
#include <cmath>
#include <vector>
#include <memory>

#include "cnstream_frame_va.hpp"
#include "postproc.hpp"
#include "cnstream_logging.hpp"

/**
 * @brief Postprocessing for YOLOv3 neural network
 * The input frame of the model should keep aspect ratio.
 */
class PostprocYolov3 : public cnstream::Postproc {
 public:
  int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const std::shared_ptr<cnstream::CNFrameInfo>& package) {
    LOGF_IF(DEMO, model->InputNum() != 1) << "PostprocYolov3: model input number is not equal to 1";
    LOGF_IF(DEMO, model->OutputNum() != 1) << "PostprocYolov3: model output number is not equal to 1";
    LOGF_IF(DEMO, net_outputs.size() != 1) << "PostprocYolov3: model result size is not equal to 1";
    const auto input_sp = model->InputShape(0);
    const int img_w = package->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag)->width;
    const int img_h = package->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag)->height;
    const int model_input_w = static_cast<int>(input_sp.W());
    const int model_input_h = static_cast<int>(input_sp.H());
    const float* net_output = net_outputs[0];

    // scaling factors
    const float scaling_factors = std::min(1.0 * model_input_w / img_w, 1.0 * model_input_h / img_h);

    // The input frame of the model should keep aspect ratio.
    // If mlu resize and convert operator is used as preproc, parameter keep_aspect_ratio of Inferencer module
    // should be set to true in config json file.
    // If cpu preproc is used as preproc, please make sure keep aspect ratio in custom preproc.
    // Scaler does not support keep aspect ratio.
    // If the input frame does not keep aspect ratio, set scaled_w = model_input_w and scaled_h = model_input_h

    // scaled size
    const int scaled_w = scaling_factors * img_w;
    const int scaled_h = scaling_factors * img_h;

    // bboxes
    const int box_num = static_cast<int>(net_output[0]);
    int box_step = 7;
    auto range_0_1 = [](float num) { return std::max(.0f, std::min(1.0f, num)); };
    cnstream::CNInferObjsPtr objs_holder =
      package->collection.Get<cnstream::CNInferObjsPtr>(cnstream::kCNInferObjsTag);
    cnstream::CNObjsVec &objs = objs_holder->objs_;
    for (int box_idx = 0; box_idx < box_num; ++box_idx) {
      float left = range_0_1(net_output[64 + box_idx * box_step + 3]);
      float right = range_0_1(net_output[64 + box_idx * box_step + 5]);
      float top = range_0_1(net_output[64 + box_idx * box_step + 4]);
      float bottom = range_0_1(net_output[64 + box_idx * box_step + 6]);

      // rectify
      left = (left * model_input_w - (model_input_w - scaled_w) / 2) / scaled_w;
      right = (right * model_input_w - (model_input_w - scaled_w) / 2) / scaled_w;
      top = (top * model_input_h - (model_input_h - scaled_h) / 2) / scaled_h;
      bottom = (bottom * model_input_h - (model_input_h - scaled_h) / 2) / scaled_h;
      left = std::max(0.0f, left);
      right = std::max(0.0f, right);
      top = std::max(0.0f, top);
      bottom = std::max(0.0f, bottom);

      auto obj = std::make_shared<cnstream::CNInferObject>();
      obj->id = std::to_string(static_cast<int>(net_output[64 + box_idx * box_step + 1]));
      obj->score = net_output[64 + box_idx * box_step + 2];

      obj->bbox.x = left;
      obj->bbox.y = top;
      obj->bbox.w = std::min(1.0f - obj->bbox.x, right - left);
      obj->bbox.h = std::min(1.0f - obj->bbox.y, bottom - top);

      if (obj->bbox.h <= 0 || obj->bbox.w <= 0 || (obj->score < threshold_ && threshold_ > 0)) continue;
      std::lock_guard<std::mutex> objs_mutex(objs_holder->mutex_);
      objs.push_back(obj);
    }

    return 0;
  }

 private:
  DECLARE_REFLEX_OBJECT_EX(PostprocYolov3, cnstream::Postproc);
};  // class PostprocessYolov3

IMPLEMENT_REFLEX_OBJECT_EX(PostprocYolov3, cnstream::Postproc);
