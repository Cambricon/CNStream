/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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
 * @brief Inferencer thread context
 *********************************************************************************/
struct InferContext {
  libstream::MluMemoryOp mem_op;
  libstream::CnInfer infer;
  libstream::MluContext env;
  libstream::MluRCOp rc_op;
  void** mlu_output = nullptr;
  void** cpu_output = nullptr;
  // resize and color convert operator output
  void** mlu_input = nullptr;
};  // struct InferContext

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
class Inferencer : public Module {
 public:
  /******************************************************************************
   * @brief Create Inferencer module
   ****************************************************************************/
  explicit Inferencer(const std::string& name);
  ~Inferencer();

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
   */
  bool Open(ModuleParamSet paramSet) override;
  /*
   * @brief Called by pipeline when pipeline stop.
   */
  void Close() override;
  /******************************************************************************
   * @brief do inference for each frame
   ****************************************************************************/
  int Process(CNFrameInfoPtr data) override;

 private:
  void RunMluRCOp(CNDataFrame* input_data, void* resize_output_data, libstream::MluRCOp* resize_op);
  InferContext* GetInferContext(CNFrameInfoPtr data);
  InferencerPrivate* d_ptr_ = nullptr;
  DECLARE_PRIVATE(d_ptr_, Inferencer);
};  // class Inferencer

}  // namespace cnstream

#endif  // MODULES_INFERENCE_INCLUDE_INFERENCER_HPP_
