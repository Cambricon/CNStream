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

#ifndef MODULES_INFERENCE_INCLUDE_PREPROC_HPP_
#define MODULES_INFERENCE_INCLUDE_PREPROC_HPP_

/**
 *  \file preproc.hpp
 *
 *  This file contains a declaration of class Preproc
 */

#include <memory>
#include <string>
#include <map>
#include <utility>
#include <vector>

#include "easyinfer/model_loader.h"
#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "reflex_object.h"

namespace cnstream {

/**
 * @class Preproc
 *
 * @brief Preproc is the base class of network preprocessing for inference module.
 */
class Preproc : virtual public ReflexObjectEx<Preproc> {
 public:
  /**
   * @brief Destructs an object.
   *
   * @return No return value.
   */
  virtual ~Preproc() {}
  /**
   * @brief Creates a preprocess object with the given preprocess's class name.
   *
   * @param[in] proc_name The preprocess class name.
   *
   * @return Returns the pointer to preprocess object.
   */
  static Preproc* Create(const std::string& proc_name);
  /**
   * @brief Initializes preprocessing parameters.
   *
   * @param[in] params The preprocessing parameters.
   *
   * @return Returns ture for success, otherwise returns false.
   **/
  virtual bool Init(const std::map<std::string, std::string> &params) { return true; }

  /**
   * @brief Executes preprocess on network inputs.
   *
   * @param[out] net_inputs  Network inputs.
   * @param[in] model  Model information including input shape and output shape.
   * @param[in] package Smart pointer of ``CNFrameInfo`` which stores origin data.
   *
   * @return Returns 0 if successful, otherwise returns -1.
   */
  virtual int Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
                      const CNFrameInfoPtr& package) = 0;
};  // class Preproc

/**
 * @class ObjPreproc
 *
 * @brief ObjPreproc is the base class of preprocess for object.
 */
class ObjPreproc : virtual public ReflexObjectEx<ObjPreproc> {
 public:
  /**
   * @brief Destructs an object.
   *
   * @return No return value.
   */
  virtual ~ObjPreproc() {}
  /**
   * @brief Creates a preprocess object with the given preprocess's class name.
   *
   * @param[in] proc_name The preprocess class name.
   *
   * @return Returns the pointer to preprocess object.
   */
  static ObjPreproc* Create(const std::string& proc_name);
  /**
   * @brief Initializes preprocessing parameters.
   *
   * @param[in] params The preprocessing parameters.
   *
   * @return Returns ture for success, otherwise returns false.
   **/
  virtual bool Init(const std::map<std::string, std::string> &params) { return true; }

  /**
   * @brief Executes preprocess on network inputs.
   *
   * @param[out] net_inputs  Network inputs.
   * @param[in] model  Model information including input shape and output shape.
   * @param[in] finfo Smart pointer of ``CNFrameInfo`` which stores origin data.
   * @param[in] obj The deduced object information.
   *
   * @return Returns 0 if successful, otherwise returns -1.
   */
  virtual int Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
                      const CNFrameInfoPtr& finfo, const std::shared_ptr<CNInferObject>& pobj) = 0;
};  // class ObjPreproc

}  // namespace cnstream

#endif  // ifndef MODULES_INFERENCE_INCLUDE_PREPROC_HPP_
