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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "cnstream_logging.hpp"
#include "video_postproc.hpp"

/**
 * @brief Video postprocessing for classification neural network
 */
class VideoPostprocClassification : public cnstream::VideoPostproc {
 public:
  /**
   * @brief Execute secondary classification neural networks postprocessing
   *
   * @param output_data: postproc result. The result of postprocessing should be set to it.
   *                     You could set any type of data to this parameter and get it in UserProcess function.
   * @param model_output: the raw output data from neural network
   * @param model_info: model information, e.g., input/output number, shape and etc.
   *
   * @return return true if succeed
   * @see VideoObjPostprocClassification::UserProcess
   */
  bool Execute(infer_server::InferData* output_data, const infer_server::ModelIO& model_output,
               const infer_server::ModelInfo* model_info) override;

  DECLARE_REFLEX_OBJECT_EX(VideoPostprocClassification, cnstream::VideoPostproc)
};  // classd VideoPostprocClassification

IMPLEMENT_REFLEX_OBJECT_EX(VideoPostprocClassification, cnstream::VideoPostproc)

bool VideoPostprocClassification::Execute(infer_server::InferData* output_data,
                                          const infer_server::ModelIO& model_output,
                                          const infer_server::ModelInfo* model_info) {
  LOGF_IF(DEMO, model_info->InputNum() != 1) << "VideoPostprocClassification: model input number is not equal to 1";
  LOGF_IF(DEMO, model_info->OutputNum() != 1) << "VideoPostprocClassification: model output number is not equal to 1";
  LOGF_IF(DEMO, model_output.buffers.size() != 1) << "VideoPostprocClassification: model result size is not equal to 1";
  const float* data = reinterpret_cast<const float*>(model_output.buffers[0].Data());

  auto len = model_info->OutputShape(0).DataCount();
  auto score_ptr = data;

  float max_score = 0;
  int label = 0;
  for (decltype(len) i = 0; i < len; ++i) {
    auto score = *(score_ptr + i);
    if (score > max_score) {
      max_score = score;
      label = i;
    }
  }

  auto obj = std::make_shared<cnstream::CNInferObject>();
  obj->id = std::to_string(label);
  obj->score = max_score;

  cnstream::CNFrameInfoPtr frame = output_data->GetUserData<cnstream::CNFrameInfoPtr>();
  cnstream::CNInferObjsPtr objs_holder = frame->collection.Get<cnstream::CNInferObjsPtr>(cnstream::kCNInferObjsTag);
  std::lock_guard<std::mutex> objs_mutex(objs_holder->mutex_);
  objs_holder->objs_.push_back(obj);
  return true;
}

/**
 * @brief Video postprocessing for secondary classification
 */
class VideoObjPostprocClassification : public cnstream::VideoPostproc {
 public:
  /**
   * @brief Execute secondary classification neural networks postprocessing
   *
   * @param output_data: postproc result. The result of postprocessing should be set to it.
   *                     You could set any type of data to this parameter and get it in UserProcess function.
   * @param model_output: the raw output data from neural network
   * @param model_info: model information, e.g., input/output number, shape and etc.
   *
   * @return return true if succeed
   * @see VideoObjPostprocClassification::UserProcess
   */
  bool Execute(infer_server::InferData* output_data, const infer_server::ModelIO& model_output,
               const infer_server::ModelInfo* model_info) override;

  DECLARE_REFLEX_OBJECT_EX(VideoObjPostprocClassification, cnstream::VideoPostproc)
};  // classd VideoObjPostprocClassification

IMPLEMENT_REFLEX_OBJECT_EX(VideoObjPostprocClassification, cnstream::VideoPostproc)

bool VideoObjPostprocClassification::Execute(infer_server::InferData* output_data,
                                             const infer_server::ModelIO& model_output,
                                             const infer_server::ModelInfo* model_info) {
  LOGF_IF(DEMO, model_info->InputNum() != 1)
      << "VideoObjPostprocClassification: model input number is not equal to 1";
  LOGF_IF(DEMO, model_info->OutputNum() != 1)
      << "VideoObjPostprocClassification: model output number is not equal to 1";
  LOGF_IF(DEMO, model_output.buffers.size() != 1)
      << "VideoObjPostprocClassification: model result size is not equal to 1";

  const float* data = reinterpret_cast<const float*>(model_output.buffers[0].Data());
  auto len = model_info->OutputShape(0).DataCount();
  auto score_ptr = data;

  float max_score = 0;
  int label = 0;
  for (decltype(len) i = 0; i < len; ++i) {
    auto score = *(score_ptr + i);
    if (score > max_score) {
      max_score = score;
      label = i;
    }
  }

  cnstream::CNInferAttr attr;
  attr.id = 0;
  attr.value = label;
  attr.score = max_score;

  std::shared_ptr<cnstream::CNInferObject> obj = output_data->GetUserData<std::shared_ptr<cnstream::CNInferObject>>();
  obj->AddAttribute("classification", attr);
  return true;
}
