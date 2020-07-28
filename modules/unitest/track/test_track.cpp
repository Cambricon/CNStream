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

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#ifdef HAVE_OPENCV
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#else
#error OpenCV required
#endif

#include "cnstream_frame_va.hpp"
#include "cnstream_module.hpp"
#include "easyinfer/mlu_memory_op.h"
#include "test_base.hpp"
#include "track.hpp"

namespace cnstream {

static constexpr const char *gname = "track";
static constexpr const char *gfunc_name = "subnet0";
static constexpr const char *g_dsmodel_path =
    "../../data/models/MLU270/feature_extract/feature_extract_v1.3.0.cambricon";
static constexpr const char *g_kcfmodel_path = "../../data/models/MLU270/KCF/yuv2gray.cambricon";
static constexpr const char *ds_track = "FeatureMatch";
static constexpr const char *kcf_track = "KCF";
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

  param["model_path"] = GetExePath() + g_kcfmodel_path;
  param["func_name"] = gfunc_name;
  param["track_name"] = "fake_name";
  EXPECT_FALSE(track->CheckParamSet(param));

  param["track_name"] = kcf_track;
  EXPECT_TRUE(track->CheckParamSet(param));

  param["device_id"] = "fake_id";
  EXPECT_FALSE(track->CheckParamSet(param));

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
  param["model_path"] = GetExePath() + g_dsmodel_path;
  param["func_name"] = gfunc_name;
  EXPECT_TRUE(track->Open(param));
  // KCF has model and wrong func
  param.clear();
  param["track_name"] = kcf_track;
  param["model_path"] = GetExePath() + g_kcfmodel_path;
  param["func_name"] = "wrong_func_name";
  EXPECT_FALSE(track->Open(param));
  // KCF model name and func name
  param.clear();
  param["track_name"] = kcf_track;
  param["model_path"] = GetExePath() + g_kcfmodel_path;
  param["func_name"] = gfunc_name;
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
  frame->ptr_cpu[0] = img.data;
  frame->stride[0] = width;
  frame->ctx.dev_type = DevContext::DevType::CPU;
  frame->fmt = CN_PIXEL_FORMAT_BGR24;
  frame->CopyToSyncMem();
  data->datas[CNDataFramePtrKey] = frame;

  CNObjsVec objs;
  for (int i = 0; i < obj_num; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1 + 0.01;
    CNInferBoundingBox bbox = {val, val, val, val};
    obj->bbox = bbox;
    objs.push_back(obj);
  }
  data->datas[cnstream::CNObjsVecKey] = objs;
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
  frame->ptr_cpu[0] = img.data;
  frame->stride[0] = width;
  frame->ctx.dev_type = DevContext::DevType::CPU;
  frame->fmt = CN_PIXEL_FORMAT_YUV420_NV21;
  frame->CopyToSyncMem();
  data->datas[CNDataFramePtrKey] = frame;

  CNObjsVec objs;
  for (int i = 0; i < obj_num; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1 + 0.01;
    CNInferBoundingBox bbox = {val, val, val, val};
    obj->bbox = bbox;
    objs.push_back(obj);
  }

  data->datas[cnstream::CNObjsVecKey] = objs;
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
  frame->ptr_cpu[0] = img.data;
  frame->stride[0] = img.cols;
  frame->ctx.dev_type = DevContext::DevType::CPU;
  frame->fmt = CN_PIXEL_FORMAT_BGR24;
  frame->CopyToSyncMem();
  data->datas[CNDataFramePtrKey] = frame;

  CNObjsVec objs;
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(1);
  CNInferBoundingBox bbox = {0.2, 0.2, 0.6, 0.6};
  obj->bbox = bbox;
  objs.push_back(obj);
  data->datas[cnstream::CNObjsVecKey] = objs;
  return data;
}

TEST(Tracker, ProcessMluFeature) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = ds_track;
  param["model_path"] = GetExePath() + g_dsmodel_path;
  param["func_name"] = gfunc_name;
  ASSERT_TRUE(track->Open(param));

  int obj_num = 4;
  int repeat_time = 10;

  for (int n = 0; n < repeat_time; ++n) {
    auto data = GenTestData(n, obj_num);
    EXPECT_EQ(track->Process(data), 0);

    CNObjsVec objs = cnstream::any_cast<CNObjsVec>(data->datas[CNObjsVecKey]);
    for (size_t idx = 0; idx < objs.size(); ++idx) {
      auto& obj = objs[idx];
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

    CNObjsVec objs = cnstream::any_cast<CNObjsVec>(data->datas[CNObjsVecKey]);
    for (size_t idx = 0; idx < objs.size(); ++idx) {
      auto& obj = objs[idx];
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
  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
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

  CNObjsVec objs = cnstream::any_cast<CNObjsVec>(data->datas[CNObjsVecKey]);
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(5);
  CNInferBoundingBox bbox = {0.6, 0.6, -0.1, -0.1};
  obj->bbox = bbox;
  objs.push_back(obj);
  data->datas[cnstream::CNObjsVecKey] = objs;
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

  CNObjsVec objs = cnstream::any_cast<CNObjsVec>(data->datas[CNObjsVecKey]);
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(6);
  CNInferBoundingBox bbox = {0.6, 0.6, 0.6, 0.6};
  obj->bbox = bbox;
  objs.push_back(obj);
  data->datas[cnstream::CNObjsVecKey] = objs;
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
    CNObjsVec objs = cnstream::any_cast<CNObjsVec>(data->datas[CNObjsVecKey]);
    for (size_t idx = 0; idx < objs.size(); ++idx) {
      auto& obj = objs[idx];
      EXPECT_FALSE(obj->track_id.empty());
    }
  }
}

TEST(Tracker, ProcessFeatureMatchMLU1) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "FeatureMatch";
  param["model_path"] = GetExePath() + g_dsmodel_path;
  param["func_name"] = gfunc_name;
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 3;
  auto data = GenTestData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);
  // Illegal width and height
  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
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
  // Illegal fmt ???
  // frame->fmt = CN_PIXEL_FORMAT_RGB24; //
  // EXPECT_EQ(track->Process(data), -1); //
  // frame->fmt = CN_PIXEL_FORMAT_BGR24; //
  // EXPECT_EQ(track->Process(data), 0); //
}

TEST(Tracker, ProcessFeatureMatchMLU2) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "FeatureMatch";
  param["model_path"] = GetExePath() + g_dsmodel_path;
  param["func_name"] = gfunc_name;
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 3;
  auto data = GenTestData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);
  auto obj = std::make_shared<CNInferObject>();
  CNObjsVec objs = cnstream::any_cast<CNObjsVec>(data->datas[CNObjsVecKey]);
  obj->id = std::to_string(5);
  CNInferBoundingBox bbox = {0.6, 0.6, -0.1, -0.1};
  obj->bbox = bbox;
  objs.push_back(obj);
  data->datas[cnstream::CNObjsVecKey] = objs;
  EXPECT_ANY_THROW(track->Process(data));
}

TEST(Tracker, ProcessFeatureMatchMLU3) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "FeatureMatch";
  param["model_path"] = GetExePath() + g_dsmodel_path;
  param["func_name"] = gfunc_name;
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 0;
  auto data = GenTestData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);
  size_t zero = 0;
  CNObjsVec objs = cnstream::any_cast<CNObjsVec>(data->datas[CNObjsVecKey]);
  EXPECT_EQ(objs.size(), zero);
}

TEST(Tracker, ProcessFeatureMatchMLU4) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "FeatureMatch";
  param["model_path"] = GetExePath() + g_dsmodel_path;
  param["func_name"] = gfunc_name;
  ASSERT_TRUE(track->Open(param));

  int repeat_time = 10;
  int obj_num = 4;
  for (int n = 0; n < repeat_time; ++n) {
    auto data = GenTestData(n, obj_num);
    EXPECT_EQ(track->Process(data), 0);
    CNObjsVec objs = cnstream::any_cast<CNObjsVec>(data->datas[CNObjsVecKey]);
    for (size_t idx = 0; idx < objs.size(); ++idx) {
      auto& obj = objs[idx];
      EXPECT_FALSE(obj->track_id.empty());
    }
  }
}

#ifdef ENABLE_KCF
std::shared_ptr<CNFrameInfo> GenTestYUVMLUData(int iter, int obj_num) {
  const int width = 1920, height = 1080;
  size_t nbytes = width * height * sizeof(uint8_t) * 3;
  size_t boundary = 1 << 16;
  nbytes = (nbytes + boundary - 1) & ~(boundary - 1);  // align to 64kb

  // fake data
  void *frame_data = nullptr;
  void *planes[CN_MAX_PLANES] = {nullptr, nullptr};
  edk::MluMemoryOp mem_op;
  frame_data = mem_op.AllocMlu(nbytes, 1);
  planes[0] = frame_data;                                                                        // y plane
  planes[1] = reinterpret_cast<void *>(reinterpret_cast<int64_t>(frame_data) + width * height);  // uv plane

  // test nv12
  auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
  data->SetStreamIndex(g_channel_id);
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  frame->frame_id = iter;
  data->timestamp = 1000;
  frame->width = width;
  frame->height = height;
  frame->ptr_mlu[0] = planes[0];
  frame->ptr_mlu[1] = planes[1];
  frame->stride[0] = frame->stride[1] = width;
  frame->ctx.ddr_channel = g_channel_id;
  frame->ctx.dev_id = g_dev_id;
  frame->ctx.dev_type = DevContext::DevType::MLU;
  frame->fmt = CN_PIXEL_FORMAT_YUV420_NV21;
  frame->CopyToSyncMem();
  data->datas[CNDataFramePtrKey] = frame;

  CNObjsVec objs;
  for (int i = 0; i < obj_num; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1 + 0.01;
    CNInferBoundingBox bbox = {val, val, val, val};
    obj->bbox = bbox;
    objs.push_back(obj);
  }
  data->datas[CNObjsVecKey] = objs;
  return data;
}

TEST(Tracker, ProcessKCFMLU0) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "KCF";
  param["model_path"] = GetExePath() + g_kcfmodel_path;
  param["func_name"] = gfunc_name;
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 0;
  edk::MluMemoryOp mem_op;
  auto data = GenTestYUVMLUData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);

  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
  // free MLUmemory
  mem_op.FreeMlu(frame->ptr_mlu[0]);
}

TEST(Tracker, ProcessKCFMLU1) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "KCF";
  param["model_path"] = GetExePath() + g_kcfmodel_path;
  param["func_name"] = gfunc_name;
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 0;
  edk::MluMemoryOp mem_op;
  auto data = GenTestYUVMLUData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);
  // Illegal width and height
  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
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
  // Illegal fmt
  frame->fmt = CN_PIXEL_FORMAT_RGB24;
  EXPECT_NO_THROW(track->Process(data));
  frame->fmt = CN_PIXEL_FORMAT_YUV420_NV21;
  EXPECT_EQ(track->Process(data), 0);

  // free MLUmemory
  mem_op.FreeMlu(frame->ptr_mlu[0]);
}

TEST(Tracker, ProcessKCFMLU2) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "KCF";
  param["model_path"] = GetExePath() + g_kcfmodel_path;
  param["func_name"] = gfunc_name;
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 0;
  edk::MluMemoryOp mem_op;
  auto data = GenTestYUVMLUData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);

  CNObjsVec objs = cnstream::any_cast<CNObjsVec>(data->datas[CNObjsVecKey]);
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(5);
  CNInferBoundingBox bbox = {0.6, 0.6, -0.1, -0.1};
  obj->bbox = bbox;
  objs.push_back(obj);
  data->datas[CNObjsVecKey] = objs;
  EXPECT_EQ(track->Process(data), 0);

  // free MLUmemory
  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
  mem_op.FreeMlu(frame->ptr_mlu[0]);
}

TEST(Tracker, ProcessKCFMLU3) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "KCF";
  param["model_path"] = GetExePath() + g_kcfmodel_path;
  param["func_name"] = gfunc_name;
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 0;
  edk::MluMemoryOp mem_op;
  auto data = GenTestYUVMLUData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);

  CNObjsVec objs = cnstream::any_cast<CNObjsVec>(data->datas[CNObjsVecKey]);
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(6);
  CNInferBoundingBox bbox = {0.6, 0.6, 0.6, 0.6};
  obj->bbox = bbox;
  objs.push_back(obj);
  data->datas[CNObjsVecKey] = objs;
  EXPECT_EQ(track->Process(data), 0);

  // free MLUmemory
  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
  mem_op.FreeMlu(frame->ptr_mlu[0]);
}

TEST(Tracker, ProcessKCFMLU4) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "KCF";
  param["model_path"] = GetExePath() + g_kcfmodel_path;
  param["func_name"] = gfunc_name;
  ASSERT_TRUE(track->Open(param));
  int iter = 0;
  int obj_num = 0;
  edk::MluMemoryOp mem_op;
  auto data = GenTestYUVMLUData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);
  CNObjsVec objs = cnstream::any_cast<CNObjsVec>(data->datas[CNObjsVecKey]);
  size_t zero = 0;
  EXPECT_EQ(objs.size(), zero);

  // free MLUmemory
  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
  mem_op.FreeMlu(frame->ptr_mlu[0]);
}

TEST(Tracker, ProcessKCFMLU5) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  param["track_name"] = "KCF";
  param["model_path"] = GetExePath() + g_kcfmodel_path;
  param["func_name"] = gfunc_name;
  ASSERT_TRUE(track->Open(param));

  int obj_num = 3;
  int repeat_time = 3;
  edk::MluMemoryOp mem_op;
  for (int n = 0; n < repeat_time; ++n) {
    auto data = GenTestYUVMLUData(n, obj_num);
    EXPECT_EQ(track->Process(data), 0);
    CNObjsVec objs = cnstream::any_cast<CNObjsVec>(data->datas[CNObjsVecKey]);
    for (auto &obj : objs) {
      EXPECT_FALSE(obj->track_id.empty());
    }
    // free MLUmemory
    CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
    mem_op.FreeMlu(frame->ptr_mlu[0]);
  }
}
#endif

}  // namespace cnstream
