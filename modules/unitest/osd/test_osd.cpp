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

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#include "cnstream_frame_va.hpp"
#include "cnstream_module.hpp"
#include "osd.hpp"
#include "test_base.hpp"

namespace cnstream {

static constexpr const char *gname = "osd";
static constexpr const char *glabel_path = "../../modules/unitest/osd/test_label.txt";

TEST(Osd, Construct) {
  std::shared_ptr<Module> osd = std::make_shared<Osd>(gname);
  EXPECT_STREQ(osd->GetName().c_str(), gname);
}

TEST(Osd, OpenClose) {
  std::shared_ptr<Module> osd = std::make_shared<Osd>(gname);
  ModuleParamSet param;
  EXPECT_TRUE(osd->Open(param));
  param["label_path"] = "test-osd";
  EXPECT_TRUE(osd->Open(param)) << "if can not read labels, function should not return false";
  std::string label_path = GetExePath() + glabel_path;
  param["label_path"] = label_path;
  EXPECT_TRUE(osd->Open(param));
  param.clear();

  param["label_size"] = "large";
  EXPECT_TRUE(osd->Open(param));
  param["label_size"] = "normal";
  EXPECT_TRUE(osd->Open(param));
  param["label_size"] = "large";
  EXPECT_TRUE(osd->Open(param));
  param["label_size"] = "larger";
  EXPECT_TRUE(osd->Open(param));
  param["label_size"] = "small";
  EXPECT_TRUE(osd->Open(param));
  param["label_size"] = "smaller";
  EXPECT_TRUE(osd->Open(param));
  param["label_size"] = "0.9";
  EXPECT_TRUE(osd->Open(param));
  param.clear();

  param["text_scale"] = "1.2";
  param["text_thickness"] = "1.5";
  param["box_thickness"] = "2";
  EXPECT_TRUE(osd->Open(param));

  param["secondary_label_path"] = label_path;
  param["attr_keys"] = "test_key";
  EXPECT_TRUE(osd->Open(param));
  param["logo"] = "Cambricon-test";
  EXPECT_TRUE(osd->Open(param));
  osd->Close();
}

TEST(Osd, Process) {
  // create osd
  std::shared_ptr<Module> osd = std::make_shared<Osd>(gname);
  ModuleParamSet param;
  std::string label_path = GetExePath() + glabel_path;
  param["label_path"] = label_path;
  param["logo"] = "Cambricon-test";
  ASSERT_TRUE(osd->Open(param));

  // prepare data
  int width = 1920;
  int height = 1080;
  cv::Mat img(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  data->SetStreamIndex(0);
  frame->frame_id = 1;
  data->timestamp = 1000;
  frame->width = width;
  frame->height = height;
  void* ptr_cpu[1] = {img.data};
  frame->stride[0] = width;
  frame->ctx.dev_type = DevContext::DevType::CPU;
  frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_BGR24;
  frame->CopyToSyncMem(ptr_cpu, false);
  data->collection.Add(kCNDataFrameTag, frame);

  std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(11);
  CNInferBoundingBox bbox = {0.6, 0.4, 0.6, 0.3};
  obj->bbox = bbox;
  objs_holder->objs_.push_back(obj);

  auto obj2 = std::make_shared<CNInferObject>();
  obj2->id = std::to_string(12);
  bbox = {0.1, -0.2, 0.3, 0.4};
  obj2->bbox = bbox;
  objs_holder->objs_.push_back(obj2);

  for (int i = 0; i < 5; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1;
    CNInferBoundingBox bbox = {val, val, val, val};
    obj->bbox = bbox;
    objs_holder->objs_.push_back(obj);
  }

  data->collection.Add(kCNInferObjsTag, objs_holder);
  EXPECT_EQ(osd->Process(data), 0);
  EXPECT_EQ(osd->Process(data), 0);
  frame->width = -1;
  EXPECT_EQ(osd->Process(data), -1);
}

TEST(Osd, ProcessSecondary) {
  // create osd
  std::shared_ptr<Module> osd = std::make_shared<Osd>(gname);
  ModuleParamSet param;
  std::string label_path = GetExePath() + glabel_path;
  param["label_path"] = label_path;
  param["secondary_label_path"] = label_path;
  param["attr_keys"] = "classification";
  ASSERT_TRUE(osd->Open(param));

  // prepare data
  int width = 1920;
  int height = 1080;
  cv::Mat img(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  data->SetStreamIndex(0);
  frame->frame_id = 1;
  data->timestamp = 1000;
  frame->width = width;
  frame->height = height;
  void* ptr_cpu[1] = {img.data};
  frame->stride[0] = width;
  frame->ctx.dev_type = DevContext::DevType::CPU;
  frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_BGR24;
  frame->CopyToSyncMem(ptr_cpu, false);
  data->collection.Add(kCNDataFrameTag, frame);

  std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(11);
  CNInferBoundingBox bbox = {0.6, 0.4, 0.6, 0.3};
  obj->bbox = bbox;
  cnstream::CNInferAttr attr;
  attr.id = 0;
  attr.value = -1;
  attr.score = -1;
  obj->AddAttribute("classification", attr);
  objs_holder->objs_.push_back(obj);

  auto obj2 = std::make_shared<CNInferObject>();
  obj2->id = std::to_string(12);
  bbox = {0.1, -0.2, 0.3, 0.4};
  obj2->bbox = bbox;
  cnstream::CNInferAttr attr2;
  attr2.id = 0;
  attr2.value = 2;
  attr2.score = 0.6;
  obj2->AddAttribute("classification", attr2);
  objs_holder->objs_.push_back(obj2);

  for (int i = 0; i < 5; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1;
    CNInferBoundingBox bbox = {val, val, val, val};
    obj->bbox = bbox;
    objs_holder->objs_.push_back(obj);
  }

  data->collection.Add(kCNInferObjsTag, objs_holder);
  EXPECT_EQ(osd->Process(data), 0);
}

TEST(Osd, CheckParamSet) {
  std::shared_ptr<Module> osd = std::make_shared<Osd>(gname);
  ModuleParamSet param;
  param.clear();
  EXPECT_TRUE(osd->CheckParamSet(param));

  std::string label_path = GetExePath() + glabel_path;
  param["label_path"] = label_path;
  EXPECT_TRUE(osd->CheckParamSet(param));
  param["label_path"] = "wrong_path";
  EXPECT_FALSE(osd->CheckParamSet(param));
  param.clear();

  param["secondary_label_path"] = label_path;
  EXPECT_TRUE(osd->CheckParamSet(param));
  param["secondary_label_path"] = "wrong_path";
  EXPECT_FALSE(osd->CheckParamSet(param));
  param.clear();

  param["label_size"] = "normal";
  EXPECT_TRUE(osd->CheckParamSet(param));
  param["label_size"] = "large";
  EXPECT_TRUE(osd->CheckParamSet(param));
  param["label_size"] = "larger";
  EXPECT_TRUE(osd->CheckParamSet(param));
  param["label_size"] = "small";
  EXPECT_TRUE(osd->CheckParamSet(param));
  param["label_size"] = "smaller";
  EXPECT_TRUE(osd->CheckParamSet(param));
  param["label_size"] = "0.9";
  EXPECT_TRUE(osd->CheckParamSet(param));
  param["label_size"] = "wrong_size";
  EXPECT_FALSE(osd->CheckParamSet(param));
  param.clear();

  param["text_scale"] = "1.2";
  param["text_thickness"] = "1.5";
  param["box_thickness"] = "1.5";
  EXPECT_TRUE(osd->CheckParamSet(param));
  param["text_scale"] = "wrong_num";
  param["text_thickness"] = "wrong_num";
  param["box_thickness"] = "wrong_num";
  EXPECT_FALSE(osd->CheckParamSet(param));
  param.clear();

  param["test_param"] = "test";
  EXPECT_TRUE(osd->CheckParamSet(param));
}

}  // namespace cnstream
