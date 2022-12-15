/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_OSD_HANDLER_HPP_
#define MODULES_OSD_HANDLER_HPP_

#include <memory>
#include <string>
#include <vector>

#include "opencv2/imgproc/imgproc.hpp"

#include "cnstream_frame_va.hpp"
#include "reflex_object.h"

using CNObjsVec = cnstream::CNObjsVec;

namespace cnstream {

class OsdHandler : virtual public cnstream::ReflexObjectEx<OsdHandler> {
 public:
  struct DrawInfo {
    infer_server::CNInferBoundingBox bbox;
    std::string basic_info;
    std::vector<std::string> attributes;
    int label_id;
    bool attr_down = true;
  };
  static OsdHandler *Create(const std::string &name);
  virtual ~OsdHandler() {}

  virtual int GetDrawInfo(const CNObjsVec &objects, const std::vector<std::string> &labels,
                          std::vector<DrawInfo> *info) = 0;
};

}  // namespace cnstream

#endif  // MODULES_OSD_HANDLER_HPP_
