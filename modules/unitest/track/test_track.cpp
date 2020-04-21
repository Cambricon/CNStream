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
#include <opencv2/opencv.hpp>

#include <memory>
#include <string>

#include "cnstream_module.hpp"
#include "easyinfer/mlu_memory_op.h"
#include "test_base.hpp"
#include "track.hpp"

namespace cnstream {

static constexpr const char *gname = "track";
static constexpr const char *gfunc_name = "subnet0";
static constexpr const char *g_dsmodel_path = "../../data/models/MLU100/Track/track.cambricon";
#ifdef CNS_MLU100
static constexpr const char *g_kcfmodel_path = "../../data/models/MLU100/KCF/yuv2gray.cambricon";
#elif CNS_MLU270
static constexpr const char *g_kcfmodel_path = "../../data/models/MLU270/KCF/yuv2gray.cambricon";
#endif
static constexpr const char *ds_track = "FeatureMatch";
static constexpr const char *kcf_track = "KCF";
static constexpr const char *img_path = "../../data/images/19.jpg";
static constexpr int g_dev_id = 0;
static constexpr int g_channel_id = 0;

TEST(Tracker, Construct) {
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  EXPECT_STREQ(track->GetName().c_str(), gname);
}

TEST(Tracker, CheckParamSet) {
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  EXPECT_FALSE(track->CheckParamSet(param));

  param["model_path"] = "fake_path";
  param["func_name"] = "fake_name";
  EXPECT_FALSE(track->CheckParamSet(param));

  param["model_path"] = GetExePath() + g_kcfmodel_path;
  param["func_name"] = gfunc_name;
  param["track_name"] = "fake_name";
  EXPECT_FALSE(track->CheckParamSet(param));

  param["track_name"] = kcf_track;
  EXPECT_TRUE(track->CheckParamSet(param));
}

TEST(Tracker, OpenClose) {
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  // wrong track name
  param["track_name"] = "foo";
  EXPECT_FALSE(track->Open(param));
  // Deep Sort On CPU
  param["track_name"] = ds_track;
  EXPECT_TRUE(track->Open(param));
  // Defaul param
  param.clear();
  EXPECT_TRUE(track->Open(param));
#ifdef CNS_MLU100
  // FeatureMatch On MLU
  param["track_name"] = ds_track;
  param["model_path"] = GetExePath() + g_dsmodel_path;
  param["func_name"] = gfunc_name;
  EXPECT_TRUE(track->Open(param));
#endif
  // KCF no model and func
  param.clear();
  param["track_name"] = kcf_track;
  EXPECT_FALSE(track->Open(param));
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
  data->channel_idx = 0;
  CNDataFrame &frame = data->frame;
  frame.frame_id = 1;
  frame.timestamp = 1000;
  frame.width = width;
  frame.height = height;
  frame.ptr_cpu[0] = img.data;
  frame.stride[0] = width;
  frame.ctx.dev_type = DevContext::DevType::CPU;
  frame.fmt = CN_PIXEL_FORMAT_BGR24;
  frame.CopyToSyncMem();
  for (int i = 0; i < obj_num; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1 + 0.01;
    CNInferBoundingBox bbox = {val, val, val, val};
    obj->bbox = bbox;
    data->objs.push_back(obj);
  }
  return data;
}
std::shared_ptr<CNFrameInfo> GenTestYUVData(int iter, int obj_num) {
  // prepare data
  int width = 1920;
  int height = 1080;
  cv::Mat img(height + height / 2, width, CV_8UC1);

  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  CNDataFrame &frame = data->frame;
  frame.frame_id = 1;
  frame.timestamp = 1000;
  frame.width = width;
  frame.height = height;
  frame.ptr_cpu[0] = img.data;
  frame.stride[0] = width;
  frame.ctx.dev_type = DevContext::DevType::CPU;
  frame.fmt = CN_PIXEL_FORMAT_YUV420_NV21;
  frame.CopyToSyncMem();
  for (int i = 0; i < obj_num; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1 + 0.01;
    CNInferBoundingBox bbox = {val, val, val, val};
    obj->bbox = bbox;
    data->objs.push_back(obj);
  }
  return data;
}

std::shared_ptr<CNFrameInfo> GenTestImageData() {
  // prepare data
  cv::Mat img;
  std::string image_path = GetExePath() + img_path;
  img = cv::imread(image_path, cv::IMREAD_COLOR);

  auto data = cnstream::CNFrameInfo::Create("1", false);
  CNDataFrame &frame = data->frame;
  frame.frame_id = 1;
  frame.timestamp = 1000;
  frame.width = img.cols;
  frame.height = img.rows;
  frame.ptr_cpu[0] = img.data;
  frame.stride[0] = img.cols;
  frame.ctx.dev_type = DevContext::DevType::CPU;
  frame.fmt = CN_PIXEL_FORMAT_BGR24;
  frame.CopyToSyncMem();
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(1);
  CNInferBoundingBox bbox = {0.2, 0.2, 0.6, 0.6};
  obj->bbox = bbox;
  data->objs.push_back(obj);
  return data;
}

#ifdef CNS_MLU100
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
    for (auto &obj : data->objs) {
      EXPECT_FALSE(obj->track_id.empty());
    }
  }
}
#endif

TEST(Tracker, ProcessCpuFeature) {
  // create track
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  ASSERT_TRUE(track->Open(param));

  int repeat_time = 10;

  for (int n = 0; n < repeat_time; ++n) {
    auto data = GenTestImageData();
    EXPECT_EQ(track->Process(data), 0);
    for (auto &obj : data->objs) {
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
  data->frame.width = -1;
  EXPECT_NO_THROW(track->Process(data));
  data->frame.width = 1920;
  EXPECT_EQ(track->Process(data), 0);

  data->frame.height = -1;
  EXPECT_NO_THROW(track->Process(data));
  data->frame.height = 1080;
  EXPECT_EQ(track->Process(data), 0);

  data->frame.width = 5096;
  data->frame.height = 3160;
  EXPECT_NO_THROW(track->Process(data));
  data->frame.width = 1920;
  data->frame.height = 1080;
  EXPECT_EQ(track->Process(data), 0);
  // Illegal fmt ???
  /* data->frame.fmt = CN_PIXEL_FORMAT_RGB24; */
  /* EXPECT_ANY_THROW(track->Process(data)); */
  data->frame.fmt = CN_PIXEL_FORMAT_BGR24;
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
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(5);
  CNInferBoundingBox bbox = {0.6, 0.6, -0.1, -0.1};
  obj->bbox = bbox;
  data->objs.push_back(obj);
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
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(6);
  CNInferBoundingBox bbox = {0.6, 0.6, 0.6, 0.6};
  obj->bbox = bbox;
  data->objs.push_back(obj);
  EXPECT_EQ(track->Process(data), 0);
}

/*******************************************
 * this case can not pass. this problem maybe
 * the same with Inferencer, cased by use thread local variables,
 * can not use open>>>close>>>open or open>>>open in the same thread.
 *******************************************/
// TEST(Tracker, ProcessFeatureMatchCPU4) {
//   // create track
//   std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
//   ModuleParamSet param;
//   param["track_name"] = "FeatureMatch";
//   ASSERT_TRUE(track->Open(param));
//   int iter = 0;
//   int obj_num = 3;
//   auto data = GenTestData(iter, obj_num);
//   EXPECT_EQ(track->Process(data), 0);
//   size_t zero = 0;
//   EXPECT_EQ(data->objs.size(), zero);
// }
TEST(Tracker, ProcessFeatureMatchCPU5) {
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
    for (auto &obj : data->objs) {
      EXPECT_FALSE(obj->track_id.empty());
    }
  }
}
#ifdef CNS_MLU100
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
  data->frame.width = -1;
  EXPECT_ANY_THROW(track->Process(data));
  data->frame.width = 1920;
  EXPECT_EQ(track->Process(data), 0);

  data->frame.height = -1;
  EXPECT_ANY_THROW(track->Process(data));
  data->frame.height = 1080;
  EXPECT_EQ(track->Process(data), 0);

  data->frame.width = 5096;
  data->frame.height = 3160;
  EXPECT_ANY_THROW(track->Process(data));
  data->frame.width = 1920;
  data->frame.height = 1080;
  EXPECT_EQ(track->Process(data), 0);
  // Illegal fmt ???
  /* data->frame.fmt = CN_PIXEL_FORMAT_RGB24; */
  /* EXPECT_ANY_THROW(track->Process(data)); */
  /* data->frame.fmt = CN_PIXEL_FORMAT_BGR24; */
  /* EXPECT_EQ(track->Process(data), 0); */
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
  obj->id = std::to_string(5);
  CNInferBoundingBox bbox = {0.6, 0.6, -0.1, -0.1};
  obj->bbox = bbox;
  data->objs.push_back(obj);
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
  int obj_num = 3;
  auto data = GenTestData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(6);
  CNInferBoundingBox bbox = {0.6, 0.6, 0.6, 0.6};
  obj->bbox = bbox;
  data->objs.push_back(obj);
  EXPECT_ANY_THROW(track->Process(data));
}

TEST(Tracker, ProcessFeatureMatchMLU4) {
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
  EXPECT_EQ(data->objs.size(), zero);
}
TEST(Tracker, ProcessFeatureMatchMLU5) {
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
    for (auto &obj : data->objs) {
      EXPECT_FALSE(obj->track_id.empty());
    }
  }
}
#endif

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
  data->channel_idx = g_channel_id;
  CNDataFrame &frame = data->frame;
  frame.frame_id = iter;
  frame.timestamp = 1000;
  frame.width = width;
  frame.height = height;
  frame.ptr_mlu[0] = planes[0];
  frame.ptr_mlu[1] = planes[1];
  frame.stride[0] = frame.stride[1] = width;
  frame.ctx.ddr_channel = g_channel_id;
  frame.ctx.dev_id = g_dev_id;
  frame.ctx.dev_type = DevContext::DevType::MLU;
  frame.fmt = CN_PIXEL_FORMAT_YUV420_NV21;
  frame.CopyToSyncMem();
  for (int i = 0; i < obj_num; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1 + 0.01;
    CNInferBoundingBox bbox = {val, val, val, val};
    obj->bbox = bbox;
    data->objs.push_back(obj);
  }
  return data;
}

// Test KCF failed because of thread local context won't destruct

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
  auto data = GenTestYUVMLUData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);
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
  auto data = GenTestYUVMLUData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);
  // Illegal width and height
  data->frame.width = -1;
  EXPECT_NO_THROW(track->Process(data));
  data->frame.width = 1920;
  EXPECT_EQ(track->Process(data), 0);

  data->frame.height = -1;
  EXPECT_NO_THROW(track->Process(data));
  data->frame.height = 1080;
  EXPECT_EQ(track->Process(data), 0);

  data->frame.width = 5096;
  data->frame.height = 3160;
  EXPECT_NO_THROW(track->Process(data));
  data->frame.width = 1920;
  data->frame.height = 1080;
  EXPECT_EQ(track->Process(data), 0);
  // Illegal fmt
  data->frame.fmt = CN_PIXEL_FORMAT_RGB24;
  EXPECT_NO_THROW(track->Process(data));
  data->frame.fmt = CN_PIXEL_FORMAT_YUV420_NV21;
  EXPECT_EQ(track->Process(data), 0);
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
  auto data = GenTestYUVMLUData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(5);
  CNInferBoundingBox bbox = {0.6, 0.6, -0.1, -0.1};
  obj->bbox = bbox;
  data->objs.push_back(obj);
  EXPECT_EQ(track->Process(data), 0);
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
  auto data = GenTestYUVMLUData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(6);
  CNInferBoundingBox bbox = {0.6, 0.6, 0.6, 0.6};
  obj->bbox = bbox;
  data->objs.push_back(obj);
  EXPECT_EQ(track->Process(data), 0);
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
  auto data = GenTestYUVMLUData(iter, obj_num);
  EXPECT_EQ(track->Process(data), 0);
  size_t zero = 0;
  EXPECT_EQ(data->objs.size(), zero);
}

#ifdef CNS_MLU100
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
  for (int n = 0; n < repeat_time; ++n) {
    auto data = GenTestYUVMLUData(n, obj_num);
    EXPECT_EQ(track->Process(data), 0);
    for (auto &obj : data->objs) {
      EXPECT_FALSE(obj->track_id.empty());
    }
  }
}
#endif

}  // namespace cnstream
