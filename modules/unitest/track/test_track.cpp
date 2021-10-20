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
#include "device/mlu_context.h"
#include "easyinfer/mlu_memory_op.h"
#include "test_base.hpp"
#include "track.hpp"

namespace cnstream {

static std::string GetDSModelPath() {
  if (infer_server::Predictor::Backend() == "magicmind") {
    return "../../data/models/feature_extract_nhwc.model";
  }
  edk::MluContext ctx;
  edk::CoreVersion core_ver = ctx.GetCoreVersion();
  std::string model_path = "";
  switch (core_ver) {
    case edk::CoreVersion::MLU220:
      model_path = "../../data/models/feature_extract_for_tracker_b4c4_argb_mlu220.cambricon";
      break;
    case edk::CoreVersion::MLU270:
    default:
      model_path = "../../data/models/feature_extract_for_tracker_b4c4_argb_mlu270.cambricon";
      break;
  }
  return model_path;
}

static constexpr const char* g_model_graph = "../../data/models/feature_extract_nhwc.graph";
static constexpr const char* g_model_data = "../../data/models/feature_extract_nhwc.data";

static constexpr const char *gname = "track";
static constexpr const char *gfunc_name = "subnet0";
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

  bool use_magicmind = infer_server::Predictor::Backend() == "magicmind";
  if (use_magicmind) {
    param["model_path"] = GetExePath() + GetDSModelPath();
  } else {
    param["model_path"] = GetExePath() + GetDSModelPath();
    param["func_name"] = gfunc_name;
  }
  param["device_id"] = std::to_string(g_dev_id);
  EXPECT_TRUE(track->CheckParamSet(param));

  param["max_cosine_distance"] = "fake_distance";
  EXPECT_FALSE(track->CheckParamSet(param));

  param["max_cosine_distance"] = std::to_string(g_max_cosine_distance);
  EXPECT_TRUE(track->CheckParamSet(param));
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
  bool use_magicmind = infer_server::Predictor::Backend() == "magicmind";
  if (use_magicmind) {
    param["model_path"] = GetExePath() + GetDSModelPath();
  } else {
    param["model_path"] = GetExePath() + GetDSModelPath();
    param["func_name"] = gfunc_name;
  }
  EXPECT_TRUE(track->Open(param));
  track->Close();
}

std::shared_ptr<CNFrameInfo> GenTestData(int iter, int obj_num) {
  // prepare data
  int width = 1920;
  int height = 1080;
  cv::Mat img(height, width, CV_8UC3, cv::Scalar(0, 0, 0));

  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  data->SetStreamIndex(g_channel_id);
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  frame->frame_id = 1;
  data->timestamp = 1000;
  frame->width = width;
  frame->height = height;
  void* ptr_cpu[1] = {img.data};
  frame->stride[0] = width;
  frame->ctx.dev_type = DevContext::DevType::CPU;
  frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_BGR24;
  frame->dst_device_id = g_dev_id;
  frame->CopyToSyncMem(ptr_cpu, true);

  std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
  for (int i = 0; i < obj_num; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1 + 0.01;
    CNInferBoundingBox bbox = {val, val, val, val};
    obj->bbox = bbox;
    objs_holder->objs_.push_back(obj);
  }
  data->collection.Add(kCNDataFrameTag, frame);
  data->collection.Add(kCNInferObjsTag, objs_holder);
  return data;
}

std::shared_ptr<CNFrameInfo> GenTestYUVData(int iter, int obj_num) {
  // prepare data
  int width = 1920;
  int height = 1080;
  cv::Mat img(height + height / 2, width, CV_8UC1);

  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  data->SetStreamIndex(g_channel_id);
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  frame->frame_id = 1;
  data->timestamp = 1000;
  frame->width = width;
  frame->height = height;
  void* ptr_cpu[2] = {img.data, img.data + height * width};
  frame->stride[0] = width;
  frame->stride[1] = width;
  frame->ctx.dev_type = DevContext::DevType::CPU;
  frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
  frame->dst_device_id = g_dev_id;
  frame->CopyToSyncMem(ptr_cpu, true);

  std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
  for (int i = 0; i < obj_num; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1 + 0.01;
    CNInferBoundingBox bbox = {val, val, val, val};
    obj->bbox = bbox;
    objs_holder->objs_.push_back(obj);
  }

  data->collection.Add(kCNDataFrameTag, frame);
  data->collection.Add(kCNInferObjsTag, objs_holder);
  return data;
}

std::shared_ptr<CNFrameInfo> GenTestImageData() {
  // prepare data
  cv::Mat img;
  std::string image_path = GetExePath() + img_path;
  img = cv::imread(image_path, cv::IMREAD_COLOR);

  auto data = cnstream::CNFrameInfo::Create("1", false);
  data->SetStreamIndex(g_channel_id);
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  frame->frame_id = 1;
  data->timestamp = 1000;
  frame->width = img.cols;
  frame->height = img.rows;
  void* ptr_cpu[1] = {img.data};
  frame->stride[0] = img.cols;
  frame->ctx.dev_type = DevContext::DevType::CPU;
  frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_BGR24;
  frame->dst_device_id = g_dev_id;
  frame->CopyToSyncMem(ptr_cpu, true);

  std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(1);
  CNInferBoundingBox bbox = {0.2, 0.2, 0.6, 0.6};
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
  bool use_magicmind = infer_server::Predictor::Backend() == "magicmind";
  if (use_magicmind) {
    param["model_path"] = GetExePath() + GetDSModelPath();
  } else {
    param["model_path"] = GetExePath() + GetDSModelPath();
    param["func_name"] = gfunc_name;
  }
  ASSERT_TRUE(track->Open(param));

  int obj_num = 4;
  int repeat_time = 10;

  for (int n = 0; n < repeat_time; ++n) {
    auto data = GenTestYUVData(n, obj_num);
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

  // Illegal width and height
  CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
  frame->width = -1;
  EXPECT_EQ(track->Process(data), -1);
  frame->width = 1920;
  EXPECT_EQ(track->Process(data), 0);

  frame->height = -1;
  EXPECT_EQ(track->Process(data), -1);
  frame->height = 1080;
  EXPECT_EQ(track->Process(data), 0);

  frame->width = 1920;
  frame->height = 1080;
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
  CNInferBoundingBox bbox = {0.6, 0.6, -0.1, -0.1};
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
  CNInferBoundingBox bbox = {0.6, 0.6, 0.6, 0.6};
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
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "FeatureMatch";
  bool use_magicmind = infer_server::Predictor::Backend() == "magicmind";
  if (use_magicmind) {
    param["model_path"] = GetExePath() + GetDSModelPath();
  } else {
    param["model_path"] = GetExePath() + GetDSModelPath();
    param["func_name"] = gfunc_name;
  }
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 3;
  auto data = GenTestYUVData(iter, obj_num);

  CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
  // invalid fmt
  frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_RGB24;
  EXPECT_EQ(track->Process(data), -1);
  frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_BGR24;
  EXPECT_EQ(track->Process(data), -1);
  frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
  // invalid width and height
  frame->width = -1;
  EXPECT_EQ(track->Process(data), -1);

  frame->height = -1;
  EXPECT_EQ(track->Process(data), -1);
}

TEST(Tracker, ProcessFeatureMatchMLU2) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "FeatureMatch";
  bool use_magicmind = infer_server::Predictor::Backend() == "magicmind";
  if (use_magicmind) {
    param["model_path"] = GetExePath() + GetDSModelPath();
  } else {
    param["model_path"] = GetExePath() + GetDSModelPath();
    param["func_name"] = gfunc_name;
  }
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 0;
  auto data = GenTestYUVData(iter, obj_num);
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
  bool use_magicmind = infer_server::Predictor::Backend() == "magicmind";
  if (use_magicmind) {
    param["model_graph"] = GetExePath() + g_model_graph;
    param["model_data"] = GetExePath() + g_model_data;
  } else {
    param["model_path"] = GetExePath() + GetDSModelPath();
    param["func_name"] = gfunc_name;
  }
  ASSERT_TRUE(track->Open(param));

  int repeat_time = 10;
  int obj_num = 4;
  std::vector<CNFrameInfoPtr> datas(10);
  for (int n = 0; n < repeat_time; ++n) {
    datas[n] = GenTestYUVData(n, obj_num);
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

}  // namespace cnstream
