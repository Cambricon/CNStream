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
#include "test_base.hpp"
#include "track.hpp"

namespace cnstream {

static constexpr const char *gname = "track";
static constexpr const char *gfunc_name = "subnet0";
#ifdef CNS_MLU100
static constexpr const char *g_dsmodel_path = "../../data/models/MLU100/Track/track.cambricon";
#elif CNS_MLU270
static constexpr const char *g_dsmodel_path =
    "../../data/models/MLU270/Classification/resnet50/resnet50_offline.cambricon";
#endif
static constexpr const char *ds_track = "FeatureMatch";
static constexpr const char *kcf_track = "KCF";
// static constexpr const char *g_kcfmodel_path = "../../data/models/MLU100/";

TEST(Tracker, Construct) {
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  EXPECT_STREQ(track->GetName().c_str(), gname);
}

TEST(Tracker, OpenClose) {
  std::shared_ptr<Module> track = std::make_shared<Tracker>(gname);
  ModuleParamSet param;
  // wrong track name
  param["track_name"] = "foo";
  EXPECT_FALSE(track->Open(param));

  // KCF track without model name and func name
  param["track_name"] = kcf_track;
  EXPECT_FALSE(track->Open(param));

  // KCF track with model name and func name
  // param["track_name"] = "KCF";
  // param["model_path"] = GetExePath() + g_dsmodel_path;
  // param["func_name"] = gfunc_name;
  // EXPECT_TRUE(track->Open(param));

  // default track featurematch, no param, cpu process
  param.clear();
  EXPECT_TRUE(track->Open(param));

  // default track featurematch, mlu process
  param["model_path"] = GetExePath() + g_dsmodel_path;
  param["func_name"] = gfunc_name;
  EXPECT_TRUE(track->Open(param));

  // track featurematch, mlu process
  param["track_name"] = ds_track;
  param["model_path"] = GetExePath() + g_dsmodel_path;
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
  int obj_num = 4;

  for (int n = 0; n < repeat_time; ++n) {
    auto data = GenTestData(n, obj_num);
    EXPECT_EQ(track->Process(data), 0);
    for (auto &obj : data->objs) {
      EXPECT_FALSE(obj->track_id.empty());
    }
  }
}

}  // namespace cnstream
