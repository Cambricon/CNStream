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

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <list>

#include "cnstream_logging.hpp"

#include "cnstream_postproc.hpp"
#include "cnstream_preproc.hpp"
#include "inferencer.hpp"
#include "test_base.hpp"

namespace cnstream {


TEST(Inferencer, CheckParamSet) {
  std::string model_path = GetExePath() + "../../data/models/" + GetModelInfoStr("resnet50", "name");
  std::string infer_name = "detector";
  std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));

  ModuleParamSet param;
  param.clear();
  param["device_id"] = "0";
  param["postproc"] = "name=empty_postproc";
  param["model_path"] = model_path;
  EXPECT_FALSE(infer->CheckParamSet(param));

  param["preproc"] = "name=empty_preproc";
  EXPECT_TRUE(infer->CheckParamSet(param));
  param["device_id"] = "no_number";  // device id must be a number
  EXPECT_FALSE(infer->CheckParamSet(param));
  param["device_id"] = "0";
  EXPECT_TRUE(infer->CheckParamSet(param));

  param["engine_num"] = "no_number";  // engine num must be a number
  EXPECT_FALSE(infer->CheckParamSet(param));
  param["engine_num"] = "1";
  EXPECT_TRUE(infer->CheckParamSet(param));

  param["batch_timeout"] = "no_number";  // batching timeout must be a number
  EXPECT_FALSE(infer->CheckParamSet(param));
  param["batch_timeout"] = "100";
  EXPECT_TRUE(infer->CheckParamSet(param));

  // batch strategy must be one of them
  std::list<std::string> batch_strategy = {"static", "STATIC", "dynamic", "DYNAMIC"};
  param["batch_strategy"] = "error_type";
  EXPECT_FALSE(infer->CheckParamSet(param));
  for (auto type : batch_strategy) {
    param["batch_strategy"] = type;
    EXPECT_TRUE(infer->CheckParamSet(param));
  }


  // show stats must be one of bool_type
  std::list<std::string> bool_type = {"1", "true", "True", "TRUE", "0", "false", "False", "FALSE"};
  param["show_stats"] = "error_type";
  EXPECT_FALSE(infer->CheckParamSet(param));
  for (auto type : bool_type) {
    param["show_stats"] = type;
    EXPECT_TRUE(infer->CheckParamSet(param));
  }

  // object_infer must be one of bool_type
  // param["object_infer"] = "error_type";
  // EXPECT_FALSE(infer->CheckParamSet(param));
  // for (auto type : bool_type) {
  //   param["object_infer"] = type;
  //   EXPECT_TRUE(infer->CheckParamSet(param));
  // }

  // keep_aspect_ratio must be one of bool_type
  // param["keep_aspect_ratio"] = "error_type";
  // EXPECT_FALSE(infer->CheckParamSet(param));
  // for (auto type : bool_type) {
  //   param["keep_aspect_ratio"] = type;
  //   EXPECT_TRUE(infer->CheckParamSet(param));
  // }

  // normalize must be one of bool_type
  // param["normalize"] = "error_type";
  // EXPECT_FALSE(infer->CheckParamSet(param));
  // for (auto type : bool_type) {
  //   param["normalize"] = type;
  //   EXPECT_TRUE(infer->CheckParamSet(param));
  // }

  // model_input_pixel_format must be one of format
  std::list<std::string> format = {"GRAY", "TENSOR", "RGB24", "BGR24"};
  param["model_input_pixel_format"] = "error_type";
  EXPECT_FALSE(infer->CheckParamSet(param));
  for (auto type : format) {
    param["model_input_pixel_format"] = type;
    EXPECT_TRUE(infer->CheckParamSet(param));
  }

  // TODO(dmh): test mean and std
}

}  // namespace cnstream
