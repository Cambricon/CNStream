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
   * @brief User process. Postprocess on outputs of classification neural network and fill data to frame.
   *
   * @param output_data: the raw output data from neural network
   * @param model_info: model information, e.g., input/output number, shape and etc.
   * @param frame: the CNframeInfo, that will be passed to the next module
   *
   * @return return true if succeed
   */
  bool UserProcess(infer_server::InferDataPtr output_data, const infer_server::ModelInfo& model_info,
                   cnstream::CNFrameInfoPtr frame) override;

  DECLARE_REFLEX_OBJECT_EX(VideoPostprocClassification, cnstream::VideoPostproc)
};  // classd VideoPostprocClassification

IMPLEMENT_REFLEX_OBJECT_EX(VideoPostprocClassification, cnstream::VideoPostproc)

bool VideoPostprocClassification::UserProcess(infer_server::InferDataPtr output_data,
                                              const infer_server::ModelInfo& model_info,
                                              cnstream::CNFrameInfoPtr frame) {
  infer_server::ModelIO model_output = output_data->GetLref<infer_server::ModelIO>();

  LOGF_IF(DEMO, model_info.InputNum() != 1);
  LOGF_IF(DEMO, model_info.OutputNum() != 1);
  LOGF_IF(DEMO, model_output.buffers.size() != 1);

  const float* data = reinterpret_cast<const float*>(model_output.buffers[0].Data());

  auto len = model_info.OutputShape(0).DataCount();
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

  if (0 == label) return false;
  auto obj = std::make_shared<cnstream::CNInferObject>();
  obj->id = std::to_string(label);
  obj->score = max_score;

  cnstream::CNInferObjsPtr objs_holder = cnstream::GetCNInferObjsPtr(frame);
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
               const infer_server::ModelInfo& model_info) override;
  /**
   * @brief User process. Fill postprocessing result to frame.
   *
   * @param output_data: The postproc result. In Execute function, you could set any type of data to it.
   * @param model_info: model information, e.g., input/output number, shape and etc.
   * @param frame: the CNframeInfo, that will be passed to the next module
   *
   * @return return true if succeed
   * @see VideoObjPostprocClassification::Execute
   */
  bool UserProcess(infer_server::InferDataPtr output_data, const infer_server::ModelInfo& model_info,
                   cnstream::CNFrameInfoPtr frame, std::shared_ptr<cnstream::CNInferObject> obj) override;

  DECLARE_REFLEX_OBJECT_EX(VideoObjPostprocClassification, cnstream::VideoPostproc)
};  // classd VideoObjPostprocClassification

IMPLEMENT_REFLEX_OBJECT_EX(VideoObjPostprocClassification, cnstream::VideoPostproc)

bool VideoObjPostprocClassification::Execute(infer_server::InferData* output_data,
                                             const infer_server::ModelIO& model_output,
                                             const infer_server::ModelInfo& model_info) {
  LOGF_IF(DEMO, model_info.InputNum() != 1);
  LOGF_IF(DEMO, model_info.OutputNum() != 1);
  LOGF_IF(DEMO, model_output.buffers.size() != 1);

  cnstream::CNInferAttr attr;

  const float* data = reinterpret_cast<const float*>(model_output.buffers[0].Data());
  auto len = model_info.OutputShape(0).DataCount();
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

  if (0 == label) return false;
  attr.id = 0;
  attr.value = label;
  attr.score = max_score;

  output_data->Set(attr);
  return true;
}

bool VideoObjPostprocClassification::UserProcess(infer_server::InferDataPtr output_data,
                                                 const infer_server::ModelInfo& model_info,
                                                 cnstream::CNFrameInfoPtr frame,
                                                 std::shared_ptr<cnstream::CNInferObject> obj) {
  cnstream::CNInferAttr attr = output_data->GetLref<cnstream::CNInferAttr>();
  // fill attribute to object
  obj->AddAttribute("classification", attr);
  return true;
}
