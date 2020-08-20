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
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#endif

#include "cnrt.h"
#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"

namespace cnstream {

static const int width = 1280;
static const int height = 720;
static const int g_dev_id = 0;

void InitFrame(CNDataFrame* frame, int image_type) {
  frame->ctx.dev_type = DevContext::CPU;
  frame->height = 1080;
  frame->width = 1920;
  frame->stride[0] = 1920;
  if (image_type == 0) {  // RGB or BGR
    frame->ptr_cpu[0] = malloc(sizeof(uint32_t) * frame->height * frame->stride[0] * 3);
  } else if (image_type == 1) {  // YUV and height % 2 == 0
    frame->stride[1] = 1920;
    frame->ptr_cpu[0] = malloc(sizeof(uint32_t) * frame->height * frame->stride[0]);
    frame->ptr_cpu[1] = malloc(sizeof(uint32_t) * frame->height * frame->stride[1] * 0.5);
  } else if (image_type == 2) {  // YUV and height % 2 != 0
    frame->stride[1] = 1920;
    frame->height -= 1;
    frame->ptr_cpu[0] = malloc(sizeof(uint32_t) * (frame->height) * frame->stride[0]);
    frame->ptr_cpu[1] = malloc(sizeof(uint32_t) * (frame->height) * frame->stride[1] * 0.5);
  }
}

void RunConvertImageTest(CNDataFrame* frame, int image_type) {
  frame->CopyToSyncMem();
  EXPECT_NE(frame->ImageBGR(), nullptr);
  free(frame->ptr_cpu[0]);
  if (image_type == 1 || image_type == 2) {  // YUV
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

TEST(CoreFrame, ConvertYUV12ImageToBGR2) {  // height % 2 != 0
  CNDataFrame frame;
  InitFrame(&frame, 2);
  frame.fmt = CN_PIXEL_FORMAT_YUV420_NV12;

  RunConvertImageTest(&frame, 2);
}

TEST(CoreFrame, ConvertYUV21ImageToBGR) {
  CNDataFrame frame;
  InitFrame(&frame, 1);
  frame.fmt = CN_PIXEL_FORMAT_YUV420_NV21;

  RunConvertImageTest(&frame, 1);
}

TEST(CoreFrame, ConvertYUV21ImageToBGR2) {
  CNDataFrame frame;
  InitFrame(&frame, 2);
  frame.fmt = CN_PIXEL_FORMAT_YUV420_NV21;

  RunConvertImageTest(&frame, 2);
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

TEST(CoreFrameDeathTest, CopyToSyncMemOnDevice) {
  CNS_CNRT_CHECK(cnrtInit(0));
  unsigned int dev_num = 0;
  CNS_CNRT_CHECK(cnrtGetDeviceCount(&dev_num));

  size_t nbytes = width * height * 3;
  size_t boundary = 1 << 16;
  nbytes = (nbytes + boundary - 1) & ~(boundary - 1);
  void* frame_data = nullptr;
  CALL_CNRT_BY_CONTEXT(cnrtMalloc(&frame_data, nbytes), g_dev_id, 0);
  // fake frame data
  std::shared_ptr<CNDataFrame> frame = std::make_shared<CNDataFrame>();
  if (frame == nullptr) {
    std::cout << "frame create error\n";
    return;
  }
  frame->frame_id = 0;
  frame->width = width;
  frame->height = height;
  frame->mlu_data = frame_data;
  frame->stride[0] = width;
  frame->stride[1] = width;
  frame->ctx.ddr_channel = 0;
  frame->ctx.dev_id = g_dev_id;
  frame->ctx.dev_type = DevContext::MLU;
  frame->fmt = CN_PIXEL_FORMAT_YUV420_NV12;

  EXPECT_DEATH(frame->CopyToSyncMemOnDevice(g_dev_id), "");
  // check device num, if num > 1, do sync
  // TODO(gaoyujia) : update driver and then uncomment the line below
  // if (dev_num > 1) frame->CopyToSyncMemOnDevice(1);

  EXPECT_DEATH(frame->CopyToSyncMemOnDevice(dev_num + 1), "");
  frame->ctx.dev_type = DevContext::INVALID;
  EXPECT_DEATH(frame->CopyToSyncMemOnDevice(1), "");
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

TEST(CoreFrame, SetAndGetFlowDepth) {
  int flow_depth = 32;
  SetFlowDepth(flow_depth);
  EXPECT_EQ(GetFlowDepth(), flow_depth);
  SetFlowDepth(0);
  EXPECT_EQ(GetFlowDepth(), 0);
}

TEST(CoreFrame, CreateFrameInfo) {
  // create frame success
  EXPECT_NE(CNFrameInfo::Create("0"), nullptr);
  // create eos frame success
  EXPECT_NE(CNFrameInfo::Create("0", true), nullptr);
}

TEST(CoreFrame, CreateFrameInfoMultiFlow_Depth) {
  uint32_t seed = (uint32_t)time(0);
  int flow_depth = rand_r(&seed) % 64 + 1;
  {
    SetFlowDepth(flow_depth);
    EXPECT_EQ(GetFlowDepth(), flow_depth);
    std::vector<std::shared_ptr<CNFrameInfo>> frame_info_ptrs;
    for (int i = 0; i < flow_depth; i++) {
      // create frame
      frame_info_ptrs.push_back(CNFrameInfo::Create("0"));
      EXPECT_NE(frame_info_ptrs[i], nullptr);
    }

    // exceed parallelism
    EXPECT_EQ(CNFrameInfo::Create("0"), nullptr);

    // create eos frame
    EXPECT_NE(CNFrameInfo::Create("0", true), nullptr);
  }
  SetFlowDepth(0);
}

}  // namespace cnstream
