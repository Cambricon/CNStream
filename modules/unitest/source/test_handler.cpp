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
#include <vector>

#include "cnstream_source.hpp"
#include "data_handler_ffmpeg.hpp"
#include "data_handler_raw.hpp"
#include "data_source.hpp"
#include "test_base.hpp"

namespace cnstream {

static constexpr const char *gname = "source";

class DataHandlerTest : public DataHandler {
 public:
  explicit DataHandlerTest(DataSource *module, const std::string &stream_id, int framerate, bool loop)
      : DataHandler(module, stream_id, framerate, loop) {}
  ~DataHandlerTest() { Close(); }
  void SetPrepare(bool prepare) { prepare_ = prepare; }

 private:
  bool PrepareResources(bool demux_only = false) override { return prepare_; }
  void ClearResources(bool demux_only = false) override {}
  bool Process() override {
    if (loop_) {
      loop_--;
      return true;
    } else {
      return false;
    }
  }
  bool prepare_ = true;
  uint32_t loop_ = 5;
};

TEST(SourceHandler, Construct) {
  DataSource src(gname);
  auto handler = std::make_shared<DataHandlerTest>(&src, std::to_string(0), 30, false);
  EXPECT_TRUE(handler != nullptr);
}

TEST(SourceHandler, GetStreamId) {
  DataSource src(gname);
  auto handler = std::make_shared<DataHandlerTest>(&src, std::to_string(123), 30, false);
  ASSERT_TRUE(handler != nullptr);
  EXPECT_TRUE(handler->GetStreamId() == std::to_string(123));
  EXPECT_EQ(handler->GetStreamIndex(), (unsigned int)0);
  auto handler_2 = std::make_shared<DataHandlerTest>(&src, std::to_string(2), 30, false);
  ASSERT_TRUE(handler_2 != nullptr);
  EXPECT_TRUE(handler_2->GetStreamId() == std::to_string(2));
  EXPECT_EQ(handler_2->GetStreamIndex(), (unsigned int)1);
  auto handler_3 = std::make_shared<DataHandlerTest>(&src, std::to_string(100), 30, false);
  ASSERT_TRUE(handler_3 != nullptr);
  EXPECT_TRUE(handler_3->GetStreamId() == std::to_string(100));
  EXPECT_EQ(handler_3->GetStreamIndex(), (unsigned int)2);
}

TEST(SourceHandler, OpenClose) {
  DataSource *psrc = nullptr;
  auto handler_wrong = std::make_shared<DataHandlerTest>(psrc, std::to_string(0), 30, false);
  ASSERT_TRUE(handler_wrong != nullptr);
  // source module is nullptr, should return false
  EXPECT_FALSE(handler_wrong->Open());

  DataSource src(gname);
  auto handler = std::make_shared<DataHandlerTest>(&src, std::to_string(0), 30, false);
  ModuleParamSet param;
  param["source_type"] = "ffmpeg";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "2";
  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(handler->Open());
  DevContext dev_ctx = handler->GetDevContext();
  EXPECT_TRUE(dev_ctx.dev_type == DevContext::MLU);
  EXPECT_EQ(dev_ctx.dev_id, 2);
  // EXPECT_EQ(dev_ctx.ddr_channel, 1);
  EXPECT_EQ(handler->GetStreamId(), "0");
  // EXPECT_EQ(handler->GetStreamIndex(), (unsigned int)1);
  handler->Close();

  param["source_type"] = "ffmpeg";
  param["output_type"] = "cpu";
  param["decoder_type"] = "cpu";
  src.Open(param);
  EXPECT_TRUE(handler->Open());
  dev_ctx = handler->GetDevContext();
  EXPECT_TRUE(dev_ctx.dev_type == DevContext::CPU);
  EXPECT_EQ(dev_ctx.dev_id, -1);
  // EXPECT_EQ(dev_ctx.ddr_channel, 1);
  EXPECT_EQ(handler->GetStreamId(), "0");
  // EXPECT_EQ(handler->GetStreamIndex(), (unsigned int)1);
  handler->Close();

  uint32_t seed = time(0);
  uint32_t random_num = (uint32_t)rand_r(&seed) % 60 + 3;

  // stream from 0 to random num
  std::vector<std::shared_ptr<DataHandlerTest>> handlers;
  for (uint32_t i = 2; i < random_num; i++) {
    handlers.push_back(std::make_shared<DataHandlerTest>(&src, std::to_string(i), 30, false));
  }

  // stream random num
  auto handler_rand = std::make_shared<DataHandlerTest>(&src, std::to_string(random_num), 30, false);
  EXPECT_TRUE(handler_rand->Open());
  dev_ctx = handler_rand->GetDevContext();
  EXPECT_TRUE(dev_ctx.dev_type == DevContext::CPU);
  EXPECT_EQ(dev_ctx.dev_id, -1);
  // EXPECT_TRUE((uint32_t)dev_ctx.ddr_channel == (random_num % 4));
  EXPECT_TRUE(handler_rand->GetStreamId() == std::to_string(random_num));
  // EXPECT_TRUE(handler_rand->GetStreamIndex() == random_num);
  handler_rand->Close();
  /*
    // stream from random num to 63
    for (uint32_t i = random_num; i < 64; i++) {
      handlers.push_back(std::make_shared<DataHandlerTest>(&src, std::to_string(i), 30, false));
    }

    // Already has 64 streams, can not get valid stream index
    auto handler_65 = std::make_shared<DataHandlerTest>(&src, std::to_string(64), 30, false);
    EXPECT_FALSE(handler_65->Open());
  */
}

TEST(SourceHandler, Loop) {
  DataSource src(gname);
  auto handler = std::make_shared<DataHandlerTest>(&src, std::to_string(0), 30, false);
  ModuleParamSet param;
  param["source_type"] = "ffmpeg";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "2";

  // prepare resource default true
  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(handler->Open());
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  handler->Close();
  // set prepare resource to false
  handler->SetPrepare(false);
  EXPECT_TRUE(handler->Open());
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  handler->Close();
}

TEST(SourceHandlerFFmpeg, CheckTimeOut) {
  const char *rtmp_path = "rtmp://";
  DataSource src(gname);
  auto ffmpeg_handler = std::make_shared<DataHandlerFFmpeg>(&src, std::to_string(0), rtmp_path, 30, false);
  EXPECT_FALSE(ffmpeg_handler->PrepareResources());
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  // less than 3 seconds, not timeout
  EXPECT_FALSE(ffmpeg_handler->CheckTimeOut(ts.tv_sec * 1000 + ts.tv_nsec / 1000000));
  // greater than 3 seconds, timeout
  EXPECT_TRUE(ffmpeg_handler->CheckTimeOut(ts.tv_sec * 1000 + ts.tv_nsec / 1000000 + 5000));
}

TEST(SourceHandlerFFmpeg, PrepareResources) {
  DataSource src(gname);
  std::string h264_path = GetExePath() + "../../modules/unitest/source/data/img.h264";
  std::string flv_path = GetExePath() + "../../modules/unitest/source/data/img.flv";
  std::string mkv_path = GetExePath() + "../../modules/unitest/source/data/img.mkv";
  std::string mp4_path = GetExePath() + "../../modules/unitest/source/data/img.mp4";
  std::string h265_path = GetExePath() + "../../modules/unitest/source/data/265.mp4";
  std::string car_path = GetExePath() + "../../modules/unitest/source/data/cars_short.mp4";

  // H264
  auto ffmpeg_handler = std::make_shared<DataHandlerFFmpeg>(&src, std::to_string(0), h264_path, 30, false);
  EXPECT_TRUE(ffmpeg_handler->PrepareResources());
  // flv
  ffmpeg_handler = std::make_shared<DataHandlerFFmpeg>(&src, std::to_string(0), flv_path, 30, false);
  EXPECT_TRUE(ffmpeg_handler->PrepareResources());
  // mkv
  ffmpeg_handler = std::make_shared<DataHandlerFFmpeg>(&src, std::to_string(0), mkv_path, 30, false);
  EXPECT_TRUE(ffmpeg_handler->PrepareResources());
  // mp4
  ffmpeg_handler = std::make_shared<DataHandlerFFmpeg>(&src, std::to_string(0), mp4_path, 30, false);
  EXPECT_TRUE(ffmpeg_handler->PrepareResources());
  // H265
  ffmpeg_handler = std::make_shared<DataHandlerFFmpeg>(&src, std::to_string(0), h265_path, 30, false);
  EXPECT_TRUE(ffmpeg_handler->PrepareResources());

  ModuleParamSet param;
  param["source_type"] = "ffmpeg";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";

  // mlu decoder
  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(ffmpeg_handler->Open());
  ffmpeg_handler->Close();
  EXPECT_TRUE(ffmpeg_handler->PrepareResources());

  ffmpeg_handler->ClearResources();

  // cpu decoder
  param["decoder_type"] = "cpu";
  param["output_type"] = "cpu";
  EXPECT_TRUE(src.Open(param));
  ffmpeg_handler = std::make_shared<DataHandlerFFmpeg>(&src, std::to_string(0), car_path, 30, false);
  EXPECT_TRUE(ffmpeg_handler->Open());
  ffmpeg_handler->Close();
  EXPECT_TRUE(ffmpeg_handler->PrepareResources());

  ffmpeg_handler->ClearResources();
}

TEST(SourceHandlerFFmpeg, Extract) {
  DataSource src(gname);
  std::string h264_path = GetExePath() + "../../modules/unitest/source/data/img.h264";
  // H264
  auto ffmpeg_handler = std::make_shared<DataHandlerFFmpeg>(&src, std::to_string(0), h264_path, 30, false);
  EXPECT_TRUE(ffmpeg_handler->PrepareResources());
  // img.mp4 has 5 frames
  for (uint32_t i = 0; i < 5; i++) {
    EXPECT_TRUE(ffmpeg_handler->Extract());
  }
  EXPECT_FALSE(ffmpeg_handler->Extract());
  ffmpeg_handler->ClearResources();
}

TEST(SourceHandlerFFmpeg, ProcessMlu) {
  DataSource src(gname);
  std::string mp4_path = GetExePath() + "../../modules/unitest/source/data/img.mp4";
  // H264
  auto ffmpeg_handler = std::make_shared<DataHandlerFFmpeg>(&src, std::to_string(0), mp4_path, 30, false);
  ModuleParamSet param;
  param["source_type"] = "ffmpeg";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";

  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(ffmpeg_handler->Open());
  ffmpeg_handler->Close();
  EXPECT_TRUE(ffmpeg_handler->PrepareResources());

  // img.mp4 has 5 frames
  for (uint32_t i = 0; i < 5; i++) {
    EXPECT_TRUE(ffmpeg_handler->Process());
  }
  // loop is set to false, send eos and return false
  EXPECT_FALSE(ffmpeg_handler->Process());

  ffmpeg_handler->ClearResources();

  // set loop to true
  ffmpeg_handler = std::make_shared<DataHandlerFFmpeg>(&src, std::to_string(0), mp4_path, 30, true);

  EXPECT_TRUE(ffmpeg_handler->Open());
  ffmpeg_handler->Close();
  EXPECT_TRUE(ffmpeg_handler->PrepareResources());

  uint32_t loop = 10;
  while (loop--) {
    // img.mp4 has 5 frames
    for (uint32_t i = 0; i < 5; i++) {
      EXPECT_TRUE(ffmpeg_handler->Process());
    }
    // loop is set to true, do not send eos and return true
    EXPECT_TRUE(ffmpeg_handler->Process());
  }

  ffmpeg_handler->ClearResources();

  // reuse codec buffer
  param["reuse_cndec_buf"] = "true";
  ffmpeg_handler = std::make_shared<DataHandlerFFmpeg>(&src, std::to_string(0), mp4_path, 30, false);
  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(ffmpeg_handler->Open());
  ffmpeg_handler->Close();
  EXPECT_TRUE(ffmpeg_handler->PrepareResources());
  EXPECT_TRUE(ffmpeg_handler->Process());

  ffmpeg_handler->ClearResources();
}

TEST(SourceHandlerFFmpeg, ProcessCpu) {
  DataSource src(gname);
  std::string mp4_path = GetExePath() + "../../modules/unitest/source/data/cars_short.mp4";
  // H264
  auto ffmpeg_handler = std::make_shared<DataHandlerFFmpeg>(&src, std::to_string(0), mp4_path, 30, false);
  ModuleParamSet param;
  param["source_type"] = "ffmpeg";
  param["output_type"] = "cpu";
  param["decoder_type"] = "cpu";

  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(ffmpeg_handler->Open());
  ffmpeg_handler->Close();
  EXPECT_TRUE(ffmpeg_handler->PrepareResources());

  // cars.mp4 has 11 frames
  for (uint32_t i = 0; i < 11; i++) {
    EXPECT_TRUE(ffmpeg_handler->Process()) << i;
  }
  // loop is set to false, send eos and return false
  EXPECT_FALSE(ffmpeg_handler->Process());

  ffmpeg_handler->ClearResources();
}

TEST(SourceHandlerRaw, PrepareResources) {
  DataSource src(gname);
  std::string h264_path = GetExePath() + "../../modules/unitest/source/data/raw.h264";
  std::string h265_path = GetExePath() + "../../modules/unitest/source/data/raw.h265";
  std::string mp4_path = GetExePath() + "../../modules/unitest/source/data/img.mp4";
  // filename is empty string
  auto raw_handler = std::make_shared<DataHandlerRaw>(&src, std::to_string(0), "", 30, false);
  EXPECT_FALSE(raw_handler->PrepareResources());

  raw_handler->ClearResources();

  // no chunk size
  raw_handler = std::make_shared<DataHandlerRaw>(&src, std::to_string(0), h264_path, 30, false);
  EXPECT_FALSE(raw_handler->PrepareResources());

  ModuleParamSet param;
  param["source_type"] = "raw";
  param["output_type"] = "cpu";
  param["decoder_type"] = "cpu";
  // chunk size 50K
  param["chunk_size"] = "50000";
  param["width"] = "256";
  param["height"] = "256";
  param["interlaced"] = "false";

  // support mlu deccoder oly
  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(raw_handler->Open());
  raw_handler->Close();
  EXPECT_FALSE(raw_handler->PrepareResources());

  // h264
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(raw_handler->Open());
  raw_handler->Close();
  EXPECT_TRUE(raw_handler->PrepareResources());
  raw_handler->ClearResources();

  // h265
  raw_handler = std::make_shared<DataHandlerRaw>(&src, std::to_string(0), h265_path, 30, false);
  EXPECT_TRUE(raw_handler->Open());
  raw_handler->Close();
  EXPECT_TRUE(raw_handler->PrepareResources());
  raw_handler->ClearResources();

  // chunk size 1K
  param["chunk_size"] = "1000";
  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(raw_handler->Open());
  raw_handler->Close();
  EXPECT_TRUE(raw_handler->PrepareResources());
  raw_handler->ClearResources();

  // only support file with extension .h264 .264 and .h265
  raw_handler = std::make_shared<DataHandlerRaw>(&src, std::to_string(0), mp4_path, 30, false);
  EXPECT_FALSE(raw_handler->PrepareResources());

  raw_handler->ClearResources();
}

TEST(SourceHandlerRaw, Extract) {
  DataSource src(gname);
  std::string h264_path = GetExePath() + "../../modules/unitest/source/data/raw.h264";
  auto raw_handler = std::make_shared<DataHandlerRaw>(&src, std::to_string(0), "", 30, false);
  EXPECT_FALSE(raw_handler->Extract());

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
  raw_handler = std::make_shared<DataHandlerRaw>(&src, std::to_string(0), h264_path, 30, false);
  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(raw_handler->Open());
  raw_handler->Close();
  EXPECT_TRUE(raw_handler->PrepareResources());
  // valid data
  EXPECT_TRUE(raw_handler->Extract());
  // invalid data (EOF)
  EXPECT_FALSE(raw_handler->Extract());

  raw_handler->ClearResources();
}

TEST(SourceHandlerRaw, Process) {
  int frame_rate = 30;
  DataSource src(gname);
  std::string h264_path = GetExePath() + "../../modules/unitest/source/data/raw.h264";
  auto raw_handler = std::make_shared<DataHandlerRaw>(&src, std::to_string(0), h264_path, frame_rate, false);

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
  raw_handler = std::make_shared<DataHandlerRaw>(&src, std::to_string(0), h264_path, frame_rate, false);
  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(raw_handler->Open());
  raw_handler->Close();
  EXPECT_TRUE(raw_handler->PrepareResources());
  EXPECT_TRUE(raw_handler->Process());
  EXPECT_FALSE(raw_handler->Process());

  raw_handler->ClearResources();

  raw_handler = std::make_shared<DataHandlerRaw>(&src, std::to_string(0), h264_path, frame_rate, true);
  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(raw_handler->Open());
  raw_handler->Close();
  EXPECT_TRUE(raw_handler->PrepareResources());

  // FIXME
  uint32_t loop = 10;
  while (loop--) {
    // valid data
    EXPECT_TRUE(raw_handler->Process());
    std::chrono::duration<double, std::milli> dura(1000.0 / frame_rate);
    std::this_thread::sleep_for(dura);
    // eos
    EXPECT_TRUE(raw_handler->Process());
    std::this_thread::sleep_for(dura);
  }

  raw_handler->ClearResources();
}

}  // namespace cnstream
