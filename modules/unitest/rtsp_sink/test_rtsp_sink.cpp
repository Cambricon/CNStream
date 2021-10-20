/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#include <ifaddrs.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
}
#endif

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#if (CV_MAJOR_VERSION >= 3)
#include <opencv2/imgcodecs/imgcodecs.hpp>
#endif

#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <future>

#include "gtest/gtest.h"
#include "cnstream_frame_va.hpp"
#include "easyinfer/mlu_memory_op.h"
#include "rtsp_sink.hpp"
#include "rtsp_server.hpp"
#include "test_base.hpp"

namespace cnstream {
static constexpr const char *gname = "rstp_sink";
static constexpr int g_device_id = 0;
static constexpr int g_dev_id = 0;
static constexpr int g_width = 1280;
static constexpr int g_height = 720;

extern void TestAllCase(ModuleParamSet params, int frame_rate, bool tiler, int line);
extern bool PullRtspStreamOpencv(int port = 9554);
extern bool PullRtspStreamFFmpeg(int port = 9554);
extern std::shared_ptr<CNFrameInfo> GenTestData(CNPixelFormat pix_fmt, int width, int height);
TEST(RtspModule, OpenClose) {
  RtspSink module(gname);
  ModuleParamSet params;
  EXPECT_FALSE(module.Open(params));

  params["port"] = "9554";
  params["rtsp_over_http"] = "false";
  params["frame_rate"] = "25";
  params["bit_rate"] = "4000000";
  params["gop_size"] = "10";
  params["view_cols"] = "1";
  params["view_rows"] = "1";
  params["resample"] = "false";
  EXPECT_TRUE(module.Open(params));

  params["device_id"] = "0";
  params["encoder_type"] = "mlu";
  params["input_frame"] = "cpu";
  EXPECT_TRUE(module.Open(params));

  params["dst_width"] = "1280";
  params["dst_height"] = "720";
  EXPECT_TRUE(module.Open(params));

  params["input_frame"] = "mlu";
  params["encoder_type"] = "mlu";
  params["device_id"] = "-1";
  EXPECT_FALSE(module.Open(params));

  params["encoder_type"] = "abc";
  EXPECT_FALSE(module.Open(params));
  module.Close();
}

TEST(RtspModule, Process) {
  // create rtsp_sink
  std::shared_ptr<RtspSink> ptr = std::make_shared<RtspSink>(gname);
  ModuleParamSet params;
  int frame_rate = 25;

  std::string device_id = "-1";
  params["port"] = "9554";
  params["encoder_type"] = "cpu";
  params["input_frame"] = "cpu";
  params["device_id"] = device_id;

  int col = 3;
  int row = 2;
  params["view_cols"] = std::to_string(col);
  params["view_rows"] = std::to_string(row);
  params["frame_rate"] = std::to_string(frame_rate);
  TestAllCase(params, frame_rate, true, __LINE__);

  EXPECT_TRUE(ptr->Open(params));
  for (int i = 0; i < col * row; i++) {
    auto data = cnstream::CNFrameInfo::Create(std::to_string(i));
    std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
    data->collection.Add(kCNDataFrameTag, frame);
    EXPECT_EQ(ptr->Process(data), -1);
    data = cnstream::CNFrameInfo::Create(std::to_string(i), true);
  }
  auto fut = std::async(std::launch::async, PullRtspStreamFFmpeg, 9445);
  auto data = cnstream::CNFrameInfo::Create(std::to_string(col * row + 1));
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  data->collection.Add(kCNDataFrameTag, frame);
  EXPECT_EQ(ptr->Process(data), -1);
  data = cnstream::CNFrameInfo::Create(std::to_string(col * row + 1), true);
  fut.get();
  ptr->Close();
}

}  // namespace cnstream
