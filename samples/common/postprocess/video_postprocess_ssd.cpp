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
 * @brief Video postprocessing for ssd neural network
 */
class VideoPostprocSsd : public cnstream::VideoPostproc {
 public:
  /**
   * @brief Execute ssd neural network postprocessing
   *
   * @param output_data: postproc result. The result of postprocessing should be set to it.
   *                     You could set any type of data to this parameter and get it in UserProcess function.
   * @param model_output: the raw output data from neural network
   * @param model_info: model information, e.g., input/output number, shape and etc.
   *
   * @return return true if succeed
   * @see VideoPostprocSsd::UserProcess
   */
  bool Execute(infer_server::InferData* output_data, const infer_server::ModelIO& model_output,
               const infer_server::ModelInfo* model_info) override;

  DECLARE_REFLEX_OBJECT_EX(VideoPostprocSsd, cnstream::VideoPostproc)
};  // class VideoPostprocSsd

IMPLEMENT_REFLEX_OBJECT_EX(VideoPostprocSsd, cnstream::VideoPostproc)

bool VideoPostprocSsd::Execute(infer_server::InferData* output_data,
                               const infer_server::ModelIO& model_output,
                               const infer_server::ModelInfo* model_info) {
  LOGF_IF(DEMO, model_info->InputNum() != 1) << "VideoPostprocSsd: model input number is not equal to 1";
  LOGF_IF(DEMO, model_info->OutputNum() != 1) << "VideoPostprocSsd: model output number is not equal to 1";
  LOGF_IF(DEMO, model_output.buffers.size() != 1) << "VideoPostprocSsd: model result size is not equal to 1";

  cnstream::CNObjsVec objs;

  const float* data = reinterpret_cast<const float*>(model_output.buffers[0].Data());
  unsigned box_num = static_cast<unsigned>(data[0]);
  data += 64;
  for (unsigned bi = 0; bi < box_num; ++bi) {
    if (data[1] == 0) continue;
    if (threshold_ > 0 && data[2] < threshold_) continue;
    std::shared_ptr<cnstream::CNInferObject> object = std::make_shared<cnstream::CNInferObject>();
    object->id = std::to_string(static_cast<int>(data[1] - 1));
    object->score = data[2];
    object->bbox.x = data[3];
    object->bbox.y = data[4];
    object->bbox.w = data[5] - object->bbox.x;
    object->bbox.h = data[6] - object->bbox.y;

    objs.push_back(object);
    data += 7;
  }
  cnstream::CNFrameInfoPtr frame = output_data->GetUserData<cnstream::CNFrameInfoPtr>();
  cnstream::CNInferObjsPtr objs_holder = frame->collection.Get<cnstream::CNInferObjsPtr>(cnstream::kCNInferObjsTag);
  std::lock_guard<std::mutex> objs_mutex(objs_holder->mutex_);
  objs_holder->objs_.insert(objs_holder->objs_.end(), objs.begin(), objs.end());
  return true;
}
