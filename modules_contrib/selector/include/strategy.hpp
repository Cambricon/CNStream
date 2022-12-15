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

#ifndef MODULES_SELECTOR_STRATEGY_HPP_
#define MODULES_SELECTOR_STRATEGY_HPP_

#include <memory>
#include <string>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "cnstream_logging.hpp"
#include "reflex_object.h"

namespace cnstream {

using CNInferObjectPtr = std::shared_ptr<CNInferObject>;

class Strategy : virtual public ReflexObjectEx<Strategy> {
 public:
  static Strategy *Create(const std::string &name);
  virtual ~Strategy() {}

  virtual bool Process(const CNInferObjectPtr &obj, int64_t frame_id) = 0;
  virtual bool Check(const CNInferObjectPtr &obj, int64_t frame_id) { return true; }
  virtual void UpdateFrame() {}
  virtual bool Config(const std::string &params) { return true; }
};

}  // namespace cnstream

#endif  // MODULES_SELECTOR_STRATEGY_HPP_
