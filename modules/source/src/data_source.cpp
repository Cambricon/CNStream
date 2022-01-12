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
#include "data_source.hpp"

#include <algorithm>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "cnstream_logging.hpp"
#include "profiler/module_profiler.hpp"

namespace cnstream {

DataSource::DataSource(const std::string &name) : SourceModule(name) {
  param_register_.SetModuleDesc(
      "DataSource is a module for handling input data (videos or images)."
      " Feed data to codec and send decoded data to the next module if there is one.");
  param_register_.Register("output_type",
                           "Where the outputs will be stored. It could be cpu or mlu,"
                           "It is used when decoder_type is cpu.");
  param_register_.Register("device_id", "Which device will be used. If there is only one device, it might be 0.");
  param_register_.Register("interval",
                           "How many frames will be discarded between two frames"
                           " which will be sent to next modules.");
  param_register_.Register("decoder_type", "Which the input data will be decoded by. It could be cpu or mlu.");
  param_register_.Register("reuse_cndec_buf",
                           "This parameter decides whether the codec buffer that stores output data"
                           "will be held and reused by the framework afterwards. It should be true or false.");
  param_register_.Register("input_buf_number",
                           "Codec buffer number for storing input data."
                           " Basically, we do not need to set it, as it will be allocated automatically.");
  param_register_.Register("output_buf_number",
                           "Codec buffer number for storing output data."
                           " Basically, we do not need to set it, as it will be allocated automatically.");
  param_register_.Register("only_key_frame", "Only decode key frames and other frames are discarded. Default is false");
  param_register_.Register("apply_stride_align_for_scaler",
                           "The output data will align the scaler(hardware on mlu220) requirements."
                           " Recommended for use with scaler on mlu220 platforms.");
}

DataSource::~DataSource() {}

static int GetDeviceId(ModuleParamSet paramSet) {
  if (paramSet.find("device_id") == paramSet.end()) {
    return -1;
  }
  std::stringstream ss;
  int device_id;
  ss << paramSet["device_id"];
  ss >> device_id;
  /*check device_id valid or not,FIXME*/
  return device_id;
}

bool DataSource::Open(ModuleParamSet paramSet) {
  if(!CheckParamSet(paramSet)) {
    return false;
  }
  if (paramSet.find("output_type") != paramSet.end()) {
    std::string out_type = paramSet["output_type"];
    if (out_type == "cpu") {
      param_.output_type_ = OutputType::OUTPUT_CPU;
    } else if (out_type == "mlu") {
      param_.output_type_ = OutputType::OUTPUT_MLU;
    } else {
      LOGE(SOURCE) << "output_type " << paramSet["output_type"] << " not supported";
      return false;
    }
    if (param_.output_type_ == OutputType::OUTPUT_MLU) {
      param_.device_id_ = GetDeviceId(paramSet);
      if (param_.device_id_ < 0) {
        LOGE(SOURCE) << "output_type MLU : device_id must be set";
        return false;
      }
    }
  }

  if (paramSet.find("interval") != paramSet.end()) {
    std::stringstream ss;
    int interval;
    ss << paramSet["interval"];
    ss >> interval;
    if (interval <= 0) {
      LOGE(SOURCE) << "interval : invalid";
      return false;
    }
    param_.interval_ = interval;
  }

  if (paramSet.find("decoder_type") != paramSet.end()) {
    std::string dec_type = paramSet["decoder_type"];
    if (dec_type == "cpu") {
      param_.decoder_type_ = DecoderType::DECODER_CPU;
    } else if (dec_type == "mlu") {
      param_.decoder_type_ = DecoderType::DECODER_MLU;
    } else {
      LOGE(SOURCE) << "decoder_type " << paramSet["decoder_type"] << " not supported";
      return false;
    }
    if (dec_type == "mlu") {
      param_.device_id_ = GetDeviceId(paramSet);
      if (param_.device_id_ < 0) {
        LOGE(SOURCE) << "decoder_type MLU : device_id must be set";
        return false;
      }
    }
  }

  if (param_.decoder_type_ == DecoderType::DECODER_MLU) {
    param_.reuse_cndec_buf = false;
    if (paramSet.find("reuse_cndec_buf") != paramSet.end()) {
      if (paramSet["reuse_cndec_buf"] == "true") {
        param_.reuse_cndec_buf = true;
      } else {
        param_.reuse_cndec_buf = false;
      }
    }
  }

  if (paramSet.find("input_buf_number") != paramSet.end()) {
    std::string ibn_str = paramSet["input_buf_number"];
    std::stringstream ss;
    ss << paramSet["input_buf_number"];
    ss >> param_.input_buf_number_;
  }

  if (paramSet.find("output_buf_number") != paramSet.end()) {
    std::string obn_str = paramSet["output_buf_number"];
    std::stringstream ss;
    ss << paramSet["output_buf_number"];
    ss >> param_.output_buf_number_;
  }

  if (paramSet.find("apply_stride_align_for_scaler") != paramSet.end()) {
    param_.apply_stride_align_for_scaler_ = paramSet["apply_stride_align_for_scaler"] == "true";
  }

  if (paramSet.find("only_key_frame") != paramSet.end()) {
    param_.only_key_frame_ = (paramSet["only_key_frame"] == "true");
  }

  return true;
}

void DataSource::Close() { RemoveSources(); }

bool DataSource::CheckParamSet(const ModuleParamSet &paramSet) const {
  bool ret = true;
  ParametersChecker checker;
  for (auto &it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOGW(SOURCE) << "[DataSource] Unknown param: " << it.first;
    }
  }
  int device_id = GetDeviceId(paramSet);
  if (paramSet.find("output_type") != paramSet.end()) {
    if (paramSet.at("output_type") != "cpu" && paramSet.at("output_type") != "mlu") {
      LOGE(SOURCE) << "[DataSource] [output_type] " << paramSet.at("output_type") << " not supported";
      ret = false;
    }
    if (paramSet.at("output_type") == "mlu" && device_id < 0) {
      LOGE(SOURCE) << "[DataSource] [output_type] MLU : device_id must be set";
      ret = false;
    }
  }

  std::string err_msg;
  if (!checker.IsNum({"interval", "input_buf_number", "output_buf_number"}, paramSet, err_msg, true)) {
    LOGE(SOURCE) << "[DataSource] " << err_msg;
    ret = false;
  }

  if (paramSet.find("decoder_type") != paramSet.end()) {
    std::string dec_type = paramSet.at("decoder_type");
    if (dec_type != "cpu" && dec_type != "mlu") {
      LOGE(SOURCE) << "[DataSource] [decoder_type] " << dec_type << " not supported.";
      ret = false;
    }
    if (dec_type == "mlu" && device_id < 0) {
      LOGE(SOURCE) << "[DataSource] [decoder_type] MLU : device_id must be set";
      ret = false;
    }
    if (dec_type == "mlu" && paramSet.find("reuse_cndec_buf") != paramSet.end()) {
      if (paramSet.at("reuse_cndec_buf") != "true" && paramSet.at("reuse_cndec_buf") != "false") {
        LOGE(SOURCE) << "[DataSource] [reuse_cndec_buf] should be true or false";
        ret = false;
      }
    }
  }

  return ret;
}

}  // namespace cnstream
