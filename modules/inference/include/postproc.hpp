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

#ifndef MODULES_INFERENCE_INCLUDE_POSTPROC_HPP_
#define MODULES_INFERENCE_INCLUDE_POSTPROC_HPP_

/**
 *  \file postproc.hpp
 *
 *  This file contains a declaration of class Postproc
 */

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "easyinfer/model_loader.h"
#include "reflex_object.h"

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"

namespace cnstream {
/**
 * @brief construct a pointer to CNFrameInfo
 */
using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;
/**
 * @brief Base class of post process
 */
class Postproc : virtual public ReflexObjectEx<Postproc> {
 public:
  /**
   * @brief do nothong
   */
  virtual ~Postproc() = 0;
  /**
   * @brief create relative postprocess
   *
   * @param proc_name postprocess class name
   *
   * @return None
   */
  static Postproc* Create(const std::string& proc_name);
  /**
   * @brief set threshold
   *
   * @param threshold the value between 0 and 1
   *
   * @return void
   */
  void SetThreshold(const float threshold);

  /**
   * @brief Execute postproc on neural network outputs.
   *
   * @param net_outputs: neural network outputs, the data is stored on the host.
   * @param model: model information(you can get input shape and output shape from model)
   * @param package: smart pointer of struct to store processed result.
   *
   * @return return 0 if succeed.
   *
   * @note this function is called by the inferencer module when the parameter
           `mem_on_mlu_for_postproc` is set to false and `obj_infer` is set to false.
           see the inferencer parameter description for details.
   */
  virtual int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
                      const CNFrameInfoPtr& package) { return 0; }

  /**
   * @brief Execute postproc on neural network outputs.
   *
   * @param net_outputs: neural network outputs, the data is stored on the mlu.
   * @param model: model information(you can get input shape and output shape from model)
   * @param packages: vector of batched frame infomations packages.
   *
   * @return return 0 if succeed.
   *
   * @note this function is called by the inferencer module when the parameter
           `mem_on_mlu_for_postproc` is set to true and `obj_infer` is set to false.
           see the inferencer parameter description for details.
   */
  virtual int Execute(const std::vector<void*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
                      const std::vector<CNFrameInfoPtr> &packages) { return 0; }

 protected:
  float threshold_ = 0;
};  // class Postproc

/**
 * @brief Base class of object post process
 */
class ObjPostproc : virtual public ReflexObjectEx<ObjPostproc> {
 public:
  /**
   * @brief do nothing
   */
  virtual ~ObjPostproc() = 0;
  /**
   * @brief create relative postprocess
   *
   * @param proc_name postprocess class name
   *
   * @return None
   */
  static ObjPostproc* Create(const std::string& proc_name);
  /**
   * @brief set threshold
   *
   * @param threshold the value between 0 and 1
   *
   * @return void
   */
  void SetThreshold(const float threshold);

  /**
   * @brief Execute postproc on neural network outputs
   *
   * @param net_outputs: neural network outputs, the data is stored on the host.
   * @param model: model information(you can get input shape and output shape from model)
   * @param finfo: smart pointer of struct to store processed result
   * @param obj: object infomations
   *
   * @return return 0 if succeed
   *
   * @note this function is called by the inferencer module when the parameter
           `mem_on_mlu_for_postproc` is set to false and `obj_infer` is set to true.
           see the inferencer parameter description for detail.
   */
  virtual int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
                      const CNFrameInfoPtr& finfo, const std::shared_ptr<CNInferObject>& pobj) { return 0; }

  /**
   * @brief Execute postproc on neural network outputs
   *
   * @param net_outputs: neural network outputs, the data is stored on the mlu.
   * @param model: model information(you can get input shape and output shape from model)
   * @param obj_infos: batched object's infomations.
   *
   * @return return 0 if succeed
   *
   * @note this function is called by the inferencer module when the parameter
           `mem_on_mlu_for_postproc` is set to true and `obj_infer` is set to true.
           see the inferencer parameter description for detail.
   */
  virtual int Execute(const std::vector<void*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
                      const std::vector<std::pair<CNFrameInfoPtr, std::shared_ptr<CNInferObject>>> &obj_infos) {
    return 0;
  }

 protected:
  float threshold_ = 0;
};  // class ObjPostproc

}  // namespace cnstream

#endif  // MODULES_INFERENCE_INCLUDE_POSTPROC_HPP_
