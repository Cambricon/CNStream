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

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "cnstream_module.hpp"
#include "data_source.hpp"
#include "easyinfer/mlu_context.h"
#include "test_base.hpp"

namespace cnstream {

static constexpr const char *gname = "source";
static constexpr const char *gvideo_path = "../../modules/unitest/source/data/cars_short.mp4";
static constexpr const char *gimage_path = "../../samples/data/images/%d.jpg";

void ResetParam(ModuleParamSet &param) {  // NOLINT
  param["source_type"] = "raw";
  param["output_type"] = "mlu";
  param["device_id"] = "0";
  param["interval"] = "1";
  param["decoder_type"] = "mlu";
  param["output_width"] = "1920";
  param["output_height"] = "1080";
  param["reuse_cndex_buf"] = "true";
  param["chunk_size"] = "16384";
  param["width"] = "1920";
  param["height"] = "1080";
  param["interlaced"] = "1";
  param["input_buf_number"] = "100";
  param["output_buf_number"] = "100";
}

TEST(Source, Construct) {
  std::shared_ptr<Module> src = std::make_shared<DataSource>(gname);
  EXPECT_STREQ(src->GetName().c_str(), gname);
}

TEST(Source, OpenClose) {
  std::shared_ptr<Module> src = std::make_shared<DataSource>(gname);
  ModuleParamSet param;

  ResetParam(param);
  EXPECT_TRUE(src->Open(param));

  // invalid source type
  param["source_type"] = "foo";
  EXPECT_FALSE(src->Open(param));
  ResetParam(param);

  // invalid output type
  param["output_type"] = "bar";
  EXPECT_FALSE(src->Open(param));
  ResetParam(param);

  // mlu output with invalid device id
  param.erase("device_id");
  ResetParam(param);

  // negative interval
  param["interval"] = "-1";
  EXPECT_FALSE(src->Open(param));
  ResetParam(param);

  // invalid decode type
  param["decoder_type"] = "blabla";
  EXPECT_FALSE(src->Open(param));
  ResetParam(param);

  // mlu decoder with invalid device id
  param.erase("device_id");
  ResetParam(param);

  // reuse cndecoder buffer
  param["reuse_cndex_buf"] = "false";
  ResetParam(param);

  // raw decode without chunk params
  param.erase("chunk_size");
  EXPECT_FALSE(src->Open(param));
  ResetParam(param);
  param.erase("width");
  EXPECT_FALSE(src->Open(param));
  ResetParam(param);
  param.erase("height");
  EXPECT_FALSE(src->Open(param));
  ResetParam(param);
  param.erase("interlaced");
  EXPECT_FALSE(src->Open(param));
  param.clear();

  // proper params
  // ffmpeg
  param["source_type"] = "ffmpeg";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  EXPECT_TRUE(src->Open(param));
  param.clear();
  src->Close();

  param["source_type"] = "ffmpeg";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["reuse_cndec_buf"] = "true";
  param["device_id"] = "0";
  EXPECT_TRUE(src->Open(param));
  param.clear();
  src->Close();

  param["source_type"] = "ffmpeg";
  param["output_type"] = "cpu";
  param["decoder_type"] = "cpu";
  EXPECT_TRUE(src->Open(param));
  param.clear();
  src->Close();

  // raw
  param["source_type"] = "raw";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  param["chunk_size"] = "16384";
  param["width"] = "1920";
  param["height"] = "1080";
  param["interlaced"] = "0";
  EXPECT_TRUE(src->Open(param));
  param.clear();
  src->Close();

  param["source_type"] = "raw";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["reuse_cndec_buf"] = "true";
  param["device_id"] = "0";
  param["chunk_size"] = "16384";
  param["width"] = "1920";
  param["height"] = "1080";
  param["interlaced"] = "1";
  EXPECT_TRUE(src->Open(param));
  param.clear();
  src->Close();

  // raw only support mlu decoder
  param["source_type"] = "raw";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  param["chunk_size"] = "16384";
  param["width"] = "1920";
  param["height"] = "1080";
  param["interlaced"] = "0";
  EXPECT_TRUE(src->Open(param));
  param.clear();
  src->Close();

  // DataSource module should not invoke Process()
  std::shared_ptr<CNFrameInfo> data = nullptr;
  EXPECT_FALSE(src->Process(data));
}

TEST(Source, SendData) {
  auto src = std::make_shared<DataSource>(gname);
  std::shared_ptr<Pipeline> pipeline = std::make_shared<Pipeline>("pipeline");
  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  data->channel_idx = 0;
  EXPECT_FALSE(src->SendData(data));
  pipeline->AddModule(src);
  EXPECT_TRUE(src->SendData(data));
}

TEST(Source, AddVideoSource) {
  auto src = std::make_shared<DataSource>(gname);
  std::string stream_id1 = "1";
  std::string stream_id2 = "2";
  std::string video_path = GetExePath() + gvideo_path;
  ModuleParamSet param;
  param["source_type"] = "ffmpeg";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  ASSERT_TRUE(src->Open(param));

  // successfully add video source
  EXPECT_EQ(src->AddVideoSource(stream_id1, video_path, 24, true), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id2, video_path, 24, false), 0);

  // repeadly add video source, wrong!
  EXPECT_EQ(src->AddVideoSource(stream_id1, video_path, 24), -1);
  EXPECT_EQ(src->AddVideoSource(stream_id2, video_path, false), -1);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  src->Close();

  // filename.empty(), return -1
  for (uint32_t i = 0; i < GetMaxStreamNumber(); i++) {
    EXPECT_EQ(src->AddVideoSource(std::to_string(i), "", 24, true), -1);
  }
  // open source failed, return -1
  EXPECT_EQ(src->AddVideoSource(std::to_string(GetMaxStreamNumber()), "", 24), -1);
  src->Close();

  // filename valid, return 0
  for (uint32_t i = 0; i < GetMaxStreamNumber(); i++) {
    EXPECT_EQ(src->AddVideoSource(std::to_string(i), video_path, 24, true), 0);
  }
  // open source failed, return -1
  EXPECT_EQ(src->AddVideoSource(std::to_string(GetMaxStreamNumber()), video_path, 24), -1);
  src->Close();
}

TEST(Source, RemoveSource) {
  std::string video_path = GetExePath() + gvideo_path;
  auto src = std::make_shared<DataSource>(gname);
  std::string stream_id1 = "1";
  std::string stream_id2 = "2";
  std::string stream_id3 = "3";
  std::string stream_id4 = "4";
  ModuleParamSet param;
  param["source_type"] = "ffmpeg";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  ASSERT_TRUE(src->Open(param));

  // successfully add video source
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(src->AddVideoSource(std::to_string(i), video_path, false), 0);
  }
  // remove source
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(src->RemoveSource(std::to_string(i)), 0);
  }
  // source not exist, log warning
  EXPECT_EQ(src->RemoveSource(std::to_string(0)), 0);
  EXPECT_EQ(src->RemoveSource(std::to_string(4)), 0);

  // remove all sources
  src->Close();

  // source not exist, log warning
  EXPECT_EQ(src->RemoveSource(std::to_string(3)), 0);
  EXPECT_EQ(src->RemoveSource(std::to_string(9)), 0);
}

TEST(Source, FFMpegMLU) {
  auto src = std::make_shared<DataSource>(gname);
  std::string video_path = GetExePath() + gvideo_path;
  std::string image_path = GetExePath() + gimage_path;
  std::string stream_id1 = "1";
  std::string stream_id2 = "2";
  std::string stream_id3 = "3";
  std::string stream_id4 = "4";

  ModuleParamSet param;
  param["source_type"] = "ffmpeg";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  ASSERT_TRUE(src->Open(param));

  // add source
  EXPECT_EQ(src->AddVideoSource(stream_id1, video_path, 24), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id2, video_path, 24, true), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id3, video_path, 24, false), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id4, image_path, 24), 0);

  EXPECT_NE(src->AddVideoSource(stream_id3, video_path, 24), 0);
  EXPECT_NE(src->AddVideoSource(stream_id4, image_path, 24), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  EXPECT_EQ(src->RemoveSource(stream_id1), 0);
  EXPECT_EQ(src->RemoveSource(stream_id2), 0);

  EXPECT_EQ(src->AddVideoSource(stream_id1, video_path, 24), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id2, image_path, 24), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  src->Close();

  // reuse codec buffer
  param["reuse_cndec_buf"] = "true";
  ASSERT_TRUE(src->Open(param));
  EXPECT_EQ(src->AddVideoSource(stream_id1, video_path, 24), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  src->Close();
}

TEST(Source, FFMpegCPU) {
  auto src = std::make_shared<DataSource>(gname);
  std::string video_path = GetExePath() + gvideo_path;
  std::string stream_id1 = "1";
  std::string stream_id2 = "2";
  std::string stream_id3 = "3";

  ModuleParamSet param;
  param["source_type"] = "ffmpeg";
  param["output_type"] = "cpu";
  param["decoder_type"] = "cpu";
  ASSERT_TRUE(src->Open(param));

  // add source
  EXPECT_EQ(src->AddVideoSource(stream_id1, video_path, 23), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id2, video_path, 24, true), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id3, video_path, 25, false), 0);

  EXPECT_NE(src->AddVideoSource(stream_id3, video_path, 26), 0);
  EXPECT_NE(src->AddVideoSource(stream_id1, video_path, 27), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  EXPECT_EQ(src->RemoveSource(stream_id1), 0);
  EXPECT_EQ(src->RemoveSource(stream_id2), 0);

  param["output_type"] = "mlu";
  param["device_id"] = "0";
  ASSERT_TRUE(src->Open(param));
  EXPECT_EQ(src->AddVideoSource(stream_id1, video_path, 22), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id2, video_path, 21), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  src->Close();
}

TEST(Source, RawMLU) {
  std::string h264_path = GetExePath() + "../../modules/unitest/source/data/raw.h264";
  std::string h265_path = GetExePath() + "../../modules/unitest/source/data/raw.h265";
  auto src = std::make_shared<DataSource>(gname);
  std::string stream_id0 = "0";
  std::string stream_id1 = "1";
  std::string stream_id2 = "2";
  std::string stream_id3 = "3";

  ModuleParamSet param;
  param["source_type"] = "raw";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  // chunk size 50K
  param["chunk_size"] = "50000";
  param["width"] = "256";
  param["height"] = "256";
  param["interlaced"] = "false";
  ASSERT_TRUE(src->Open(param));

  // add source
  EXPECT_EQ(src->AddVideoSource(stream_id0, h264_path, 23, false), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id1, h264_path, 30, true), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id2, h265_path, 21, false), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id3, h265_path, 27, true), 0);

  EXPECT_NE(src->AddVideoSource(stream_id3, h264_path, 20, true), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  src->Close();

  // reuse codec buffer
  param["reuse_cndec_buf"] = "true";
  ASSERT_TRUE(src->Open(param));
  EXPECT_EQ(src->AddVideoSource(stream_id0, h264_path, 24, false), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id1, h264_path, 24, true), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  src->Close();
}

}  // namespace cnstream
