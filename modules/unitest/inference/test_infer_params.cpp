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

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "infer_params.hpp"

namespace cnstream {

TEST(Inferencer, infer_param_desc_less_compare) {
  InferParamDesc desc1, desc2;
  desc1.name = "abc";
  desc2.name = "abcd";
  InferParamDescLessCompare less_compare;
  EXPECT_TRUE(less_compare(desc1, desc2));
}

TEST(Inferencer, infer_param_desc_is_legal) {
  InferParamDesc desc;
  desc.name = "abc";
  desc.type = "string";
  desc.parser = [] (const std::string &value, InferParams *param_set) -> bool { return true; };
  EXPECT_TRUE(desc.IsLegal());

  desc.name = "";
  EXPECT_FALSE(desc.IsLegal());

  desc.name = "abc";
  desc.type = "";
  EXPECT_FALSE(desc.IsLegal());

  desc.name = "abc";
  desc.type = "string";
  desc.parser = NULL;
  EXPECT_FALSE(desc.IsLegal());
}

bool InferParamsEQ(const InferParams &p1, const InferParams &p2) {
  return p1.device_id == p2.device_id &&
         p1.object_infer == p2.object_infer &&
         p1.threshold == p2.threshold &&
         p1.use_scaler == p2.use_scaler &&
         p1.infer_interval == p2.infer_interval &&
         p1.batching_timeout == p2.batching_timeout &&
         p1.keep_aspect_ratio == p2.keep_aspect_ratio &&
         p1.data_order == p2.data_order &&
         p1.func_name == p2.func_name &&
         p1.model_path == p2.model_path &&
         p1.preproc_name == p2.preproc_name &&
         p1.postproc_name == p2.postproc_name &&
         p1.obj_filter_name == p2.obj_filter_name &&
         p1.dump_resized_image_dir == p2.dump_resized_image_dir &&
         p1.model_input_pixel_format == p2.model_input_pixel_format &&
         p1.custom_preproc_params == p2.custom_preproc_params &&
         p1.custom_postproc_params == p2.custom_postproc_params;
}

TEST(Inferencer, infer_param_manager) {
  InferParamManager manager;
  ParamRegister param_register;
  manager.RegisterAll(&param_register);

  std::vector<std::string> infer_param_list = {
    "device_id",
    "object_infer",
    "threshold",
    "use_scaler",
    "infer_interval",
    "batching_timeout",
    "keep_aspect_ratio",
    "data_order",
    "func_name",
    "model_path",
    "preproc_name",
    "postproc_name",
    "obj_filter_name",
    "dump_resized_image_dir",
    "model_input_pixel_format",
    "custom_preproc_params",
    "custom_postproc_params"
  };

  for (const auto &it : infer_param_list)
    EXPECT_TRUE(param_register.IsRegisted(it));

  // check parse params right
  InferParams expect_ret;
  expect_ret.device_id = 1;
  expect_ret.object_infer = true;
  expect_ret.threshold = 0.5;
  expect_ret.use_scaler = true;
  expect_ret.infer_interval = 1;
  expect_ret.batching_timeout = 3;
  expect_ret.keep_aspect_ratio = false;
  expect_ret.data_order = edk::DimOrder::NCHW;
  expect_ret.func_name = "fake_name";
  expect_ret.model_path = "fake_path";
  expect_ret.preproc_name = "fake_name";
  expect_ret.postproc_name = "fake_name";
  expect_ret.obj_filter_name = "filter_name";
  expect_ret.dump_resized_image_dir = "dir";
  expect_ret.model_input_pixel_format = CNDataFormat::CN_PIXEL_FORMAT_BGRA32;
  expect_ret.custom_preproc_params = {
    std::make_pair(std::string("param"), std::string("value"))};
  expect_ret.custom_postproc_params = {
    std::make_pair(std::string("param"), std::string("value"))};

  ModuleParamSet raw_params;
  raw_params["device_id"] = std::to_string(expect_ret.device_id);
  raw_params["object_infer"] = std::to_string(expect_ret.object_infer);
  raw_params["threshold"] = std::to_string(expect_ret.threshold);
  raw_params["use_scaler"] = std::to_string(expect_ret.use_scaler);
  raw_params["infer_interval"] = std::to_string(expect_ret.infer_interval);
  raw_params["batching_timeout"] = std::to_string(expect_ret.batching_timeout);
  raw_params["keep_aspect_ratio"] = std::to_string(expect_ret.keep_aspect_ratio);
  raw_params["data_order"] = "NCHW";
  raw_params["func_name"] = expect_ret.func_name;
  raw_params["model_path"] = expect_ret.model_path;
  raw_params["preproc_name"] = expect_ret.preproc_name;
  raw_params["postproc_name"] = expect_ret.postproc_name;
  raw_params["obj_filter_name"] = expect_ret.obj_filter_name;
  raw_params["dump_resized_image_dir"] = expect_ret.dump_resized_image_dir;
  raw_params["model_input_pixel_format"] = "BGRA32";
  raw_params["custom_preproc_params"] = "{\"param\" : \"value\"}";
  raw_params["custom_postproc_params"] = "{\"param\" : \"value\"}";

  {
    InferParams ret;
    EXPECT_TRUE(manager.ParseBy(raw_params, &ret));
    EXPECT_TRUE(InferParamsEQ(expect_ret, ret));
  }

  // check default value
  raw_params.clear();
  {
    InferParams default_value;
    default_value.device_id = 0;
    default_value.object_infer = false;
    default_value.threshold = 0.0;
    default_value.use_scaler = false;
    default_value.infer_interval = 1;
    default_value.batching_timeout = 3000;
    default_value.keep_aspect_ratio = false;
    default_value.data_order = edk::DimOrder::NHWC;
    default_value.func_name = "";
    default_value.model_path = "";
    default_value.preproc_name = "";
    default_value.postproc_name = "";
    default_value.obj_filter_name = "";
    default_value.dump_resized_image_dir = "";
    default_value.model_input_pixel_format = CNDataFormat::CN_PIXEL_FORMAT_RGBA32;

    InferParams ret;
    EXPECT_TRUE(manager.ParseBy(raw_params, &ret));
    EXPECT_FALSE(InferParamsEQ(default_value, ret));
  }

  // check value type
  raw_params.clear();
  {
    InferParams ret;
    raw_params["device_id"] = "wrong";
    EXPECT_FALSE(manager.ParseBy(raw_params, &ret));
  }

  raw_params.clear();
  {
    InferParams ret;
    raw_params["object_infer"] = "wrong";
    EXPECT_FALSE(manager.ParseBy(raw_params, &ret));
  }

  raw_params.clear();
  {
    InferParams ret;
    raw_params["threshold"] = "wrong";
    EXPECT_FALSE(manager.ParseBy(raw_params, &ret));
  }

  raw_params.clear();
  {
    InferParams ret;
    raw_params["use_scaler"] = "wrong";
    EXPECT_FALSE(manager.ParseBy(raw_params, &ret));
  }

  raw_params.clear();
  {
    InferParams ret;
    raw_params["infer_interval"] = "wrong";
    EXPECT_FALSE(manager.ParseBy(raw_params, &ret));
  }

  raw_params.clear();
  {
    InferParams ret;
    raw_params["batching_timeout"] = "wrong";
    EXPECT_FALSE(manager.ParseBy(raw_params, &ret));
  }

  raw_params.clear();
  {
    InferParams ret;
    raw_params["keep_aspect_ratio"] = "2";
    EXPECT_FALSE(manager.ParseBy(raw_params, &ret));
  }

  raw_params.clear();
  {
    InferParams ret;
    raw_params["data_order"] = "CHWN";
    EXPECT_FALSE(manager.ParseBy(raw_params, &ret));
  }

  raw_params.clear();
  {
    InferParams ret;
    raw_params["device_id"] = std::to_string(1ULL << 33);
    EXPECT_FALSE(manager.ParseBy(raw_params, &ret));
  }
}

TEST(Inferencer, custom_preproc_params_parse) {
  InferParamManager manager;
  ParamRegister param_register;
  manager.RegisterAll(&param_register);
  ModuleParamSet raw_params;
  raw_params["custom_preproc_params"] = "{wrong_json_format,}";
  InferParams ret;
  EXPECT_FALSE(manager.ParseBy(raw_params, &ret));
}

TEST(Inferencer, custom_postproc_params_parse) {
  InferParamManager manager;
  ParamRegister param_register;
  manager.RegisterAll(&param_register);
  ModuleParamSet raw_params;
  raw_params["custom_postproc_params"] = "{wrong_json_format,}";
  InferParams ret;
  EXPECT_FALSE(manager.ParseBy(raw_params, &ret));
}

}  // namespace cnstream

