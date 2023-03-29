/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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
#include <string>
#include <utility>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "cnstream_logging.hpp"
#include "video_postproc.hpp"

/**
 * @brief Video postprocessing for YOLOv5 network.
 */
class VideoPostprocYolov5 : public cnstream::VideoPostproc {
 public:
  /**
   * @brief Execute YOLOv5 network postprocessing.
   *
   * @param[out] output_data: The result of postprocessing should be set to it.
   * @param[in] model_output: The raw network output data.
   * @param[in] model_info: The model information, e.g., input/output number, shape and etc.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   */
  bool Execute(infer_server::InferData* output_data, const infer_server::ModelIO& model_output,
               const infer_server::ModelInfo* model_info) override;

 private:
  DECLARE_REFLEX_OBJECT_EX(VideoPostprocYolov5, cnstream::VideoPostproc);
};  // class VideoPostprocYolov5

IMPLEMENT_REFLEX_OBJECT_EX(VideoPostprocYolov5, cnstream::VideoPostproc);

bool VideoPostprocYolov5::Execute(infer_server::InferData* output_data, const infer_server::ModelIO& model_output,
                                  const infer_server::ModelInfo* model_info) {
  LOGF_IF(DEMO, model_info->InputNum() != 1) << "VideoPostprocYolov5: model input number is not equal to 1";
  LOGF_IF(DEMO, model_info->OutputNum() != 1) << "VideoPostprocYolov5: model output number is not equal to 1";
  LOGF_IF(DEMO, model_output.buffers.size() != 1) << "VideoPostprocYolov5: model result size is not equal to 1";

  cnstream::CNFrameInfoPtr frame = output_data->GetUserData<cnstream::CNFrameInfoPtr>();
  cnstream::CNInferObjsPtr objs_holder = frame->collection.Get<cnstream::CNInferObjsPtr>(cnstream::kCNInferObjsTag);
  cnstream::CNObjsVec &objs = objs_holder->objs_;

  const auto input_sp = model_info->InputShape(0);
  const int img_w = frame->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag)->width;
  const int img_h = frame->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag)->height;

  int w_idx = 2;
  int h_idx = 1;
  if (model_info->InputLayout(0).order == infer_server::DimOrder::NCHW) {
    w_idx = 3;
    h_idx = 2;
  }
  const int model_input_w = static_cast<int>(input_sp[w_idx]);
  const int model_input_h = static_cast<int>(input_sp[h_idx]);

  const float* net_output = reinterpret_cast<const float*>(model_output.buffers[0].Data());

  // scaling factors
  const float scaling_factors = std::min(1.0 * model_input_w / img_w, 1.0 * model_input_h / img_h);

  // The input frame of the model should keep aspect ratio.
  // If mlu resize and convert operator is used as preproc, parameter keep_aspect_ratio of Inferencer2 module
  // should be set to true in config json file.
  // If cpu preproc is used as preproc, please make sure keep aspect ratio in custom preproc.
  // Scaler does not support keep aspect ratio.
  // If the input frame does not keep aspect ratio, set scaled_w = model_input_w and scaled_h = model_input_h

  // scaled size
  const int scaled_w = scaling_factors * img_w;
  const int scaled_h = scaling_factors * img_h;

  // bounding boxes
  const int box_num = static_cast<int>(net_output[0]);
  int box_step = 7;
  auto range_0_1 = [](float num) { return std::max(.0f, std::min(1.0f, num)); };

  for (int box_idx = 0; box_idx < box_num; ++box_idx) {
    float left = net_output[64 + box_idx * box_step + 3];
    float right = net_output[64 + box_idx * box_step + 5];
    float top = net_output[64 + box_idx * box_step + 4];
    float bottom = net_output[64 + box_idx * box_step + 6];

    // rectify
    left = (left - (model_input_w - scaled_w) / 2) / scaled_w;
    right = (right - (model_input_w - scaled_w) / 2) / scaled_w;
    top = (top - (model_input_h - scaled_h) / 2) / scaled_h;
    bottom = (bottom - (model_input_h - scaled_h) / 2) / scaled_h;
    left = range_0_1(left);
    right = range_0_1(right);
    top = range_0_1(top);
    bottom = range_0_1(bottom);

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

  return true;
}
