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
#include <string>
#include <vector>

#include "data_source.hpp"
#include "cnstream_logging.hpp"

namespace cnstream {

DataSource::DataSource(const std::string &name) : SourceModule(name) {
  param_register_.SetModuleDesc(
      "DataSource is a module for handling input data (videos or images)."
      " Feed data to codec and send decoded data to the next module if there is one.");
  param_helper_.reset(new (std::nothrow) ModuleParamsHelper<DataSourceParam>(name));

  static const std::vector<ModuleParamDesc> register_param = {
    {"interval", "1",
    "How many frames will be discarded between two frames which will be sent to next modules.",
    PARAM_OPTIONAL, OFFSET(DataSourceParam, interval), ModuleParamParser<uint32_t>::Parser, "uint32_t"},
    {"bufpool_size", "16", "bufpool size for the stream. Please be noted, on CE3226, it must be VB pool, and video "
     "encoder will use it as well.", PARAM_OPTIONAL, OFFSET(DataSourceParam, bufpool_size),
     ModuleParamParser<uint32_t>::Parser, "uint32_t"},
    {"device_id", "0",
     "Which device will be used. If there is only one device, it might be 0.",
     PARAM_REQUIRED, OFFSET(DataSourceParam, device_id), ModuleParamParser<int>::Parser, "int"}
  };
  param_helper_->Register(register_param, &param_register_);
}

DataSource::~DataSource() {}

bool DataSource::Open(ModuleParamSet param_set) {
  if (!CheckParamSet(param_set)) {
    return false;
  }

  param_ = param_helper_->GetParams();
  uint32_t dev_cnt = 0;
  if (cnrtGetDeviceCount(&dev_cnt) != cnrtSuccess || static_cast<uint32_t>(param_.device_id) >= dev_cnt) {
    LOGE(SOURCE) << "[" << GetName() << "] device " << param_.device_id << " does not exist.";
    return false;
  }

  return true;
}

void DataSource::Close() { RemoveSources(); }

bool DataSource::CheckParamSet(const ModuleParamSet &param_set) const {
  std::string err_msg;
  if (!param_helper_->ParseParams(param_set)) {
    LOGE(SOURCE) << "[" << GetName() << "] parse parameters failed.";
    return false;
  }

  bool ret = true;
  ParametersChecker checker;
  if (!checker.IsNum({"interval", "bufpool_size", "device_id"}, param_set, err_msg, true)) {
    LOGE(SOURCE) << "[DataSource] " << err_msg;
    ret = false;
  }

  return ret;
}

}  // namespace cnstream
