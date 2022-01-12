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

#ifndef MODULES_UTIL_INCLUDE_FRAME_FILTER_HPP_
#define MODULES_UTIL_INCLUDE_FRAME_FILTER_HPP_

/**
 *  \file frame_filter.hpp
 *
 *  This file contains a declaration of class FrameFilter
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
 * @brief The base class of frame filter.
 */
class FrameFilter : virtual public ReflexObjectEx<FrameFilter> {
 public:
  /**
   * @brief Does nothing.
   */
  virtual ~FrameFilter() {}
  /**
   * @brief Creates relative frame filter.
   *
   * @param frame_filter_name The frame filter class name.
   *
   * @return None
   */
  static FrameFilter* Create(const std::string& frame_filter_name) {
    return ReflexObjectEx<FrameFilter>::CreateObject(frame_filter_name);
  }

  /**
   * @brief Filters frame.
   *
   * @param finfo: The smart pointer of struct to store origin frame data.
   *
   * @return Returns true if this frame is satisfied, otherwise returns false.
   */
  virtual bool Filter(const CNFrameInfoPtr& finfo) = 0;
};  // class FrameFilter

}  // namespace cnstream

#endif  // ifndef MODULES_UTIL_INCLUDE_FRAME_FILTER_HPP_
