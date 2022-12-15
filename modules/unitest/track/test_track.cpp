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
#include <vector>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "cnis/processor.h"
#include "cnstream_frame_va.hpp"
#include "cnstream_module.hpp"
#include "test_base.hpp"
#include "track.hpp"

namespace cnstream {

static std::string GetDSModelPath() {
  std::string model_path = "../../data/models/" + GetModelInfoStr("feature_extract", "name");
  return model_path;
}

static constexpr const char* g_model_graph = "../../data/models/feature_extract_nhwc.graph";
static constexpr const char* g_model_data = "../../data/models/feature_extract_nhwc.data";

static constexpr const char *gname = "track";
static constexpr const char *ds_track = "FeatureMatch";
static constexpr const char *img_path = "../../data/images/19.jpg";
static constexpr int g_dev_id = 0;
static constexpr int g_channel_id = 0;
static constexpr float g_max_cosine_distance = 0.2f;

TEST(Tracker, Construct) {
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  EXPECT_STREQ(track->GetName().c_str(), gname);
}

TEST(Tracker, CheckParamSet) {
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  EXPECT_TRUE(track->CheckParamSet(param));

  param["model_path"] = "fake_path";
  EXPECT_FALSE(track->CheckParamSet(param));

  param["device_id"] = "fake_id";
  EXPECT_FALSE(track->CheckParamSet(param));

  param["model_path"] = GetExePath() + GetDSModelPath();
  param["device_id"] = std::to_string(g_dev_id);
  EXPECT_TRUE(track->CheckParamSet(param));

  param["max_cosine_distance"] = "fake_distance";
  EXPECT_FALSE(track->CheckParamSet(param));

  param["max_cosine_distance"] = std::to_string(g_max_cosine_distance);
  EXPECT_TRUE(track->CheckParamSet(param));

  param["track_name"] = "no_such_track_name";
  EXPECT_FALSE(track->CheckParamSet(param));

  param["track_name"] = "FeatureMatch";
  EXPECT_TRUE(track->CheckParamSet(param));
  param["engine_num"] = "fake_num";
  EXPECT_FALSE(track->CheckParamSet(param));

  param["engine_num"] = "1";
  EXPECT_TRUE(track->CheckParamSet(param));
  param["no_such_param"] = "no_such_value";
  EXPECT_FALSE(track->CheckParamSet(param));
}

TEST(Tracker, OpenClose) {
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  // Deep Sort On CPU
  param["track_name"] = ds_track;
  EXPECT_TRUE(track->Open(param));
  // Defaul param
  param.clear();
  EXPECT_TRUE(track->Open(param));
  // FeatureMatch On MLU
  param["track_name"] = ds_track;
  param["device_id"] = "0";

  param["model_path"] = GetExePath() + GetDSModelPath();

  EXPECT_TRUE(track->Open(param));

  param["max_cosine_distance"] = "0.06";
  param["engine_num"] = "1";

  param["track_name"] = "no_such_track_name";
  EXPECT_FALSE(track->Open(param));

  param["track_name"] = ds_track;
  param["model_input_pixel_format"] = "BGR24";
  EXPECT_TRUE(track->Open(param));

  param["model_input_pixel_format"] = "GRAY";
  EXPECT_TRUE(track->Open(param));

  param["model_input_pixel_format"] = "TENSOR";
  EXPECT_TRUE(track->Open(param));

  param["model_input_pixel_format"] = "NO_THIS_FORMAT";
  EXPECT_FALSE(track->Open(param));
  track->Close();
}

std::shared_ptr<CNFrameInfo> GenTestData(int iter, int obj_num) {
  // prepare data
  int width = 1920;
  int height = 1080;
  cv::Mat img(height, width, CV_8UC3, cv::Scalar(0, 0, 0));

  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  data->SetStreamIndex(g_channel_id);
  data->timestamp = 1000;

  CnedkBufSurfaceCreateParams create_params;
  create_params.batch_size = 1;
  memset(&create_params, 0, sizeof(create_params));
  create_params.device_id = g_dev_id;
  create_params.batch_size = 1;
  create_params.width = 1920;
  create_params.height = 1080;
  create_params.color_format = CNEDK_BUF_COLOR_FORMAT_NV21;
  create_params.mem_type = CNEDK_BUF_MEM_DEVICE;

  CnedkBufSurface* surf;
  CnedkBufSurfaceCreate(&surf, &create_params);


  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());

  frame->frame_id = 1;
  frame->buf_surf = std::make_shared<cnedk::BufSurfaceWrapper>(surf, false);

  std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
  for (int i = 0; i < obj_num; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1 + 0.01;
    CnInferBbox bbox(val, val, val, val);
    obj->bbox = bbox;
    objs_holder->objs_.push_back(obj);
  }
  data->collection.Add(kCNDataFrameTag, frame);
  data->collection.Add(kCNInferObjsTag, objs_holder);
  return data;
}


std::shared_ptr<CNFrameInfo> GenTestImageData(bool eos = false) {
  // prepare data
  std::string image_path = GetExePath() + img_path;

  cv::Mat img = cv::imread(image_path, cv::IMREAD_COLOR);

  auto data = cnstream::CNFrameInfo::Create("1", eos);
  data->SetStreamIndex(g_channel_id);
  data->timestamp = 1000;

  std::shared_ptr<CNDataFrame> frame = GenerateCNDataFrame(img, g_dev_id);

  std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(1);
  CnInferBbox bbox(0.2, 0.2, 0.6, 0.6);
  obj->bbox = bbox;
  objs_holder->objs_.push_back(obj);
  data->collection.Add(kCNDataFrameTag, frame);
  data->collection.Add(kCNInferObjsTag, objs_holder);
  return data;
}

TEST(Tracker, ProcessMluFeature) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = ds_track;
  param["model_path"] = GetExePath() + GetDSModelPath();

  ASSERT_TRUE(track->Open(param));

  int obj_num = 4;
  int repeat_time = 10;

  for (int n = 0; n < repeat_time; ++n) {
    auto data = GenTestData(n, obj_num);
    EXPECT_EQ(track->Process(data), 0);
    // send eos to ensure data process done
    auto eos = cnstream::CNFrameInfo::Create(std::to_string(0), true);
    eos->SetStreamIndex(g_channel_id);
    EXPECT_EQ(track->Process(eos), 0);

    CNInferObjsPtr objs_holder = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
    for (size_t idx = 0; idx < objs_holder->objs_.size(); ++idx) {
      auto& obj = objs_holder->objs_[idx];
      EXPECT_FALSE(obj->track_id.empty());
    }
  }
}

TEST(Tracker, ProcessCpuFeature) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  ASSERT_TRUE(track->Open(param));

  int repeat_time = 1;

  auto data = GenTestImageData();
  for (int n = 0; n < repeat_time; ++n) {
    EXPECT_EQ(track->Process(data), 0);

    CNInferObjsPtr objs_holder = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
    for (size_t idx = 0; idx < objs_holder->objs_.size(); ++idx) {
      auto& obj = objs_holder->objs_[idx];
      EXPECT_FALSE(obj->track_id.empty());
    }
  }

  EXPECT_NE(track->Process(nullptr), 0);
  // Illegal StreamIndex
  data->SetStreamIndex(128);
  EXPECT_EQ(track->Process(data), -1);
  data = GenTestImageData(true);
  EXPECT_EQ(track->Process(data), 0);
}

TEST(Tracker, ProcessFeatureMatchCPU0) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "FeatureMatch";
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 3;
  auto data = GenTestData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);
}

TEST(Tracker, ProcessFeatureMatchCPU1) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "FeatureMatch";
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 3;
  auto data = GenTestData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);

  CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);

  EXPECT_EQ(track->Process(data), 0);
}

TEST(Tracker, ProcessFeatureMatchCPU2) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "FeatureMatch";
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 3;
  auto data = GenTestData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);

  CNInferObjsPtr objs_holder = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(5);
  CnInferBbox bbox(0.6, 0.6, 0.6, 0.6);
  obj->bbox = bbox;
  objs_holder->objs_.push_back(obj);
  EXPECT_EQ(track->Process(data), 0);
}

TEST(Tracker, ProcessFeatureMatchCPU3) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "FeatureMatch";
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 3;
  auto data = GenTestData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);

  CNInferObjsPtr objs_holder = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(6);
  CnInferBbox bbox(0.6, 0.6, 0.6, 0.6);
  obj->bbox = bbox;
  objs_holder->objs_.push_back(obj);
  EXPECT_EQ(track->Process(data), 0);
}

TEST(Tracker, ProcessFeatureMatchCPU4) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "FeatureMatch";
  ASSERT_TRUE(track->Open(param));

  int obj_num = 4;
  int repeat_time = 10;
  for (int n = 0; n < repeat_time; ++n) {
    auto data = GenTestData(n, obj_num);
    EXPECT_EQ(track->Process(data), 0);
    CNInferObjsPtr objs_holder = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
    for (size_t idx = 0; idx < objs_holder->objs_.size(); ++idx) {
      auto& obj = objs_holder->objs_[idx];
      EXPECT_FALSE(obj->track_id.empty());
    }
  }
}

TEST(Tracker, ProcessFeatureMatchMLU1) {
  // create track
  // std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  // ModuleParamSet param;
  // param["track_name"] = "FeatureMatch";
  // bool use_magicmind = infer_server::Predictor::Backend() == "magicmind";
  // if (use_magicmind) {
  //   param["model_path"] = GetExePath() + GetDSModelPath();
  // } else {
  //   param["model_path"] = GetExePath() + GetDSModelPath();
  // }
  // ASSERT_TRUE(track->Open(param));
  // int iter = 0;
  // int obj_num = 3;
  // auto data = GenTestData(iter, obj_num);

  // CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
  // // invalid fmt
  // frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_RGB24;
  // EXPECT_EQ(track->Process(data), -1);
  // frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_BGR24;
  // EXPECT_EQ(track->Process(data), -1);
  // frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
  // // invalid width and height
  // frame->width = -1;
  // EXPECT_EQ(track->Process(data), -1);

  // frame->height = -1;
  // EXPECT_EQ(track->Process(data), -1);
}

TEST(Tracker, ProcessFeatureMatchMLU2) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "FeatureMatch";
  param["model_path"] = GetExePath() + GetDSModelPath();
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 0;
  auto data = GenTestData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);
  // send eos to ensure data process done
  auto eos = cnstream::CNFrameInfo::Create(std::to_string(0), true);
  eos->SetStreamIndex(g_channel_id);
  EXPECT_EQ(track->Process(eos), 0);
  size_t zero = 0;
  CNInferObjsPtr objs_holder = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
  EXPECT_EQ(objs_holder->objs_.size(), zero);
}

TEST(Tracker, ProcessFeatureMatchMLU3) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "FeatureMatch";
  param["model_path"] = GetExePath() + GetDSModelPath();

  ASSERT_TRUE(track->Open(param));

  int repeat_time = 10;
  int obj_num = 4;
  std::vector<CNFrameInfoPtr> datas(10);
  for (int n = 0; n < repeat_time; ++n) {
    datas[n] = GenTestData(n, obj_num);
    EXPECT_EQ(track->Process(datas[n]), 0);
  }

  // send eos to ensure data process done
  auto eos = cnstream::CNFrameInfo::Create(std::to_string(0), true);
  eos->SetStreamIndex(g_channel_id);
  EXPECT_EQ(track->Process(eos), 0);

  for (int n = 0; n < repeat_time; ++n) {
    auto& data = datas[n];
    CNInferObjsPtr objs_holder = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
    for (size_t idx = 0; idx < objs_holder->objs_.size(); ++idx) {
      auto& obj = objs_holder->objs_[idx];
      EXPECT_FALSE(obj->track_id.empty());
    }
  }
}

TEST(Tracker, ProcessCpuIoUMatch) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  ASSERT_TRUE(track->Open(param));

  auto data = GenTestImageData();
  // not async extract feature
  param["track_name"] = "IoUMatch";
  ASSERT_TRUE(track->Open(param));
  EXPECT_EQ(track->Process(data), 0);
}

}  // namespace cnstream
