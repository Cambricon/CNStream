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
   * model_path: The path of the offline model.
   * func_name: The function name that is defined in the offline model. It could be found in Cambricon twins file. For most cases, it is "subnet0".
   * postproc_name: The class name for postprocessing. See cnstream::Postproc.
   * preproc_name: The class name for preprocessing on CPU. See cnstream::Preproc.
   * device_id: MLU device ordinal number.
   * batch_size: The batch size. The maximum value is 32. The default value if 1. Only active on MLU100.
   * batching_timeout: The batching timeout. The default value is 3000.0[ms]. type[float]. unit[ms].
   *@endverbatim
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
   * @brief Checks parameters for a module.
   *
   * @param paramSet Parameters of this module.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet& paramSet) const override;

 protected:
  DECLARE_PRIVATE(d_ptr_, Inferencer);
};  // class Inferencer

}  // namespace cnstream

#endif  // MODULES_INFERENCE_INCLUDE_INFERENCER_HPP_
