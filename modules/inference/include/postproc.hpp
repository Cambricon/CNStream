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
#include <map>
#include <utility>
#include <vector>

#include "easyinfer/model_loader.h"
#include "reflex_object.h"

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"

namespace cnstream {

/**
 * @class Postproc
 *
 * @brief Postproc is the base class of post process.
 */
class Postproc : virtual public ReflexObjectEx<Postproc> {
 public:
  /**
   * @brief Destructs an object.
   *
   * @return No return value.
   */
  virtual ~Postproc() = 0;
  /**
   * @brief Creates a postprocess object with the given postprocess's class name.
   *
   * @param[in] proc_name The postprocess class name.
   *
   * @return The pointer to postprocess object.
   */
  static Postproc* Create(const std::string& proc_name);
  /**
   * @brief Initializes postprocessing parameters.
   *
   * @param[in] params The postprocessing parameters.
   *
   * @return Returns ture for success, otherwise returns false.
   **/
  virtual bool Init(const std::map<std::string, std::string> &params) { return true; }
  /**
   * @brief Sets threshold.
   *
   * @param[in] threshold The value between 0 and 1.
   *
   * @return No return value.
   */
  void SetThreshold(const float threshold);
  /**
   * @brief Executes postproc on network outputs.
   *
   * @param[in] net_outputs Network outputs, and the data is stored on the host.
   * @param[in] model Model information including input shape and output shape.
   * @param[in,out] package Smart pointer of ``CNFrameInfo`` to store processed data.
   *
   * @return Returns 0 if successful, otherwise returns -1.
   *
   * @note
   * - This function is called by the Inferencer module when the parameter `mem_on_mlu_for_postproc`
           is set to false and `obj_infer` is set to false. See the Inferencer parameter description for details.
   */
  virtual int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
                      const CNFrameInfoPtr& package) { return 0; }

  /**
   * @brief Execute post processing on network outputs.
   *
   * @param[in] net_outputs Network outputs, and the data is stored on the MLU.
   * @param[in] model Model information including input shape and output shape.
   * @param[in,out] packages The batched frames's result of postprocessing.
   *
   * @return Returns 0 if successful, otherwise returns -1.
   *
   * @note
   *  - This function is called by the Inferencer module when the parameter ``mem_on_mlu_for_postproc``
           is set to true and ``obj_infer`` is set to false.
           See the Inferencer parameter description for details.
   */
  virtual int Execute(const std::vector<void*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
                      const std::vector<CNFrameInfoPtr> &packages) { return 0; }

 protected:
  float threshold_ = 0;
};  // class Postproc

/**
 * @class ObjPostproc
 *
 * @brief ObjPostproc is the base class of object post processing.
 */
class ObjPostproc : virtual public ReflexObjectEx<ObjPostproc> {
 public:
  /**
   * @brief Destructs an object.
   *
   * @return No return value.
   */
  virtual ~ObjPostproc() = 0;
  /**
   * @brief Creates a postprocess object with the given postprocess's class name.
   *
   * @param[in] proc_name The postprocess class name.
   *
   * @return The pointer to postprocess object.
   */
  static ObjPostproc* Create(const std::string& proc_name);
  /**
   * @brief Initializes postprocessing parameters.
   *
   * @param[in] params The postprocessing parameters.
   *
   * @return Returns ture for success, otherwise returns false.
   **/
  virtual bool Init(const std::map<std::string, std::string> &params) { return true; }
  /**
   * @brief Sets threshold.
   *
   * @param[in] threshold The value between 0 and 1.
   *
   * @return No return value.
   */
  void SetThreshold(const float threshold);
  /**
   * @brief Executes post processing on network outputs.
   *
   * @param[in] net_outputs Network outputs, and the data is stored on the host.
   * @param[in] model Model information including input shape and output shape.
   * @param[in,out] finfo Smart pointer of ``CNFrameInfo`` to store processed data.
   * @param[in] pobj The deduced object information.
   *
   * @return Returns 0 if successful, otherwise returns -1.
   *
   * @note
   * - This function is called by the Inferencer module when the parameter
          ``mem_on_mlu_for_postproc`` is set to false and ``obj_infer`` is set to true.
          See the Inferencer parameter description for details.
   */
  virtual int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
                      const CNFrameInfoPtr& finfo, const std::shared_ptr<CNInferObject>& pobj) { return 0; }

  /**
   * @brief Execute post processing on network outputs.
   *
   * @param[in] net_outputs Network outputs, and the data is stored on the MLU.
   * @param[in] model Model information including input shape and output shape.
   * @param[in,out] obj_infos The batched frames's result of postprocessing.
   *
   * @return Returns 0 if successful, otherwise returns -1.
   *
   * @note
   * - This function is called by the Inferencer module when the parameter
          ``mem_on_mlu_for_postproc`` is set to true and ``obj_infer`` is set to true.
          See the Inferencer parameter description for details.
   */
  virtual int Execute(const std::vector<void*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
                      const std::vector<std::pair<CNFrameInfoPtr, std::shared_ptr<CNInferObject>>>& obj_infos) {
    return 0;
  }

 protected:
  float threshold_ = 0;
};  // class ObjPostproc

}  // namespace cnstream

#endif  // MODULES_INFERENCE_INCLUDE_POSTPROC_HPP_
