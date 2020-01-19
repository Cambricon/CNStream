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

#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#ifdef HAVE_OPENCV
#include "opencv2/opencv.hpp"
#endif

#include "cnstream_frame.hpp"

namespace cnstream {

void InitFrame(CNDataFrame* frame, int image_type) {
  frame->ctx.dev_type = DevContext::CPU;
  frame->height = 1080;
  frame->width = 1920;
  frame->stride[0] = 1920;
  if (image_type == 0) {  // RGB or BGR
    frame->ptr_cpu[0] = malloc(sizeof(uint32_t) * frame->height * frame->stride[0] * 3);
  } else if (image_type == 1) {  // YUV
    frame->stride[1] = 1920;
    frame->ptr_cpu[0] = malloc(sizeof(uint32_t) * frame->height * frame->stride[0]);
    frame->ptr_cpu[1] = malloc(sizeof(uint32_t) * frame->height * frame->stride[1] * 0.5);
  }
}

void RunConvertImageTest(CNDataFrame* frame, int image_type) {
  frame->CopyToSyncMem();
  EXPECT_NE(frame->ImageBGR(), nullptr);
  free(frame->ptr_cpu[0]);
  if (image_type == 1) {  // YUV
    free(frame->ptr_cpu[1]);
  }
}
#ifdef HAVE_OPENCV
TEST(CoreFrame, ConvertBGRImageToBGR) {
  CNDataFrame frame;
  InitFrame(&frame, 0);
  frame.fmt = CN_PIXEL_FORMAT_BGR24;

  RunConvertImageTest(&frame, 0);

  EXPECT_NE(frame.ImageBGR(), nullptr);
}

TEST(CoreFrame, ConvertRGBImageToBGR) {
  CNDataFrame frame;
  InitFrame(&frame, 0);
  frame.fmt = CN_PIXEL_FORMAT_RGB24;

  RunConvertImageTest(&frame, 0);
}

TEST(CoreFrame, ConvertYUV12ImageToBGR) {
  CNDataFrame frame;
  InitFrame(&frame, 1);
  frame.fmt = CN_PIXEL_FORMAT_YUV420_NV12;

  RunConvertImageTest(&frame, 1);
}

TEST(CoreFrame, ConvertYUV21ImageToBGR) {
  CNDataFrame frame;
  InitFrame(&frame, 1);
  frame.fmt = CN_PIXEL_FORMAT_YUV420_NV21;

  RunConvertImageTest(&frame, 1);
}

TEST(CoreFrame, ConvertImageToBGRFailed) {
  CNDataFrame frame;
  InitFrame(&frame, 1);
  frame.fmt = CN_PIXEL_FORMAT_YUV420_NV21;

  frame.CopyToSyncMem();
  frame.fmt = CN_INVALID;
  EXPECT_EQ(frame.ImageBGR(), nullptr);
  free(frame.ptr_cpu[0]);
  free(frame.ptr_cpu[1]);
}
#endif

TEST(CoreFrameDeathTest, CopyToSyncMemFailed) {
  CNDataFrame frame;
  InitFrame(&frame, 0);
  frame.fmt = CN_PIXEL_FORMAT_BGR24;

  frame.CopyToSyncMem();

  EXPECT_DEATH(frame.CopyToSyncMem(), "");
  frame.ctx.dev_type = DevContext::INVALID;
  EXPECT_DEATH(frame.CopyToSyncMem(), "");

  free(frame.ptr_cpu[0]);
}

TEST(CoreFrame, InferObjAddAttribute) {
  CNInferObject infer_obj;
  std::string key = "test_key";
  CNInferAttr value;
  value.id = 0;
  value.value = 0;
  value.score = 0.9;
  // add attribute successfully
  EXPECT_TRUE(infer_obj.AddAttribute(key, value));
  // attribute exist
  EXPECT_FALSE(infer_obj.AddAttribute(key, value));
}

TEST(CoreFrame, InferObjGetAttribute) {
  CNInferObject infer_obj;
  CNInferAttr infer_attr;

  // get attribute failed
  infer_attr = infer_obj.GetAttribute("wrong_key");
  EXPECT_EQ(infer_attr.id, -1);
  EXPECT_EQ(infer_attr.value, -1);
  EXPECT_EQ(infer_attr.score, 0.0);

  std::string key = "test_key";
  CNInferAttr value;
  value.id = 0;
  value.value = 0;
  value.score = 0.9;

  // add attribute successfully
  EXPECT_TRUE(infer_obj.AddAttribute(key, value));
  // get attribute successfully
  infer_attr = infer_obj.GetAttribute(key);
  EXPECT_EQ(infer_attr.id, value.id);
  EXPECT_EQ(infer_attr.value, value.value);
  EXPECT_EQ(infer_attr.score, value.score);
}

TEST(CoreFrame, InferObjAddExtraAttribute) {
  CNInferObject infer_obj;
  std::string key = "test_key";
  std::string value = "test_value";
  // add extra attribute successfully
  EXPECT_TRUE(infer_obj.AddExtraAttribute(key, value));
  // attribute exist
  EXPECT_FALSE(infer_obj.AddExtraAttribute(key, value));
}

TEST(CoreFrame, InferObjGetExtraAttribute) {
  CNInferObject infer_obj;

  // get extra attribute failed
  EXPECT_EQ(infer_obj.GetExtraAttribute("wrong_key"), "");

  std::string key = "test_key";
  std::string value = "test_value";

  // add extra attribute successfully
  EXPECT_TRUE(infer_obj.AddExtraAttribute(key, value));
  // get extra attribute successfully
  EXPECT_EQ(infer_obj.GetExtraAttribute(key), value);
}

TEST(CoreFrame, InferObjAddAndGetfeature) {
  CNInferObject infer_obj;
  CNInferFeature infer_feature;
  infer_feature.push_back(0.1);
  infer_feature.push_back(0.2);

  // add feature successfully
  EXPECT_NO_THROW(infer_obj.AddFeature(infer_feature));
  // get features
  std::vector<CNInferFeature> features = infer_obj.GetFeatures();
  EXPECT_EQ(features.size(), (uint32_t)1);
  EXPECT_EQ(features[0].size(), (uint32_t)2);
  EXPECT_EQ(features[0], infer_feature);

  infer_feature.clear();
  infer_feature.push_back(0.3);
  infer_feature.push_back(0.4);
  infer_feature.push_back(0.5);

  // add feature successfully
  EXPECT_NO_THROW(infer_obj.AddFeature(infer_feature));
  // get features
  features.clear();
  features = infer_obj.GetFeatures();
  EXPECT_EQ(features.size(), (uint32_t)2);
  EXPECT_EQ(features[0].size(), (uint32_t)2);
  EXPECT_EQ(features[1].size(), (uint32_t)3);
  EXPECT_EQ(features[1], infer_feature);
}

TEST(CoreFrame, SetAndGetParallelism) {
  int paral = 32;
  SetParallelism(paral);
  EXPECT_EQ(GetParallelism(), paral);
  SetParallelism(0);
  EXPECT_EQ(GetParallelism(), 0);
}

TEST(CoreFrame, CreateFrameInfo) {
  // create frame success
  EXPECT_NE(CNFrameInfo::Create("0"), nullptr);
  // create eos frame success
  EXPECT_NE(CNFrameInfo::Create("0", true), nullptr);
}

TEST(CoreFrame, CreateFrameInfoMultiParal) {
  uint32_t seed = (uint32_t)time(0);
  int paral = rand_r(&seed) % 64 + 1;
  {
    SetParallelism(paral);
    EXPECT_EQ(GetParallelism(), paral);
    std::vector<std::shared_ptr<CNFrameInfo>> frame_info_ptrs;
    for (int i = 0; i < paral; i++) {
      // create frame
      frame_info_ptrs.push_back(CNFrameInfo::Create("0"));
      EXPECT_NE(frame_info_ptrs[i], nullptr);
      frame_info_ptrs[i]->frame.ctx.dev_type = DevContext::CPU;
    }

    // exceed parallelism
    EXPECT_EQ(CNFrameInfo::Create("0"), nullptr);

    // create eos frame
    EXPECT_NE(CNFrameInfo::Create("0", true), nullptr);
  }
  SetParallelism(0);
}

}  // namespace cnstream

