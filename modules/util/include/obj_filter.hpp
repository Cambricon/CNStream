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

#ifndef MODULES_UTIL_INCLUDE_OBJ_FILTER_HPP_
#define MODULES_UTIL_INCLUDE_OBJ_FILTER_HPP_

/**
 *  \file obj_filter.hpp
 *
 *  This file contains a declaration of class ObjFilter
 */

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "reflex_object.h"

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"

namespace cnstream {

/**
 * @brief The base class of object filter.
 */
class ObjFilter : virtual public ReflexObjectEx<ObjFilter> {
 public:
  /**
   * @brief Does nothing.
   */
  virtual ~ObjFilter() {}
  /**
   * @brief Creates relative object filter.
   *
   * @param filter_name The obj filter class name.
   *
   * @return None
   */
  static ObjFilter* Create(const std::string& filter_name) {
    return ReflexObjectEx<ObjFilter>::CreateObject(filter_name);
  }

  /**
   * @brief Filters objects of the frame.
   *
   * @param finfo: The smart pointer of struct to store origin frame data, (object is detected from this frame).
   * @param obj: The object infomations.
   *
   * @return Returns true if this object is satisfied, otherwise returns false.
   */
  virtual bool Filter(const CNFrameInfoPtr& finfo, const CNInferObjectPtr& pobj) = 0;
};  // class ObjFilter

}  // namespace cnstream

#endif  // ifndef MODULES_UTIL_INCLUDE_OBJ_FILTER_HPP_
