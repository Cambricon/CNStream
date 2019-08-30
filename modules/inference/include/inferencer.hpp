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

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "cninfer/cninfer.h"
#include "cninfer/mlu_context.h"
#include "cninfer/mlu_memory_op.h"
#include "cninfer/model_loader.h"
#include "cnpreproc/resize_and_colorcvt.h"

#include "cnstream_error.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

CNSTREAM_REGISTER_EXCEPTION(Inferencer);

class InferencerPrivate;

using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;

/*********************************************************************************
 * @brief Inferencer is a module for running offline model inference.
 *
 * The input could come from Decoder or other plugins, in MLU memory,
 * or CPU memory.
 *
 * Also, if the data shape does not match the model input shape,
 * before inference it will be resized and converted color space on mlu
 * for MLU memory, and on CPU for CPU memory if CPU preproc set.
 * Afterwards, run infer with offline model loading from model path.
 *********************************************************************************/
class Inferencer : public Module, public ModuleCreator<Inferencer> {
 public:
  /******************************************************************************
   * @brief Create Inferencer module
   ****************************************************************************/
  explicit Inferencer(const std::string& name);
  virtual ~Inferencer();

  /*
   * @brief Called by pipeline when pipeline start.
   * @paramSet
   *   model_path: Offline model path
   *   func_name: Function name is defined in offline model.
   *              It could be found in cambricon_twins file.
   *              For most case, it is "subnet0".
   *   postproc_name:
   *   cpu_preproc_name:
   *   device_id:
   *   batch_size:  maximum 32, default 1
   */
  bool Open(ModuleParamSet paramSet) override;
  /*
   * @brief Called by pipeline when pipeline stop.
   */
  void Close() override;
  /******************************************************************************
   * @brief do inference for each frame
   ****************************************************************************/
  int Process(CNFrameInfoPtr data) final;

  /*****************************************************************************
   * @brief inferencing by batch
   *****************************************************************************/
  int ProcessBatch();

 protected:
  InferencerPrivate* d_ptr_ = nullptr;
  DECLARE_PRIVATE(d_ptr_, Inferencer);
};  // class Inferencer

}  // namespace cnstream

#endif  // MODULES_INFERENCE_INCLUDE_INFERENCER_HPP_
