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

#include <iostream>
#include <memory>
#include <string>

#include "easyinfer/mlu_context.h"
#include "easyinfer/mlu_memory_op.h"

#include "rtsp_sink.hpp"
#include "test_base.hpp"

namespace cnstream {
static constexpr const char *gname = "rtsp";
static constexpr int g_dev_id = 0;
static constexpr int g_channel_id = 0;

extern void TestAllCase(std::shared_ptr<Module> ptr, ModuleParamSet params, int line);
extern bool PullRtspStreamOpencv();
extern bool PullRtspStreamFFmpeg();
extern std::string GetIp();
extern std::shared_ptr<CNFrameInfo> GenTestData(ColorFormat cmode, int width, int height);
extern void Process(std::shared_ptr<Module> ptr, ColorFormat cmode, int width, int height, int line);

TEST(RtspSink, Open) {
  std::shared_ptr<Module> ptr = std::make_shared<RtspSink>(gname);
  ModuleParamSet params;
  EXPECT_TRUE(ptr->Open(params));
  params["udp_port"] = "9999";
  params["http_port"] = "8000";
  params["frame_rate"] = "35";
  params["kbit_rate"] = "512";
  params["gop_size"] = "30";
  params["view_mode"] = "single";
  params["dst_width"] = "352";
  params["dst_height"] = "288";
  params["color_mode"] = "nv";
  params["preproc_type"] = "cpu";
  params["encoder_type"] = "mlu";
  params["device_id"] = "0";
  EXPECT_TRUE(ptr->CheckParamSet(params));
  ASSERT_NO_THROW(ptr->Close());

  EXPECT_TRUE(ptr->Open(params));
  params.clear();
  params["view_mode"] = "mosaic";
  params["view_rows"] = "3";
  params["view_cols"] = "2";
  EXPECT_TRUE(ptr->CheckParamSet(params));
  ASSERT_NO_THROW(ptr->Close());
  // EXPECT_TRUE(ptr->Open(params));
}

TEST(RtspSink, Process) {
  std::shared_ptr<Module> ptr = std::make_shared<RtspSink>(gname);
  ModuleParamSet params;

  params.clear();
  params["view_mode"] = "single";
  params["dst_width"] = "0";
  params["dst_height"] = "0";
  params["color_mode"] = "nv";
  TestAllCase(ptr, params, __LINE__);

  params["color_mode"] = "bgr";
  TestAllCase(ptr, params, __LINE__);

  params["view_mode"] = "mosaic";
  int col = 3;
  int row = 2;
  params["view_cols"] = std::to_string(col);
  params["view_rows"] = std::to_string(row);
  TestAllCase(ptr, params, __LINE__);

  EXPECT_TRUE(ptr->Open(params));
  for (int i = 0; i < col * row; i++) {
    auto data = cnstream::CNFrameInfo::Create(std::to_string(i));
    EXPECT_EQ(ptr->Process(data), -1);
    data = cnstream::CNFrameInfo::Create(std::to_string(i), true);
  }
  // PullRtspStreamOpencv();
  PullRtspStreamFFmpeg();
  auto data = cnstream::CNFrameInfo::Create(std::to_string(col * row + 1));
  EXPECT_EQ(ptr->Process(data), -1);
  data = cnstream::CNFrameInfo::Create(std::to_string(col * row + 1), true);
  ptr->Close();

  params["color_mode"] = "wrong";
  EXPECT_TRUE(ptr->Open(params));

  data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));

  // test nv12
  EXPECT_EQ(ptr->Process(data), -1);
  ptr->Close();
  cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);
}

TEST(RtspSink, CheckParam) {
  std::shared_ptr<Module> ptr = std::make_shared<RtspSink>(gname);
  ModuleParamSet params;
  params["wrong"] = "9999";
  EXPECT_TRUE(ptr->CheckParamSet(params));

  params.clear();
  params["udp_port"] = "str";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params["udp_port"] = "-1";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();
  params["http_port"] = "str";
  params["frame_rate"] = "-1";
  params["kbit_rate"] = "-1";
  params["gop_size"] = "-1";
  params["dst_width"] = "-1";
  params["dst_height"] = "-1";
  params["view_rows"] = "-1";
  params["view_cols"] = "-1";
  EXPECT_FALSE(ptr->CheckParamSet(params));

  params.clear();
  EXPECT_TRUE(ptr->CheckParamSet(params));

  params["encoder_type"] = "wrong";
  EXPECT_FALSE(ptr->CheckParamSet(params));

  params.clear();
  params["preproc_type"] = "wrong";
  EXPECT_FALSE(ptr->CheckParamSet(params));

  params.clear();
  params["view_mode"] = "wrong";
  EXPECT_FALSE(ptr->CheckParamSet(params));

  params.clear();
  params["color_mode"] = "wrong";
  EXPECT_FALSE(ptr->CheckParamSet(params));

  params.clear();
  params["view_mode"] = "mosaic";
  params["color_mode"] = "nv";
  EXPECT_TRUE(ptr->CheckParamSet(params));
}
}  // namespace cnstream
