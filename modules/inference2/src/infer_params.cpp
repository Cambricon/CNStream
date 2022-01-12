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
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <functional>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <map>
#include <vector>

#include "infer_params.hpp"
#define ASSERT(value) {                                 \
  bool __attribute__((unused)) ret = (value);           \
  assert(ret);                                          \
}
using ModuleParamSet = std::map<std::string, std::string>;

namespace cnstream {

static bool STR2BOOL(const std::string &value, bool *ret) {
  if (!ret) return false;
  static const std::set<std::string> true_value_list = {
    "1", "true", "True", "TRUE"
  };
  static const std::set<std::string> false_value_list = {
    "0", "false", "False", "FALSE"
  };

  if (true_value_list.find(value) != true_value_list.end()) {
    *ret = true;
    return true;
  }
  if (false_value_list.find(value) != false_value_list.end()) {
    *ret = false;
    return true;
  }
  return false;
}

static bool STR2U32(const std::string &value, uint32_t *ret) {
  if (!ret) return false;
  unsigned long t = 0;  // NOLINT
  try {
    t = stoul(value);
    if (t > std::numeric_limits<uint32_t>::max()) return false;
    *ret = t;
  } catch (std::exception& e) {
    return false;
  }
  return true;
}

static bool STR2FLOAT(const std::string &value, float *ret) {
  if (!ret) return false;
  try {
    *ret = stof(value);
  } catch (std::exception& e) {
    return false;
  }
  return true;
}

static std::vector<float> STR2VECTORFLOAT(const std::string &value) {
  try {
    std::vector<float> res;
    std::istringstream ss(value);
    std::string tmp;
    int i = 0;
    while (ss >> tmp) {
      res.push_back(stof(tmp));
      i++;
    }
    if (i > 4) return {};
    return res;
  } catch (std::exception &e) {
    return {};
  }
}

void Infer2ParamManager::RegisterAll(ParamRegister *pregister) {
  Infer2ParamDesc param;
  param.name = "model_path";
  param.desc_str = "Required. The path of the offline model.";
  param.default_value = "";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    if (value.empty()) return false;
    param_set->model_path = value;
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "func_name";
  param.desc_str = "Optional. The function name that is defined in the offline model. Work only if backend is cnrt. "
                   "It could be found in Cambricon twins file. For most cases, it is \"subnet0\".";
  param.default_value = "subnet0";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    if (infer_server::Predictor::Backend() == "cnrt" && value.empty()) return false;
    param_set->func_name = value;
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "frame_filter_name";
  param.desc_str =
      "Optional. The class name for frame filter. See cnstream::FrameFilter. "
      "The class specified by this name must inherited from class cnstream::FrameFilter. "
      "Frames will not be filtered when this parameter is not set.";
  param.default_value = "";
  param.type = "string";
  param.parser = [](const std::string &value, Infer2Param *param_set) -> bool {
    param_set->frame_filter_name = value;
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "obj_filter_name";
  param.desc_str =
      "Optional. The class name for object filter. See cnstream::ObjFilter. "
      "The class specified by this name must inherited from class cnstream::ObjFilter. "
      "No object will be filtered when this parameter not set.";
  param.default_value = "";
  param.type = "string";
  param.parser = [](const std::string &value, Infer2Param *param_set) -> bool {
    param_set->obj_filter_name = value;
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "postproc_name";
  param.desc_str = "Required. The class name for postprocess. The class specified by this name "
                   "must inherit from class cnstream::VideoPostproc.";
  param.default_value = "";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    if (value.empty()) return false;
    param_set->postproc_name = value;
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "preproc_name";
  param.desc_str =
      "Optional. Preprocess name. These values are accepted.\n"
      " 1. rcop/RCOP. Preprocessing will be done on MLU by ResizeYuv2Rgb operator\n"
      " 2. scaler/SCALER. Preprocessing will be done on Scaler\n"
      " 3. cncv/CNCV. Preprocessing will be done on MLU by CNCV\n"
      " 4. The class name of custom preprocessing. The class specified by this"
      " name must inherit from class cnstream::VideoPreproc.";
  param.default_value = "cncv";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    std::string value_transform = value;
    std::transform(value_transform.begin(), value_transform.end(), value_transform.begin(), ::toupper);
    if ("SCALER" == value_transform) {
      param_set->preproc_name = "SCALER";
    } else if ("RCOP" == value_transform) {
      param_set->preproc_name = "RCOP";
    } else if ("CNCV" == value_transform) {
      param_set->preproc_name = "CNCV";
    } else {
      param_set->preproc_name = value;
    }
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "device_id";
  param.desc_str = "Optional. MLU device ordinal number.";
  param.default_value = "0";
  param.type = "uint32";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    return STR2U32(value, &param_set->device_id);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "engine_num";
  param.desc_str = "Optional. infer server engine number. Increase the engine number to improve performance."
                   " However, more MLU resources will be used. It is important to choose a proper number."
                   " Usually, it could be set to the core number of the device / the core number of the model.";
  param.default_value = "1";
  param.type = "uint32";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    bool ret = STR2U32(value, &param_set->engine_num);
    if (ret && param_set->engine_num == 0) ret = false;
    return ret;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "batching_timeout";
  param.desc_str = "Optional. The batching timeout. unit[ms].";
  param.default_value = "1000";
  param.type = "uint32";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    return STR2U32(value, &param_set->batching_timeout);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "batch_strategy";
  param.desc_str = "Optional. The batch strategy. The options are dynamic and static."
                   " Dynamic strategy: high throughput but high latency."
                   " Static strategy: low latency but low throughput.";
  param.default_value = "dynamic";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    std::string value_transform = value;
    std::transform(value_transform.begin(), value_transform.end(), value_transform.begin(), ::toupper);
    if ("STATIC" == value_transform) {
      param_set->batch_strategy = InferBatchStrategy::STATIC;
      return true;
    } else if ("DYNAMIC" == value_transform) {
      param_set->batch_strategy = InferBatchStrategy::DYNAMIC;
      return true;
    }
    return false;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "infer_interval";
  param.desc_str = "Optional. Inferencing one frame every [infer_interval] frames.";
  param.default_value = "1";
  param.type = "uint32";
  param.parser = [](const std::string &value, Infer2Param *param_set) -> bool {
    return STR2U32(value, &param_set->infer_interval);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "priority";
  param.desc_str = "Optional. The priority of this infer task in infer server.";
  param.default_value = "0";
  param.type = "uint32";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    return STR2U32(value, &param_set->priority);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "data_order";
  param.desc_str = "Optional. The order in which the output data of the model are placed.value range : NCHW/NHWC.";
  param.default_value = "NHWC";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    if ("NCHW" == value) {
      param_set->data_order = InferDimOrder::NCHW;
      return true;
    } else if ("NHWC" == value) {
      param_set->data_order = InferDimOrder::NHWC;
      return true;
    }
    return false;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "threshold";
  param.desc_str = "Optional. The threshold will be set to postprocessing.";
  param.default_value = "0";
  param.type = "float";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    return STR2FLOAT(value, &param_set->threshold);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "show_stats";
  param.desc_str = "Optional. Whether show performance statistics. "
                   "1/true/TRUE/True/0/false/FALSE/False these values are accepted.";
  param.default_value = "false";
  param.type = "bool";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    return STR2BOOL(value, &param_set->show_stats);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "object_infer";
  param.desc_str = "Optional. if object_infer is set to true, the objects of the frame will be the inputs."
                   " Otherwise, frames will be the inputs."
                   " 1/true/TRUE/True/0/false/FALSE/False these values are accepted.";
  param.default_value = "false";
  param.type = "bool";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    return STR2BOOL(value, &param_set->object_infer);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "keep_aspect_ratio";
  param.desc_str = "Optional. Only when rcop preproc is used, it is valid."
                   " Remain the scale of width and height to constant."
                   " 1/true/TRUE/True/0/false/FALSE/False these values are accepted.";
  param.default_value = "false";
  param.type = "bool";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    return STR2BOOL(value, &param_set->keep_aspect_ratio);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "mean";
  param.desc_str =
      "Optional. Only when mean_std preproc is used, it is valid."
      "For getting the mean value of each pixel in input image."
      "Only a float vector(size 3-4) are accepted.";
  param.default_value = {};
  param.type = "float[]";
  param.parser = [](const std::string &value, Infer2Param *param_set) -> bool {
    if (value.empty()) return true;
    param_set->mean_ = STR2VECTORFLOAT(value);
    if (param_set->mean_.empty()) return false;
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "std";
  param.desc_str =
      "Optional. Only when mean_std preproc is used, it is valid."
      "For getting the std value of each pixel in input image."
      "Only a float vector(size 3-4) are accepted.";
  param.default_value = {};
  param.type = "float[]";
  param.parser = [](const std::string &value, Infer2Param *param_set) -> bool {
    if (value.empty()) return true;
    param_set->std_ = STR2VECTORFLOAT(value);
    if (param_set->std_.empty()) return false;
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "normalize";
  param.desc_str =
      "Optional. Only when mean_std preproc is used, it is valid."
      "For multiplying 255 in std and mean array."
      "1/true/TRUE/True/0/false/FALSE/False these values are accepted.";
  param.default_value = "false";
  param.type = "bool";
  param.parser = [](const std::string &value, Infer2Param *param_set) -> bool {
    return STR2BOOL(value, &param_set->normalize);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "model_input_pixel_format";
  param.desc_str = "Optional. The pixel format of the model input image."
                   " For using RCOP preproc ARGB32/ABGR32/RGBA32/BGRA32 are supported."
                   " For using Custom preproc RGB24/BGR24/ARGB32/ABGR32/RGBA32/BGRA32 are supported."
                   " This parameter does not take effect when SCALER preproc is used.";

  param.default_value = "RGBA32";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Param *param_set) -> bool {
    if ("RGBA32" == value) param_set->model_input_pixel_format = InferVideoPixelFmt::RGBA;
    else if ("BGRA32" == value) param_set->model_input_pixel_format = InferVideoPixelFmt::BGRA;
    else if ("ARGB32" == value) param_set->model_input_pixel_format = InferVideoPixelFmt::ARGB;
    else if ("ABGR32" == value) param_set->model_input_pixel_format = InferVideoPixelFmt::ABGR;
    else if ("RGB24" == value) param_set->model_input_pixel_format = InferVideoPixelFmt::RGB24;
    else if ("BGR24" == value) param_set->model_input_pixel_format = InferVideoPixelFmt::BGR24;
    else
      return false;
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "custom_preproc_params";
  param.desc_str =
      "Optional. Custom preprocessing parameters. After the inferencer module creates an instance of "
      "the preprocessing class specified by preproc_name or obj_preproc_name, the Init function of the specified "
      "preprocessing class will be called, and these parameters will be passed to Init. See Preproc::Init "
      "and ObjPreproc::Init for detail.";
  param.default_value = "";
  param.type = "json string";
  param.parser = [](const std::string &value, Infer2Param *param_set) -> bool {
    if (value.empty()) {
      param_set->custom_preproc_params.clear();
      return true;
    }
    rapidjson::Document doc;
    if (doc.Parse<rapidjson::kParseCommentsFlag>(value.c_str()).HasParseError()) {
      LOGE(CORE) << "Parse custom preprocessing parameters configuration failed. "
                    "Error code [" << std::to_string(doc.GetParseError()) << "]"
                 << " Offset [" << std::to_string(doc.GetErrorOffset()) << "]. JSON:" << value;
      return false;
    }
    param_set->custom_preproc_params.clear();
    for (auto iter = doc.MemberBegin(); iter != doc.MemberEnd(); ++iter) {
      std::string value;
      if (!iter->value.IsString()) {
        rapidjson::StringBuffer sbuf;
        rapidjson::Writer<rapidjson::StringBuffer> jwriter(sbuf);
        iter->value.Accept(jwriter);
        value = sbuf.GetString();
      } else {
        value = iter->value.GetString();
      }
      param_set->custom_preproc_params[iter->name.GetString()] = iter->value.GetString();
    }
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "custom_postproc_params";
  param.desc_str =
      "Optional. Custom postprocessing parameters. After the inferencer module creates an instance of "
      "the postprocessing class specified by postproc_name or obj_postproc_name, the Init function of the specified "
      "postprocessing class will be called, and these parameters will be passed to Init. See Postproc::Init "
      "and ObjPostproc::Initfor detail.";
  param.default_value = "";
  param.type = "json string";
  param.parser = [](const std::string &value, Infer2Param *param_set) -> bool {
    if (value.empty()) {
      param_set->custom_postproc_params.clear();
      return true;
    }
    rapidjson::Document doc;
    if (doc.Parse<rapidjson::kParseCommentsFlag>(value.c_str()).HasParseError()) {
      LOGE(CORE) << "Parse custom postprocessing parameters configuration failed. "
                    "Error code [" << std::to_string(doc.GetParseError()) << "]"
                 << " Offset [" << std::to_string(doc.GetErrorOffset()) << "]. JSON:" << value;
      return false;
    }
    param_set->custom_postproc_params.clear();
    for (auto iter = doc.MemberBegin(); iter != doc.MemberEnd(); ++iter) {
      std::string value;
      if (!iter->value.IsString()) {
        rapidjson::StringBuffer sbuf;
        rapidjson::Writer<rapidjson::StringBuffer> jwriter(sbuf);
        iter->value.Accept(jwriter);
        value = sbuf.GetString();
      } else {
        value = iter->value.GetString();
      }
      param_set->custom_postproc_params[iter->name.GetString()] = value;
    }
    return true;
  };
  ASSERT(RegisterParam(pregister, param));
}

bool Infer2ParamManager::RegisterParam(ParamRegister *pregister, const Infer2ParamDesc &param_desc) {
  if (!pregister) return false;
  if (!param_desc.IsLegal()) return false;
  auto insert_ret = param_descs_.insert(param_desc);
  if (!insert_ret.second) return false;
  std::string desc = param_desc.desc_str + " --- "
                   + "type : [" + param_desc.type  + "] --- "
                   + "default value : [" + param_desc.default_value + "]";
  pregister->Register(param_desc.name, desc);
  return true;
}

bool Infer2ParamManager::ParseBy(const ModuleParamSet &raw_params, Infer2Param *pout) {
  if (!pout) return false;
  ModuleParamSet raws = raw_params;
  for (const Infer2ParamDesc &desc : param_descs_) {
    std::string value = desc.default_value;
    auto it = raws.find(desc.name);
    if (it != raws.end()) {
      value = it->second;
      raws.erase(it);
    }
    if (!desc.parser(value, pout)) {
      LOGE(INFERENCER2) << "Parse parameter [" << desc.name << "] failed. value is [" << value << "]";
      return false;
    }
  }
  for (const auto &it : raws) {
    if (it.first != "json_file_dir") {
      LOGE(INFERENCER2) << "Parameter named [" << it.first << "] did not registered.";
      return false;
    }
  }
  return true;
}

}  // namespace cnstream

