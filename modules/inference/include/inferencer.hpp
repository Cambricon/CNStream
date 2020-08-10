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

#ifndef MODULES_INFERENCE_INCLUDE_INFERENCER_HPP_
#define MODULES_INFERENCE_INCLUDE_INFERENCER_HPP_

/**
 *  \file inferencer.hpp
 *
 *  This file contains a declaration of struct Inferencer and its subtructure.
 */

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "cnstream_error.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

CNSTREAM_REGISTER_EXCEPTION(Inferencer);

class InferencerPrivate;
class InferParamManager;

/**
 * @brief Constructs a pointer to CNFrameInfo.
 */
using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;

/**
 * @brief Inferencer is a module for running offline model inference.
 *
 * @detail
 * The input could come from Decoder or other plugins, in MLU memory
 * or CPU memory. Also, If the ``preproc_name`` parameter is set to ``PreprocCpu``
 * in the Open function or configuration file,
 * CPU is used for image preprocessing. Otherwise, if the ``preproc_name`` parameter is not
 * set, MLU is used for image preprocessing. The image preprocessing includes
 * data shape resizing and color space convertion.
 * Afterwards, you can infer with offline model loading from the model path.
 */
class Inferencer : public Module, public ModuleCreator<Inferencer> {
 public:
  /**
   * @brief Creates Inferencer module.
   *
   * @param name The name of the Inferencer module.
   *
   * @return None
   */
  explicit Inferencer(const std::string& name);
  /**
   * @brief Virtual function that does nothing.
   */
  virtual ~Inferencer();

  /**
   * @brief Called by pipeline when the pipeline is started.
   *
   * @param paramSet:
   * @verbatim
   *   model_path: Required. The path of the offline model.
   *   func_name: Required. The function name that is defined in the offline model.
                  It could be found in Cambricon twins file. For most cases, it is "subnet0".
   *   postproc_name: Required. The class name for postprocess. The class specified by this name must
                      inherited from class cnstream::Postproc when [object_infer] is false, otherwise the
                      class specified by this name must inherit from class cnstream::ObjPostproc.
   *   preproc_name: Optional. The class name for preprocessing on CPU. The class specified by this name must
                     inherited from class cnstream::Preproc when [object_infer] is false, otherwise the class
                     specified by this name must inherit from class cnstream::ObjPreproc. Preprocessing will be
                     done on MLU by ResizeYuv2Rgb (cambricon Bang op) when this parameter not set.
   *   use_scaler: Optional. Whether use the scaler to preprocess the input. The scaler will not be used by default.
   *   device_id: Optional. MLU device ordinal number. The default value is 0.
   *   batching_timeout: Optional. The batching timeout. The default value is 3000.0[ms]. type[float]. unit[ms].
   *   data_order: Optional. Data format. The default format is NHWC.
   *   threshold: Optional. The threshold of the confidence. By default it is 0.
   *   infer_interval: Optional. Process one frame for every ``infer_interval`` frames.
   *   show_stats: Optional. Whether show inferencer performance statistics. It will not be shown by default.
   *   stats_db_name: Required when show_stats is set to ``true``. The directory to store the db file.
                      e.g., ``dir1/dir2/detect.db``.
   *   object_infer: Optional. if object_infer is set to true, the detection target is used as the input to
                     inferencing. if it is set to false, the video frame is used as the input to inferencing.
                     False by default.
   *   obj_filter_name: Optional. The class name for object filter. See cnstream::ObjFilter. This parameter is valid
   when object_infer is true. When this parameter not set, no object will be filtered.
   *   keep_aspect_ratio: Optional. As the mlu is used for image processing, the scale remains constant.
   *   model_input_pixel_format: Optional. As the mlu is used for image processing, set the pixel format of the
   *                             model input image. RGBA32 by default.
   *
   * @endverbatim
   *
   * @return Returns ture if the inferencer has been opened successfully.
   */
  bool Open(ModuleParamSet paramSet) override;
  /**
   * @brief Called by pipeline when the pipeline is stopped.
   *
   * @return Void.
   */
  void Close() override;
  /**
   * @brief Performs inference for each frame.
   *
   * @param data The information and data of frames.
   *
   * @retval 1: The process has run successfully.
   * @retval -1: The process is failed.
   */
  int Process(CNFrameInfoPtr data) final;

  /**
   * @brief Check ParamSet for inferencer..
   *
   * @param param_set Parameters for this module.
   *
   * @return Returns true if this API run successfully. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet &param_set) const override;

 private:
  InferParamManager *param_manager_ = nullptr;
  DECLARE_PRIVATE(d_ptr_, Inferencer);
};  // class Inferencer

}  // namespace cnstream

#endif  // MODULES_INFERENCE_INCLUDE_INFERENCER_HPP_
