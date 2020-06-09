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
#include <thread>
#include <vector>

#ifdef HAVE_OPENCV
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#else
#error OpenCV required
#endif
#include "easyinfer/mlu_context.h"
#include "easyinfer/mlu_memory_op.h"
#include "easyinfer/model_loader.h"

#include "inferencer.hpp"
#include "postproc.hpp"
#include "preproc.hpp"
#include "test_base.hpp"

namespace cnstream {

static const char *g_model_path = "../../data/models/MLU270/Classification/resnet50/resnet50_offline.cambricon";
static const char *g_func_name = "subnet0";
static const char *g_postproc_name = "PostprocClassification";

static constexpr int g_dev_id = 0;
static constexpr int g_channel_id = 0;


void GetResult(std::shared_ptr<Module> infer) {
  uint32_t i = 0;
  while (1) {
    auto data = infer->GetOutputFrame();
    if (data != nullptr) {
      if (!(data->frame.flags & CNFrameFlag::CN_FRAME_FLAG_EOS)) {
        EXPECT_EQ(data->frame.frame_id, i);
        i++;
        std::cout << "Got data, frame id = " << data->frame.frame_id << std::endl;
      } else {
        std::cout << "**********Got EOS *********" << std::endl;
        break;
      }
    }
  }
}

TEST(Inferencer, Demo) {
  std::string model_path = GetExePath() + g_model_path;

  std::shared_ptr<Module> infer = std::make_shared<Inferencer>("test_infer");
  std::thread th = std::thread(&GetResult, infer);
  ModuleParamSet param;
  param["model_path"] = model_path;
  param["func_name"] = g_func_name;
  param["postproc_name"] = g_postproc_name;
  param["device_id"] = std::to_string(g_dev_id);
  param["batching_timeout"] = "30";
  // Open
  ASSERT_TRUE(infer->Open(param));

  const int width = 1280, height = 720;
  size_t nbytes = width * height * sizeof(uint8_t) * 3 / 2;
  size_t boundary = 1 << 16;
  nbytes = (nbytes + boundary - 1) & ~(boundary - 1);  // align to 64kb

  // fake data vector
  std::vector<void*> frame_data_vec;
  edk::MluMemoryOp mem_op;

  // test nv12
  for (uint32_t i = 0; i < 32; i++) {
    // fake data
    void *frame_data = nullptr;
    frame_data = mem_op.AllocMlu(nbytes, 1);
    frame_data_vec.push_back(frame_data);
    void *planes[CN_MAX_PLANES] = {nullptr, nullptr};
    planes[0] = frame_data;                                                                        // y plane
    planes[1] = reinterpret_cast<void *>(reinterpret_cast<int64_t>(frame_data) + width * height);  // uv plane

    auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
    CNDataFrame &frame = data->frame;
    frame.frame_id = i;
    frame.timestamp = i;
    frame.width = width;
    frame.height = height;
    frame.ptr_mlu[0] = planes[0];
    frame.ptr_mlu[1] = planes[1];
    frame.stride[0] = frame.stride[1] = width;
    frame.ctx.ddr_channel = g_channel_id;
    frame.ctx.dev_id = g_dev_id;
    frame.ctx.dev_type = DevContext::DevType::MLU;
    frame.fmt = CN_PIXEL_FORMAT_YUV420_NV12;
    frame.CopyToSyncMem();
    int ret = infer->Process(data);
    EXPECT_EQ(ret, 1);
  }
  // eos frame
  auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);
  int ret = infer->Process(data);
  EXPECT_EQ(ret, 1);

  if (th.joinable()) {
    th.join();
  }

  ASSERT_NO_THROW(infer->Close());

  for (auto it : frame_data_vec) {
    mem_op.FreeMlu(it);
    it = nullptr;
  }
}

}  // namespace cnstream
