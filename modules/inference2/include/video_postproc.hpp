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

#ifndef MODULES_INFERENCE2_INCLUDE_VIDEO_POSTPROC_HPP_
#define MODULES_INFERENCE2_INCLUDE_VIDEO_POSTPROC_HPP_

/**
 *  \file video_postproc.hpp
 *
 *  This file contains a declaration of class VideoPostproc
 */

#include <memory>
#include <string>
#include <vector>

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "infer_server.h"
#include "processor.h"
#include "reflex_object.h"

namespace cnstream {
/**
 * @brief construct a pointer to CNFrameInfo
 */
using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;

class VideoPostproc : virtual public ReflexObjectEx<VideoPostproc> {
 public:
  /**
   * @brief do nothong
   */
  virtual ~VideoPostproc() = 0;
  /**
   * @brief create relative postprocess
   *
   * @param proc_name postprocess class name
   *
   * @return None
   */
  static VideoPostproc* Create(const std::string& proc_name);
  /**
   * @brief set threshold
   *
   * @param threshold the value between 0 and 1
   *
   * @return void
   */
  void SetThreshold(const float threshold);

  /**
   * @brief Execute postprocessing
   *
   * @param output_data: the postprocessing result. The result of postprocessing should be set to it.
   *                     You could set any type of data to this parameter and get it in UserProcess function.
   * @param model_output: the raw neural network output data
   * @param model_info: model information, e.g., input/output number, shape and etc.
   *
   * @note This function is executed by infer server postproc processor. You could override it to develop custom
   *       postprocessing. However we provide a default function, so that you could ignore this function, and
   *       develop your code in UserProcess function.
   *       Basically, in default function, we set the raw model output to output_data parameter,
   *       so that you could get it in UserProcess function.
   *       To set any type of data to output_data, use this statement,
   *       e.g., `int example_var = 1; output_data->Set(example_var);`
   *
   *  @return return true if succeed
   * @see UserProcess
   */
  virtual bool Execute(infer_server::InferData* output_data, const infer_server::ModelIO& model_output,
                       const infer_server::ModelInfo& model_info) {
    output_data->Set(model_output);
    return true;
  }

  /**
   * @brief User process. Do anything in this function. For example, fill postprocessing result to frame.
   *
   * @param output_data: The postproc result. In Execute function, you could set any type of data to it.
   * @param model_info: model information, e.g., input/output number, shape and etc.
   * @param frame: the CNframeInfo, that will be passed to the next module
   *
   * @note You must override this function, when the inputs are frames.
   * @note The output_data parameter in this function and that in Execute function are the same.
   *       You could get any type of data that you set to output_data in Execute function.
   *       As we provide a default Exectue function, that we set raw model output to output_data,
   *       if you do not override the Execute function, you could get raw model output by
   *       `infer_server::ModelIO model_output = output_data->GetLref<infer_server::ModelIO>();`.
   *       Otherwise, you could get any type you set, e.g., `int example_var = output->GetLref<int>();`
   *
   * @return return true if succeed
   * @see Execute
   */
  virtual bool UserProcess(infer_server::InferDataPtr output_data, const infer_server::ModelInfo& model_info,
                           cnstream::CNFrameInfoPtr frame) {
    return false;
  }

  /**
   * @brief User process. Do anything in this function. For example, fill postprocessing result to frame.
   * 
   * Usually, this function is for secondary neural network
   *
   * @param output_data: The postproc result. In Execute function, you could set any type of data to it.
   * @param model_info: model information, e.g., input/output number, shape and etc.
   * @param frame: the CNframeInfo, that will be passed to the next module
   * @param obj: the CNInferObject, the input object.
   *
   * @note You must override this function, when the inputs are objects.
   * @note The output_data parameter in this function and that in Execute function are the same.
   *       You could get any type of data that you set to output_data in Execute function.
   *       As we provide a default Exectue function, that we set raw model output to output_data,
   *       if you do not override the Execute function, you could get raw model output by
   *       `infer_server::ModelIO model_output = output_data->GetLref<infer_server::ModelIO>();`.
   *       Otherwise, you could get any type you set, e.g., `int example_var = output->GetLref<int>();`
   *
   * @return return true if succeed
   * @see Execute
   */
  virtual bool UserProcess(infer_server::InferDataPtr output_data, const infer_server::ModelInfo& model_info,
                           cnstream::CNFrameInfoPtr frame, std::shared_ptr<cnstream::CNInferObject> obj) {
    return false;
  }

 protected:
  float threshold_ = 0;
};  // class VideoPostproc

}  // namespace cnstream

#endif  // MODULES_INFERENCE2_INCLUDE_VIDEO_POSTPROC_HPP_
