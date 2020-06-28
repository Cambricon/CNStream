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

#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "encoder.hpp"
#include "test_base.hpp"

namespace cnstream {
static constexpr const char *gname = "encoder";

TEST(EncoderModule, OpenClose) {
  Encoder module(gname);
  ModuleParamSet params;

  params.clear();
  EXPECT_TRUE(module.Open(params));

  params["dump_dir"] = GetExePath();
  EXPECT_TRUE(module.Open(params));
  module.Close();
  // module.Process(nullptr);
}

TEST(EncoderModule, Process) {
  std::shared_ptr<Module> ptr = std::make_shared<Encoder>(gname);
  ModuleParamSet params;
  params["dump_dir"] = GetExePath();
  EXPECT_TRUE(ptr->Open(params));
  // prepare data
  int width = 1920;
  int height = 1080;
  cv::Mat img(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  data->SetStreamIndex(0);
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
  EXPECT_EQ(ptr->Process(data), 0);
  ptr->Close();

  params["dump_type"] = "image";
  EXPECT_TRUE(ptr->Open(params));
  EXPECT_EQ(ptr->Process(data), 0);
  ptr->Close();
}

TEST(EncoderModule, CheckParamSet) {
  Encoder module(gname);
  ModuleParamSet params;
  params["dump_dir"] = GetExePath();
  params["dump_type"] = "image";
  EXPECT_TRUE(module.CheckParamSet(params));

  params["fake_key"] = "fake_value";
  EXPECT_TRUE(module.CheckParamSet(params));
}

}  // namespace cnstream
