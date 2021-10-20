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

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "cnstream_module.hpp"
#include "data_handler_file.hpp"
#include "data_handler_mem.hpp"
#include "data_handler_rtsp.hpp"
#include "data_source.hpp"
#include "device/mlu_context.h"
#include "test_base.hpp"

namespace cnstream {

static constexpr const char *gname = "source";
static constexpr const char *gvideo_path = "../../data/videos/cars.mp4";
static constexpr const char *gimage_path = "../../data/images/%d.jpg";

void ResetParam(ModuleParamSet &param) {  // NOLINT
  param["output_type"] = "mlu";
  param["device_id"] = "0";
  param["interval"] = "1";
  param["decoder_type"] = "mlu";
  param["reuse_cndec_buf"] = "true";
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
  EXPECT_TRUE(src->CheckParamSet(param));
  EXPECT_TRUE(src->Open(param));

  // invalid output_type type
  param["output_type"] = "foo";
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

  // proper params
  // ffmpeg
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  EXPECT_TRUE(src->CheckParamSet(param));
  EXPECT_TRUE(src->Open(param));
  param.clear();
  src->Close();

  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["reuse_cndec_buf"] = "true";
  param["device_id"] = "0";
  EXPECT_TRUE(src->Open(param));
  param.clear();
  src->Close();

  param["output_type"] = "cpu";
  param["decoder_type"] = "cpu";
  EXPECT_TRUE(src->Open(param));
  param.clear();
  src->Close();

  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["reuse_cndec_buf"] = "true";
  param["device_id"] = "0";
  EXPECT_TRUE(src->Open(param));
  param.clear();
  src->Close();

  // DataSource module should not invoke Process()
  std::shared_ptr<CNFrameInfo> data = nullptr;
  EXPECT_FALSE(src->Process(data));
}

TEST(Source, AddSource) {
  auto src = std::make_shared<DataSource>(gname);
  std::string stream_id1 = "1";
  std::string stream_id2 = "2";
  std::string stream_id3 = "3";
  std::string stream_id4 = "4";
  std::string video_path = GetExePath() + gvideo_path;
  std::string rtsp_path = "rtsp://test";

  ModuleParamSet param;
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  ASSERT_TRUE(src->Open(param));

  auto handler1 = FileHandler::Create(src.get(), stream_id1, video_path, 24, true);
  auto handler2 = FileHandler::Create(src.get(), stream_id2, video_path, 24, false);
  auto handler3 = FileHandler::Create(src.get(), stream_id3, video_path, 24, false);
  // auto handler4 = RtspHandler::Create(src.get(), stream_id4, rtsp_path);

  // successfully add video source
  EXPECT_EQ(src->AddSource(handler1), 0);
  EXPECT_EQ(src->AddSource(handler2), 0);
  EXPECT_EQ(src->AddSource(handler3), 0);
  // EXPECT_EQ(src->AddSource(handler4), 0);

  // repeadly add video source, wrong!
  EXPECT_EQ(src->AddSource(handler1), -1);
  EXPECT_EQ(src->AddSource(handler2), -1);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  src->Close();

  // filename.empty(), return -1
  auto handler_error = FileHandler::Create(src.get(), std::to_string(5), "", 24, false);
  EXPECT_EQ(handler_error, nullptr);

  // filename valid, return 0
  const uint32_t max_test_stream_num = 64;
  for (uint32_t i = 0; i < max_test_stream_num; i++) {
    auto handler = FileHandler::Create(src.get(), std::to_string(i), video_path, 24, false);
    EXPECT_EQ(src->AddSource(handler), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  // open source failed, return -1, TODO(LMX): test max stream size, (MLU270 , video_path can not create 128 streams.)
  // auto handler = FileHandler::Create(src.get(), std::to_string(GetMaxStreamNumber()), video_path, 24, false);
  // EXPECT_EQ(src->AddSource(handler), -1);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
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
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  ASSERT_TRUE(src->Open(param));

  // successfully add video source
  for (int i = 0; i < 10; i++) {
    auto handler = FileHandler::Create(src.get(), std::to_string(i), video_path, 24, false);
    EXPECT_EQ(src->AddSource(handler), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  ASSERT_TRUE(src->Open(param));

  // add source
  auto handler1 = FileHandler::Create(src.get(), stream_id1, video_path, 24, false);
  EXPECT_EQ(src->AddSource(handler1), 0);

  auto handler2 = FileHandler::Create(src.get(), stream_id2, video_path, 24, true);
  EXPECT_EQ(src->AddSource(handler2), 0);

  auto handler3 = FileHandler::Create(src.get(), stream_id3, video_path, 24, false);
  EXPECT_EQ(src->AddSource(handler3), 0);

  auto handler4 = FileHandler::Create(src.get(), stream_id4, image_path, 24, false);
  EXPECT_EQ(src->AddSource(handler4), 0);

  EXPECT_NE(src->AddSource(handler3), 0);
  EXPECT_NE(src->AddSource(handler4), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  EXPECT_EQ(src->RemoveSource(handler1), 0);
  EXPECT_EQ(src->RemoveSource(handler2), 0);

  EXPECT_EQ(src->AddSource(handler1), 0);
  EXPECT_EQ(src->AddSource(handler2), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  src->Close();

  // reuse codec buffer
  param["reuse_cndec_buf"] = "true";
  ASSERT_TRUE(src->Open(param));
  auto handler = FileHandler::Create(src.get(), stream_id1, video_path, 24, false);
  EXPECT_EQ(src->AddSource(handler), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  src->Close();
}

TEST(Source, FFMpegCPU) {
  auto src = std::make_shared<DataSource>(gname);
  std::string video_path = GetExePath() + gvideo_path;
  std::string stream_id1 = "1";
  std::string stream_id2 = "2";
  std::string stream_id3 = "3";

  ModuleParamSet param;
  param["output_type"] = "cpu";
  param["decoder_type"] = "cpu";
  ASSERT_TRUE(src->Open(param));

  // add source

  auto handler1 = FileHandler::Create(src.get(), stream_id1, video_path, 23, false);
  EXPECT_EQ(src->AddSource(handler1), 0);

  auto handler2 = FileHandler::Create(src.get(), stream_id2, video_path, 24, true);
  EXPECT_EQ(src->AddSource(handler2), 0);

  auto handler3 = FileHandler::Create(src.get(), stream_id3, video_path, 25, false);
  EXPECT_EQ(src->AddSource(handler3), 0);

  EXPECT_NE(src->AddSource(handler3), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  EXPECT_EQ(src->RemoveSource(stream_id3), 0);
  EXPECT_EQ(src->RemoveSource(stream_id1), 0);
  EXPECT_EQ(src->RemoveSource(stream_id2), 0);

  param["output_type"] = "mlu";
  param["device_id"] = "0";
  ASSERT_TRUE(src->Open(param));

  auto handler4 = FileHandler::Create(src.get(), stream_id1, video_path, 22, false);
  EXPECT_EQ(src->AddSource(handler4), 0);

  auto handler5 = FileHandler::Create(src.get(), stream_id2, video_path, 21, false);
  EXPECT_EQ(src->AddSource(handler5), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  src->Close();
}

TEST(Source, MemMLU) {
  std::string h264_path = GetExePath() + "../../modules/unitest/source/data/raw.h264";
  auto src = std::make_shared<DataSource>(gname);
  std::string stream_id0 = "0";
  std::string stream_id1 = "1";
  std::string stream_id2 = "2";
  std::string stream_id3 = "3";

  ModuleParamSet param;
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  ASSERT_TRUE(src->Open(param));

  std::vector<std::shared_ptr<SourceHandler>> vec_handlers;
  std::vector<std::thread> vec_threads_mem;

  // add source
  auto handler0 = ESMemHandler::Create(src.get(), stream_id0);
  EXPECT_EQ(src->AddSource(handler0), 0);
  vec_handlers.push_back(handler0);

  auto handler1 = ESMemHandler::Create(src.get(), stream_id1);
  EXPECT_EQ(src->AddSource(handler1), 0);
  vec_handlers.push_back(handler1);

  auto handler2 = ESMemHandler::Create(src.get(), stream_id2);
  EXPECT_EQ(src->AddSource(handler2), 0);
  vec_handlers.push_back(handler2);

  auto handler3 = ESMemHandler::Create(src.get(), stream_id3);
  EXPECT_EQ(src->AddSource(handler3), 0);
  vec_handlers.push_back(handler3);

  EXPECT_NE(src->AddSource(handler3), 0);

  for (auto &handler : vec_handlers) {
    vec_threads_mem.push_back(std::thread([&]() {
      FILE *fp = fopen(h264_path.c_str(), "rb");
      if (fp) {
        auto memHandler = std::dynamic_pointer_cast<cnstream::ESMemHandler>(handler);
        unsigned char buf[4096];
        int read_cnt = 0;
        while (!feof(fp) && read_cnt < 10) {
          int size = fread(buf, 1, 4096, fp);
          memHandler->Write(buf, size);
          read_cnt++;
        }
        memHandler->Write(NULL, 0);
        fclose(fp);
      }
    }));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  src->Close();

  for (auto &thread_id : vec_threads_mem) {
    if (thread_id.joinable()) thread_id.join();
  }
  vec_threads_mem.clear();
  vec_handlers.clear();

  // reuse codec buffer
  param["reuse_cndec_buf"] = "true";
  ASSERT_TRUE(src->Open(param));

  handler0 = ESMemHandler::Create(src.get(), stream_id1);
  EXPECT_EQ(src->AddSource(handler0), 0);
  vec_handlers.push_back(handler0);

  handler1 = ESMemHandler::Create(src.get(), stream_id2);
  EXPECT_EQ(src->AddSource(handler1), 0);
  vec_handlers.push_back(handler1);

  for (auto &handler : vec_handlers) {
    vec_threads_mem.push_back(std::thread([&]() {
      FILE *fp = fopen(h264_path.c_str(), "rb");
      if (fp) {
        auto memHandler = std::dynamic_pointer_cast<cnstream::ESMemHandler>(handler);
        unsigned char buf[4096];
        int read_cnt = 0;
        while (!feof(fp) && read_cnt < 10) {
          int size = fread(buf, 1, 4096, fp);
          memHandler->Write(buf, size);
          read_cnt++;
        }
        memHandler->Write(NULL, 0);
        fclose(fp);
      }
    }));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  src->Close();

  for (auto &thread_id : vec_threads_mem) {
    if (thread_id.joinable()) thread_id.join();
  }
  vec_threads_mem.clear();
  vec_handlers.clear();
}

}  // namespace cnstream
