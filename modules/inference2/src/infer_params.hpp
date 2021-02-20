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

#ifndef MODULES_INFERENCE_SRC_INFER_PARAMS_HPP_
#define MODULES_INFERENCE_SRC_INFER_PARAMS_HPP_

#include <functional>
#include <set>
#include <string>

#include "cnstream_config.hpp"
#include "cnstream_frame_va.hpp"
#include "cnstream_logging.hpp"
#include "easyinfer/model_loader.h"
#include "infer_base.hpp"

namespace cnstream {

struct Infer2ParamDesc {
  std::string name;
  std::string desc_str;
  std::string default_value;
  std::string type;  // e.g. bool
  std::function<bool(const std::string &value, Infer2Param *param_set)> parser = NULL;
  bool IsLegal() const {
    return name != "" && type != "" && parser;
  }
};  // struct Infer2ParamDesc

struct Infer2ParamDescLessCompare {
  bool operator() (const Infer2ParamDesc &p1, const Infer2ParamDesc &p2) {
    return p1.name < p2.name;
  }
};  // struct Infer2ParamDescLessCompare

class Infer2ParamManager {
 public:
  void RegisterAll(ParamRegister *pregister);

  bool ParseBy(const ModuleParamSet &raw_params, Infer2Param *pout);

 private:
  bool RegisterParam(ParamRegister *pregister, const Infer2ParamDesc &param_desc);
  std::set<Infer2ParamDesc, Infer2ParamDescLessCompare> param_descs_;
};  // struct Infer2ParamManager

}  // namespace cnstream

#endif  // MODULES_INFERENCE_SRC_INFER_PARAMS_HPP_

