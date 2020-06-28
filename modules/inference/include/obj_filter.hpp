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

#ifndef MODULES_INFERENCE_INCLUDE_OBJ_FILTER_HPP_
#define MODULES_INFERENCE_INCLUDE_OBJ_FILTER_HPP_

/**
 *  \file preproc.hpp
 *
 *  This file contains a declaration of class Preproc
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
using CNInferObjectPtr = std::shared_ptr<CNInferObject>;

/**
 * @brief Base class of object filter
 */
class ObjFilter : virtual public ReflexObjectEx<ObjFilter> {
 public:
  /**
   * @brief do nothing
   */
  virtual ~ObjFilter() {}
  /**
   * @brief create relative object filter
   *
   * @param filter_name obj filter class name
   *
   * @return None
   */
  static ObjFilter* Create(const std::string& filter_name);

  /**
   * @brief Execute preproc on neural network inputs
   *
   * @param finfo: smart pointer of struct to store origin frame data, (object is detected from this frame)
   * @param obj: object infomations
   *
   * @return return true then this object will be processed by inferencer module,
   *   and return false will be skipped by inferencer module,
   */
  virtual bool Filter(const CNFrameInfoPtr& finfo, const CNInferObjectPtr& pobj) = 0;
};  // class ObjFilter

}  // namespace cnstream

#endif  // ifndef MODULES_INFERENCE_INCLUDE_OBJ_FILTER_HPP_
