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
#include <vector>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#include "cnstream_frame_va.hpp"
#include "cnstream_module.hpp"
#include "osd.hpp"
#include "test_base.hpp"
#include "osd_handler.hpp"

namespace cnstream {

static constexpr const char *gname = "osd";
static constexpr const char *glabel_path = "../../modules/unitest/data/test_label.txt";
static constexpr const char *img_path = "../../data/images/19.jpg";

class FakeOsdHandler : public cnstream::OsdHandler {
 public:
  FakeOsdHandler() = default;
  ~FakeOsdHandler() = default;
  int GetDrawInfo(const CNObjsVec &objects, const std::vector<std::string> &labels,
                  std::vector<cnstream::OsdHandler::DrawInfo> *info) override {
    return 0;
  };

 private:
  DECLARE_REFLEX_OBJECT_EX(FakeOsdHandler, cnstream::OsdHandler);
};

IMPLEMENT_REFLEX_OBJECT_EX(FakeOsdHandler, cnstream::OsdHandler);


int g_dev_id = 0;

TEST(Osd, Construct) {
  std::shared_ptr<Module> osd = std::make_shared<Osd>(gname);
  EXPECT_STREQ(osd->GetName().c_str(), gname);
}

TEST(Osd, OpenClose) {
  std::shared_ptr<Module> osd = std::make_shared<Osd>(gname);
  ModuleParamSet param;
  EXPECT_TRUE(osd->Open(param));
  param["label_path"] = "test-osd";
  EXPECT_FALSE(osd->Open(param)) << "if can not read labels, function should not return false";
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
  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  data->SetStreamIndex(0);
  data->timestamp = 1000;

  std::string image_path = GetExePath() + img_path;
  cv::Mat img = cv::imread(image_path, cv::IMREAD_COLOR);

  cv::resize(img, img, cv::Size(1920, 1080));

  std::shared_ptr<CNDataFrame> frame = GenerateCNDataFrame(img, g_dev_id);

  data->collection.Add(kCNDataFrameTag, frame);


  std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(11);
  CnInferBbox bbox(0.6, 0.4, 0.6, 1);
  obj->bbox = bbox;
  objs_holder->objs_.push_back(obj);

  auto obj2 = std::make_shared<CNInferObject>();
  obj2->id = std::to_string(12);
  CnInferBbox bbox1(0.1, -0.2, 0.3, 0.4);
  obj2->bbox = bbox1;
  objs_holder->objs_.push_back(obj2);

  for (int i = 0; i < 5; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1;
    CnInferBbox bbox(val, val, val, val);
    obj->bbox = bbox;
    objs_holder->objs_.push_back(obj);
  }

  data->collection.Add(kCNInferObjsTag, objs_holder);
  EXPECT_EQ(osd->Process(data), 0);
  EXPECT_EQ(osd->Process(data), 0);
  // EXPECT_EQ(osd->Process(data), -1);
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
  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  data->SetStreamIndex(0);
  data->timestamp = 1000;

  std::string image_path = GetExePath() + img_path;
  cv::Mat img = cv::imread(image_path, cv::IMREAD_COLOR);

  cv::resize(img, img, cv::Size(1920, 1080));

  std::shared_ptr<CNDataFrame> frame = GenerateCNDataFrame(img, g_dev_id);

  data->collection.Add(kCNDataFrameTag, frame);

  std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(11);
  CnInferBbox bbox(0.6, 0.4, 0.6, 0.3);
  obj->bbox = bbox;
  cnstream::CNInferAttr attr;
  attr.id = 0;
  attr.value = -1;
  attr.score = -1;
  obj->AddAttribute("classification", attr);
  objs_holder->objs_.push_back(obj);

  auto obj2 = std::make_shared<CNInferObject>();
  obj2->id = std::to_string(12);
  {
    CnInferBbox bbox(0.1, -0.2, 0.3, 0.4);
    obj2->bbox = bbox;
  }
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
    CnInferBbox bbox(val, val, val, val);
    obj->bbox = bbox;
    objs_holder->objs_.push_back(obj);
  }

  data->collection.Add(kCNInferObjsTag, objs_holder);

  EXPECT_EQ(osd->Process(data), 0);
  data = cnstream::CNFrameInfo::Create(std::to_string(0), true);  //  eos
  data->collection.Add(kCNDataFrameTag, frame);
  EXPECT_EQ(osd->Process(data), 0);
  osd->OnEos(std::to_string(0));
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
  EXPECT_FALSE(osd->CheckParamSet(param));
}


TEST(Osd, OsdHandler) {
  std::shared_ptr<Module> osd = std::make_shared<Osd>(gname);
  ModuleParamSet param;
  param.clear();
  param["osd_handler"] = "FakeOsdHandler";

  std::string label_path = GetExePath() + glabel_path;
  param["label_path"] = label_path;
  param["logo"] = "Cambricon-test";
  ASSERT_TRUE(osd->Open(param));


  // prepare data
  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  data->SetStreamIndex(0);
  data->timestamp = 1000;

  std::string image_path = GetExePath() + img_path;
  cv::Mat img = cv::imread(image_path, cv::IMREAD_COLOR);

  cv::resize(img, img, cv::Size(1920, 1080));

  std::shared_ptr<CNDataFrame> frame = GenerateCNDataFrame(img, g_dev_id);

  data->collection.Add(kCNDataFrameTag, frame);


  std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(11);
  CnInferBbox bbox(0.6, 0.4, 0.6, 1);
  obj->bbox = bbox;
  objs_holder->objs_.push_back(obj);

  auto obj2 = std::make_shared<CNInferObject>();
  obj2->id = std::to_string(12);
  CnInferBbox bbox1(0.1, -0.2, 0.3, 0.4);
  obj2->bbox = bbox1;
  objs_holder->objs_.push_back(obj2);

  for (int i = 0; i < 5; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1;
    CnInferBbox bbox(val, val, val, val);
    obj->bbox = bbox;
    objs_holder->objs_.push_back(obj);
  }

  data->collection.Add(kCNInferObjsTag, objs_holder);
  EXPECT_EQ(osd->Process(data), 0);
  EXPECT_NE(osd->Process(nullptr), 0);
  EXPECT_EQ(osd->Process(data), 0);
  osd->OnEos(std::to_string(0));
}

}  // namespace cnstream
