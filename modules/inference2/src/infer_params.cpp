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

#include <array>
#include <cassert>
#include <functional>
#include <limits>
#include <set>
#include <string>
#include <unordered_map>

#include "infer_params.hpp"
#define ASSERT(value) {                                 \
  bool __attribute__((unused)) ret = (value);           \
  assert(ret);                                          \
}
using ModuleParamSet = std::unordered_map<std::string, std::string>;

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

void Infer2ParamManager::RegisterAll(ParamRegister *pregister) {
  Infer2ParamDesc param;
  param.name = "model_path";
  param.desc_str = "Required. The path of the offline model.";
  param.default_value = "";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    param_set->model_path = value;
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "func_name";
  param.desc_str = "Required. The function name that is defined in the offline model. "
                   "It could be found in Cambricon twins file. For most cases, it is \"subnet0\".";
  param.default_value = "subnet0";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    if (value.empty()) return false;
    param_set->func_name = value;
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "postproc_name";
  param.desc_str = "Required. The class name for postprocess. The class specified by this name "
                   "must inherited from class cnstream::Postproc when object_infer set to false, "
                   "otherwise the class specified by this name must inherit from class "
                   "cnstream::ObjPostproc.";
  param.default_value = "";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    param_set->postproc_name = value;
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "preproc_name";
  param.desc_str = "Optional. The class name for custom preprocessing. The class specified by this"
                   " name must inherited from class cnstream::Preproc when object_infer is false, "
                   "otherwise the class specified by this name must inherit from class cnstream::ObjPreproc. "
                   "Preprocessing will be done on MLU by ResizeYuv2Rgb when this parameter not "
                   "set and use_scaler set to false.";
  param.default_value = "";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    param_set->preproc_name = value;
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "use_scaler";
  param.desc_str = "Optional. Use scaler to do preprocessing when this parameter set to true and "
                   "preproc_name not set. 1/true/TRUE/True/0/false/FALSE/False these values are accepted.";
  param.default_value = "false";
  param.type = "bool";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    return STR2BOOL(value, &param_set->use_scaler);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "device_id";
  param.desc_str = "Optional. MLU device ordinal number.";
  param.default_value = "0";
  param.type = "uint32";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    return STR2U32(value, &param_set->device_id);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "engine_num";
  param.desc_str = "Optional. infer server engine number.";
  param.default_value = "1";
  param.type = "uint32";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    return STR2U32(value, &param_set->device_id);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "batching_timeout";
  param.desc_str = "Optional. The batching timeout. unit[ms].";
  param.default_value = "3000";
  param.type = "uint32";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    return STR2U32(value, &param_set->batching_timeout);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "batch_strategy";
  param.desc_str = "The batch strategy for modules.";
  param.default_value = "static";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    if ("static" == value || "STATIC" == value) {
      param_set->batch_strategy = "static";
      return true;
    } else if ("dynamic" == value || "DYNAMIC" == value) {
      param_set->batch_strategy = "dynamic";
      return true;
    }
    return false;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "data_order";
  param.desc_str = "Optional. The order in which the output data of the model are placed.value range : NCHW/NHWC.";
  param.default_value = "NHWC";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    if ("NCHW" == value) {
      param_set->data_order = edk::DimOrder::NCHW;
      return true;
    } else if ("NHWC" == value) {
      param_set->data_order = edk::DimOrder::NHWC;
      return true;
    }
    return false;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "threshold";
  param.desc_str = "Optional. The threshold pass to postprocessing function.";
  param.default_value = "0";
  param.type = "float";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    return STR2FLOAT(value, &param_set->threshold);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "infer_interval";
  param.desc_str = "Optional. Inferencing one frame every [infer_interval] frames.";
  param.default_value = "1";
  param.type = "uint32";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    return STR2U32(value, &param_set->infer_interval);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "show_stats";
  param.desc_str = "Optional. Whether show inferencer performance statistics. "
                   "1/true/TRUE/True/0/false/FALSE/False these values are accepted.";
  param.default_value = "false";
  param.type = "bool";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    return STR2BOOL(value, &param_set->show_stats);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "object_infer";
  param.desc_str = "Optional. if object_infer is set to true, the detection target is used as the input to"
                   " inferencing. if it is set to false, the video frame is used as the input to inferencing."
                   " 1/true/TRUE/True/0/false/FALSE/False these values are accepted.";
  param.default_value = "false";
  param.type = "bool";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    return STR2BOOL(value, &param_set->object_infer);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "obj_filter_name";
  param.desc_str = "Optional. The class name for object filter. See cnstream::ObjFilter. "
                   "This parameter is valid when this parameter is true. "
                   "No object will be filtered when this parameter not set.";
  param.default_value = "";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    param_set->obj_filter_name = value;
    return true;
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "keep_aspect_ratio";
  param.desc_str = "Optional. As the mlu is used for image preprocessing, the scale remains constant. "
                   "1/true/TRUE/True/0/false/FALSE/False these values are accepted.";
  param.default_value = "false";
  param.type = "bool";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    return STR2BOOL(value, &param_set->keep_aspect_ratio);
  };
  ASSERT(RegisterParam(pregister, param));

  param.name = "model_input_pixel_format";
  param.desc_str = "Optional. The pixel format of the model input image. ARGB32/ABGR32/RGBA32/BGRA32 are supported.";
  param.default_value = "RGBA32";
  param.type = "string";
  param.parser = [] (const std::string &value, Infer2Params *param_set) -> bool {
    if ("RGBA32" == value) param_set->model_input_pixel_format = CN_PIXEL_FORMAT_RGBA32;
    else if ("BGRA32" == value) param_set->model_input_pixel_format = CN_PIXEL_FORMAT_BGRA32;
    else if ("ARGB32" == value) param_set->model_input_pixel_format = CN_PIXEL_FORMAT_ARGB32;
    else if ("ABGR32" == value) param_set->model_input_pixel_format = CN_PIXEL_FORMAT_ABGR32;
    else
      return false;
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

bool Infer2ParamManager::ParseBy(const ModuleParamSet &raw_params, Infer2Params *pout) {
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

