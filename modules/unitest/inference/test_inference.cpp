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
#include <sys/stat.h>
#include <opencv2/opencv.hpp>

#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "cninfer/mlu_context.h"
#include "cninfer/mlu_memory_op.h"
#include "cninfer/model_loader.h"

#include "inferencer.hpp"
#include "test_base.hpp"

namespace cnstream {

static const char *name = "test-infer";
static const char *g_image_path = "../../samples/data/images/3.jpg";
static const char *g_model_path =
    "../../samples/data/models/MLU100/Primary_Detector/resnet34ssd/resnet34_ssd.cambricon";
static const char *g_func_name = "subnet0";
static const char *g_postproc_name = "PostprocSsd";

static constexpr int g_dev_id = 0;
static constexpr int g_channel_id = 0;

TEST(Inferencer, Construct) {
  std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
  EXPECT_STREQ(infer->GetName().c_str(), name);
}

TEST(Inferencer, OpenClose) {
  std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
  ModuleParamSet param;
  EXPECT_FALSE(infer->Open(param));
  param["model_path"] = "test-infer";
  param["func_name"] = g_func_name;
  param["postproc_name"] = "test-postproc-name";
  param["device_id"] = std::to_string(g_dev_id);
  EXPECT_FALSE(infer->Open(param));
  param["model_path"] = GetExePath() + g_model_path;
  param["func_name"] = g_func_name;
  param["postproc_name"] = g_postproc_name;
  EXPECT_TRUE(infer->Open(param));
  infer->Close();
}

TEST(Inferencer, Process) {
  std::string model_path = GetExePath() + g_model_path;
  std::string image_path = GetExePath() + g_image_path;

  // test with MLU preproc (resize & convert)
  {
    std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
    ModuleParamSet param;
    param["model_path"] = model_path;
    param["func_name"] = g_func_name;
    param["postproc_name"] = g_postproc_name;
    param["device_id"] = std::to_string(g_dev_id);
    ASSERT_TRUE(infer->Open(param));

    const int width = 1280, height = 720;
    size_t nbytes = width * height * sizeof(uint8_t) * 3;
    size_t boundary = 1 << 16;
    nbytes = (nbytes + boundary - 1) & ~(boundary - 1);  // align to 64kb

    // fake data
    void *frame_data = nullptr;
    void *planes[CN_MAX_PLANES] = {nullptr, nullptr};
    libstream::MluMemoryOp mem_op;
    frame_data = mem_op.alloc_mem_on_mlu(nbytes, 1);
    planes[0] = frame_data;                                                                        // y plane
    planes[1] = reinterpret_cast<void *>(reinterpret_cast<int64_t>(frame_data) + width * height);  // uv plane

    uint32_t strides[CN_MAX_PLANES] = {(uint32_t)width, (uint32_t)width};

    // test nv12
    {
      auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
      data->channel_idx = g_channel_id;
      CNDataFrame &frame = data->frame;
      frame.frame_id = 1;
      frame.timestamp = 1000;
      frame.CopyFrameFromMLU(g_dev_id, g_channel_id, CN_PIXEL_FORMAT_YUV420_NV12, width, height, planes, strides);
      int ret = infer->Process(data);
      EXPECT_EQ(ret, 1);
    }

    // test nv21
    {
      auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
      data->channel_idx = g_channel_id;
      CNDataFrame &frame = data->frame;
      frame.frame_id = 1;
      frame.timestamp = 1000;
      frame.CopyFrameFromMLU(g_dev_id, g_channel_id, CN_PIXEL_FORMAT_YUV420_NV21, width, height, planes, strides);
      int ret = infer->Process(data);
      EXPECT_EQ(ret, 1);
    }

    mem_op.free_mem_on_mlu(frame_data);
  }

  // test with CPU preproc
  {
    std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
    ModuleParamSet param;
    param["model_path"] = model_path;
    param["func_name"] = g_func_name;
    param["preproc_name"] = "PreprocCpu";
    param["postproc_name"] = g_postproc_name;
    param["device_id"] = std::to_string(g_dev_id);
    ASSERT_TRUE(infer->Open(param));

    const int width = 1920, height = 1080;
    size_t nbytes = size_t(1) * width * height * sizeof(uint8_t) * 3;

    auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
    data->channel_idx = g_channel_id;
    CNDataFrame &frame = data->frame;
    frame.frame_id = 1;
    frame.timestamp = 1000;
    frame.width = width;
    frame.height = height;
    frame.fmt = CN_PIXEL_FORMAT_BGR24;
    frame.strides[0] = width;
    frame.ctx.dev_type = DevContext::DevType::CPU;
    frame.data[0].reset(new CNSyncedMemory(nbytes));

    int ret = infer->Process(data);
    EXPECT_EQ(ret, 1);
  }
}

}  // namespace cnstream
