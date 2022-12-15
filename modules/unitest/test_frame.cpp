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
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "cnrt.h"
#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"

namespace cnstream {

static const int width = 1280;
static const int height = 720;
static const int g_dev_id = 0;

void InitFrame(CNDataFrame* frame, CnedkBufSurfaceColorFormat fmt) {
  CnedkBufSurfaceCreateParams create_params;
  create_params.batch_size = 1;
  memset(&create_params, 0, sizeof(create_params));
  create_params.device_id = g_dev_id;
  create_params.batch_size = 1;
  create_params.width = 1920;
  create_params.height = 1080;
  create_params.color_format = fmt;
  create_params.mem_type = CNEDK_BUF_MEM_DEVICE;

  CnedkBufSurface* surf;

  CnedkBufSurfaceCreate(&surf, &create_params);

  frame->buf_surf = std::make_shared<cnedk::BufSurfaceWrapper>(surf, false);
}

void RunConvertImageTest(CNDataFrame* frame) {
  EXPECT_FALSE(frame->ImageBGR().empty());
}

TEST(CoreFrame, ConvertYUV12ImageToBGR) {
  CNDataFrame frame;
  InitFrame(&frame, CNEDK_BUF_COLOR_FORMAT_NV12);
  RunConvertImageTest(&frame);
}

TEST(CoreFrame, ConvertYUV12ImageToBGR2) {  // height % 2 != 0
  CNDataFrame frame;
  InitFrame(&frame, CNEDK_BUF_COLOR_FORMAT_NV21);
  RunConvertImageTest(&frame);
}


TEST(CoreFrameDeathTest, ConvertImageToBGRFailed) {
  CNDataFrame frame;
  InitFrame(&frame, CNEDK_BUF_COLOR_FORMAT_BGR);
  EXPECT_DEATH(frame.ImageBGR(), ".*Unsupported pixel format.*");
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

  CNInferFeature infer_feature1{1, 2, 3, 4, 5};

  CNInferFeature infer_feature2{1, 2, 3, 4, 5, 6, 7};

  // add feature successfully
  EXPECT_NO_THROW(infer_obj.AddFeature("feature1", infer_feature1));
  EXPECT_NO_THROW(infer_obj.AddFeature("feature2", infer_feature2));

  // get features
  CNInferFeatures features = infer_obj.GetFeatures();
  EXPECT_EQ(features.size(), (uint32_t)2);
  EXPECT_EQ(infer_obj.GetFeature("feature1"), infer_feature1);
  EXPECT_EQ(infer_obj.GetFeature("feature2"), infer_feature2);
}

TEST(CoreFrame, CreateFrameInfo) {
  // create frame success
  EXPECT_NE(CNFrameInfo::Create("0"), nullptr);
  // create eos frame success
  EXPECT_NE(CNFrameInfo::Create("0", true), nullptr);
}

}  // namespace cnstream
