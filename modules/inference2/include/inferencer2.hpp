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

#ifndef MODULES_INFER_HPP_
#define MODULES_INFER_HPP_
/**
 *  This file contains a declaration of class Inferencer2
 */

#include <memory>
#include <string>
#include <vector>

#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"
#include "device/mlu_context.h"
#include "infer_base.hpp"

namespace cnstream {

class Infer2ParamManager;
/**
 * @brief for inference based on infer_server.
 */
class Inferencer2 : public Module, public ModuleCreator<Inferencer2> {
 public:
  /**
   *  @brief  Generate Inferencer2
   *
   *  @param  Name : Module name
   *
   *  @return None
   */
  explicit Inferencer2(const std::string& name);

  /**
   * @brief Called by pipeline when pipeline start.
   *
   * @param paramSet: parameters for this module.
   *
   * @return whether module open succeed.
   */
  bool Open(ModuleParamSet paramSet) override;

  /**
   * @brief Called by pipeline when pipeline end.
   *
   * @return void.
   */
  void Close() override;

  /**
   * @brief Process each data frame.
   *
   * @param data : Pointer to the frame info.
   *
   * @return whether post data to communicate processor succeed.
   *
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

  /**
   * @brief Check ParamSet for this module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Return true if this API run successfully. Otherwise, return false.
   */
  bool CheckParamSet(const ModuleParamSet& paramSet) const override;

  virtual ~Inferencer2();

 private:
  std::shared_ptr<InferHandler> infer_handler_ = nullptr;  ///< inference2 handler
  Infer2Param infer_params_;
  std::shared_ptr<Infer2ParamManager> param_manager_ = nullptr;
};  // class Inferencer2

}  // namespace cnstream
#endif
