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
 * @brief Video postprocessing for YOLOv3 neural network
 */
class VideoPostprocYolov3 : public cnstream::VideoPostproc {
 public:
  /**
   * @brief User process. Postprocess on outputs of YOLOv3 neural network and fill data to frame.
   *
   * @param output_data: the raw output data from neural network
   * @param model_info: model information, e.g., input/output number, shape and etc.
   * @param frame: the CNframeInfo, that will be passed to the next module
   *
   * @return return true if succeed
   */
  bool UserProcess(infer_server::InferDataPtr output_data,
                   const infer_server::ModelInfo& model_info,
                   cnstream::CNFrameInfoPtr frame) override;

 private:
  DECLARE_REFLEX_OBJECT_EX(VideoPostprocYolov3, cnstream::VideoPostproc);
};  // class VideoPostprocYolov3

IMPLEMENT_REFLEX_OBJECT_EX(VideoPostprocYolov3, cnstream::VideoPostproc);

bool VideoPostprocYolov3::UserProcess(infer_server::InferDataPtr output_data,
                                      const infer_server::ModelInfo& model_info,
                                      cnstream::CNFrameInfoPtr frame) {
  infer_server::ModelIO model_output = output_data->GetLref<infer_server::ModelIO>();
  LOGF_IF(DEMO, model_info.InputNum() != 1);
  LOGF_IF(DEMO, model_info.OutputNum() != 1);
  LOGF_IF(DEMO, model_output.buffers.size() != 1);

  cnstream::CNInferObjsPtr objs_holder = cnstream::GetCNInferObjsPtr(frame);
  cnstream::CNObjsVec &objs = objs_holder->objs_;

  const auto input_sp = model_info.InputShape(0);
  const int img_w = cnstream::GetCNDataFramePtr(frame)->width;
  const int img_h = cnstream::GetCNDataFramePtr(frame)->height;
  const int model_input_w = static_cast<int>(input_sp.GetW());
  const int model_input_h = static_cast<int>(input_sp.GetH());

  const float* net_output = reinterpret_cast<const float*>(model_output.buffers[0].Data());

  // scaling factors
  const float scaling_factors = std::min(1.0 * model_input_w / img_w, 1.0 * model_input_h / img_h);

  // scaled size
  const int scaled_w = scaling_factors * img_w;
  const int scaled_h = scaling_factors * img_h;

  // bounding boxes
  const int box_num = static_cast<int>(net_output[0]);
  int box_step = 7;
  auto range_0_1 = [](float num) { return std::max(.0f, std::min(1.0f, num)); };

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

  return true;
}

/**
 * @brief Video postprocessing for YOLOv3 neural network when inputs of the network are not keeping aspect ratio
 */
class VideoPostprocFakeYolov3 : public cnstream::VideoPostproc {
 public:
  /**
   * @brief Execute YOLOv3 neural network postprocessing
   *
   * @param output_data: postproc result. The result of postprocessing should be set to it.
   *                     You could set any type of data to this parameter and get it in UserProcess function.
   * @param model_output: the raw output data from neural network
   * @param model_info: model information, e.g., input/output number, shape and etc.
   *
   * @return return true if succeed
   * @see VideoPostprocFakeYolov3::UserProcess
   */
  bool Execute(infer_server::InferData* output_data, const infer_server::ModelIO& model_output,
               const infer_server::ModelInfo& model_info) override;
  /**
   * @brief User process. Fill postprocessing result to frame.
   *
   * @param output_data: The postproc result. In Execute function, you could set any type of data to it.
   * @param model_info: model information, e.g., input/output number, shape and etc.
   * @param frame: the CNframeInfo, that will be passed to the next module
   *
   * @return return true if succeed
   * @see VideoPostprocFakeYolov3::Execute
   */
  bool UserProcess(infer_server::InferDataPtr output_data, const infer_server::ModelInfo& model_info,
                   cnstream::CNFrameInfoPtr frame) override;

 private:
  DECLARE_REFLEX_OBJECT_EX(VideoPostprocFakeYolov3, cnstream::VideoPostproc);
};  // class VideoPostprocFakeYolov3

IMPLEMENT_REFLEX_OBJECT_EX(VideoPostprocFakeYolov3, cnstream::VideoPostproc);

bool VideoPostprocFakeYolov3::Execute(infer_server::InferData* output_data,
                                      const infer_server::ModelIO& model_output,
                                      const infer_server::ModelInfo& model_info) {
  LOGF_IF(DEMO, model_info.InputNum() != 1);
  LOGF_IF(DEMO, model_info.OutputNum() != 1);
  LOGF_IF(DEMO, model_output.buffers.size() != 1);

  cnstream::CNObjsVec objs;

  const float* data = reinterpret_cast<const float*>(model_output.buffers[0].Data());
  unsigned box_num = static_cast<unsigned>(data[0]);
  data += 64;
  for (auto bi = 0u; bi < box_num; ++bi) {
    if (threshold_ > 0 && data[2] < threshold_) continue;
    std::shared_ptr<cnstream::CNInferObject> object = std::make_shared<cnstream::CNInferObject>();
    object->id = std::to_string(data[1]);
    object->score = data[2];
    object->bbox.x = data[3];
    object->bbox.y = data[4];
    object->bbox.w = data[5] - object->bbox.x;
    object->bbox.h = data[6] - object->bbox.y;

    objs.push_back(object);
    data += 7;
  }
  output_data->Set(objs);
  return true;
}

bool VideoPostprocFakeYolov3::UserProcess(infer_server::InferDataPtr output_data,
                                          const infer_server::ModelInfo& model_info,
                                          cnstream::CNFrameInfoPtr frame) {
  cnstream::CNObjsVec result_objs = output_data->GetLref<cnstream::CNObjsVec>();

  // fill result in frame
  cnstream::CNInferObjsPtr objs_holder = cnstream::GetCNInferObjsPtr(frame);
  std::lock_guard<std::mutex> objs_mutex(objs_holder->mutex_);
  objs_holder->objs_.insert(objs_holder->objs_.end(), result_objs.begin(), result_objs.end());
  return true;
}
