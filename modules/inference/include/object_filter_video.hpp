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

#ifndef OBJECT_FILTER_VIDEO_HPP_
#define OBJECT_FILTER_VIDEO_HPP_

/**
 *  \file object_filter_video.hpp
 *
 *  This file contains a declaration of class ObjectFilter
 */

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "reflex_object.h"

namespace cnstream {

using CNInferObjectPtr = std::shared_ptr<CNInferObject>;

/**
 * @brief Base class of video object filter
 */
class ObjectFilterVideo : virtual public ReflexObjectEx<ObjectFilterVideo> {
 public:
  /**
   * @brief do nothing
   */
  virtual ~ObjectFilterVideo() {}
  /**
   * @brief create relative object filter
   *
   * @param name object filter class name
   *
   * @return None
   */
  static ObjectFilterVideo *Create(const std::string &name);
  /**
   * @brief Filter function for object infer
   *
   * @param package: smart pointer of struct to store origin frame data, (object is detected from this frame)
   * @param object: object information
   *
   * @return return true then this object will be processed by inferencer module,
   *   and return false will be skipped by inferencer module.
   */
  virtual bool Filter(const CNFrameInfoPtr package, const CNInferObjectPtr object) {
    if (object->GetExtraAttribute("SkipObject") != "") {
      return false;
    }
    return true;
  }
  /**
   * @brief Config function for object filter
   *
   * @param config: config string for filter
   *
   * @return return true if config successfully, false for failure
   */
  virtual bool Config(const std::vector<std::string> &params) { return true; }
};  // class ObjectFilterVideo

/**
 * @brief object filter for category
 */
class ObjectFilterVideoCategory : public ObjectFilterVideo {
 public:
  bool Filter(const CNFrameInfoPtr package, const CNInferObjectPtr object) override {
    std::string category = object->GetExtraAttribute("Category");
    for (auto &c : categories_) {
      if (c == category || c == object->id) return true;
    }
    return false;
  }

  bool Config(const std::vector<std::string> &params) override {
    if (params.empty()) return false;
    categories_ = params;
    return (!categories_.empty());
  }

 protected:
  std::vector<std::string> categories_;
};  // class ObjectFilterVideoCategory

}  // namespace cnstream

#endif  // OBJECT_FILTER_VIDEO_HPP_
