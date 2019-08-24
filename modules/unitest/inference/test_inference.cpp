/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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

namespace cnstream {

static const char *name = "test-infer";
static const char *g_image_path = "../../samples/data/images/3.jpg";
static const char *g_model_path =
    "../../samples/data/models/MLU100/Primary_Detector/resnet34ssd/resnet34_ssd.cambricon";
static const char *g_func_name = "subnet0";
static const char *g_postproc_name = "PostprocSsd";

static constexpr int g_dev_id = 0;
static constexpr int g_channel_id = 0;

static constexpr int PATH_MAX_LENGTH = 1 << 10;

std::string GetExePath() {
  char path[PATH_MAX_LENGTH];
  int cnt = readlink("/proc/self/exe", path, PATH_MAX_LENGTH);
  if (cnt < 0 || cnt >= PATH_MAX_LENGTH) {
    return "";
  }
  for (int i = cnt; i >= 0; --i) {
    if ('/' == path[i]) {
      path[i + 1] = '\0';
      break;
    }
  }
  return std::string(path);
}

TEST(Inferencer, TestConstruct) {
  std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
  EXPECT_STREQ(infer->GetName().c_str(), name);
}

TEST(Inferencer, TestOpenClose) {
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

TEST(Inferencer, TestProcess) {
  std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
  ModuleParamSet param;
  std::string model_path = GetExePath() + g_model_path;
  std::string image_path = GetExePath() + g_image_path;
  param["model_path"] = model_path;
  param["func_name"] = g_func_name;
  param["postproc_name"] = g_postproc_name;
  param["device_id"] = std::to_string(g_dev_id);
  ASSERT_TRUE(infer->Open(param));

  // get model info to set input images
  auto model = std::make_shared<libstream::ModelLoader>(model_path.c_str(), g_func_name);
  ASSERT_NE(model.get(), nullptr);
  model->InitLayout();
  ASSERT_EQ(model->input_num(), (uint32_t)1);
  libstream::CnShape in_shape = model->input_shapes()[0];

  libstream::MluMemoryOp mem_op;
  mem_op.set_loader(model);

  libstream::MluContext mlu_env;
  mlu_env.set_dev_id(g_dev_id);
  mlu_env.set_channel_id(g_channel_id);
  mlu_env.ConfigureForThisThread();

  // test in place
  {
    cv::Mat image = cv::imread(image_path, 1);
    void *cpu_input[1];
    cpu_input[0] = image.data;
    void **mlu_input = mem_op.alloc_mem_on_mlu_for_input(1);
    mem_op.memcpy_input_h2d(cpu_input, mlu_input, 1);

    int width = in_shape.w(), height = in_shape.h();
    uint32_t strides[] = {in_shape.stride()};
    auto data = std::make_shared<CNFrameInfo>();
    data->channel_idx = g_channel_id;
    CNDataFrame &frame = data->frame;
    frame.frame_id = 1;
    frame.timestamp = 1000;
    frame.CopyFrameFromMLU(g_dev_id, g_channel_id, CN_PIXEL_FORMAT_BGR24, width, height, mlu_input, strides);
    int ret = infer->Process(data);
    EXPECT_EQ(ret, 0);

    mem_op.free_mem_array_on_mlu(mlu_input, 1);
    // TODO: another check infer result
  }

  // test with resize & convert
  {
    int width = 1280, height = 720;
    cv::Mat image = cv::imread(image_path, 1);
    cv::resize(image, image, cv::Size(1280, 720));

    size_t nbytes = width * height * sizeof(uint8_t) * 3;
    size_t boundary = 1 << 16;
    size_t round_size = (nbytes + boundary - 1) & ~(boundary - 1);

    void *mlu_input[1];
    mlu_input[0] = mem_op.alloc_mem_on_mlu(round_size, 1);
    mem_op.memcpy_h2d(image.data, mlu_input[0], nbytes, 1);

    uint32_t strides[] = {(uint32_t)width};

    auto data = std::make_shared<CNFrameInfo>();
    data->channel_idx = g_channel_id;
    CNDataFrame &frame = data->frame;
    frame.frame_id = 1;
    frame.timestamp = 1000;
    frame.CopyFrameFromMLU(g_dev_id, g_channel_id, CN_PIXEL_FORMAT_BGR24, width, height, mlu_input, strides);
    int ret = infer->Process(data);
    EXPECT_EQ(ret, 0);

    mem_op.free_mem_on_mlu(mlu_input[0]);
  }
}

}  // namespace cnstream
