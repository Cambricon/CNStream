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

#include "cnstream_source.hpp"
#include "data_handler_file.hpp"
#include "data_handler_mem.hpp"
#include "data_handler_rtsp.hpp"
#include "data_source.hpp"
#include "test_base.hpp"

namespace cnstream {

static constexpr const char *gname = "source";
static constexpr const char *gmp4_path = "../../modules/unitest/source/data/img.mp4";

// device = 0 represents mlu, device = 1 represents cpu
#if 0
static void OpenHandler(std::shared_ptr<DataHandler> handler, int device) {
  DataSource *source = dynamic_cast<DataSource*>(handler->module_);
  handler->param_ = source->GetSourceParam();
  if (device == 0) {
    handler->dev_ctx_.dev_type = DevContext::MLU;
    handler->dev_ctx_.dev_id = 0;
  } else {
    handler->dev_ctx_.dev_type = DevContext::CPU;
    handler->dev_ctx_.dev_id = -1;
  }
  handler->dev_ctx_.ddr_channel = handler->stream_index_ % 4;
}
#endif

class SourceHandlerTest : public SourceHandler {
 public:
  explicit SourceHandlerTest(DataSource *module, const std::string &stream_id) : SourceHandler(module, stream_id) {}
  ~SourceHandlerTest() {}

  bool Open() override { return true; }
  void Close() override {}
};

TEST(SourceHandler, Construct) {
  DataSource src(gname);
  auto handler = std::make_shared<SourceHandlerTest>(&src, std::to_string(0));
  EXPECT_TRUE(handler != nullptr);
}

TEST(SourceHandler, GetStreamId) {
  DataSource src(gname);
  auto handler = std::make_shared<SourceHandlerTest>(&src, std::to_string(123));
  ASSERT_TRUE(handler != nullptr);
  EXPECT_TRUE(handler->GetStreamId() == std::to_string(123));
  auto handler_2 = std::make_shared<SourceHandlerTest>(&src, std::to_string(2));
  ASSERT_TRUE(handler_2 != nullptr);
  EXPECT_TRUE(handler_2->GetStreamId() == std::to_string(2));
  auto handler_3 = std::make_shared<SourceHandlerTest>(&src, std::to_string(100));
  ASSERT_TRUE(handler_3 != nullptr);
  EXPECT_TRUE(handler_3->GetStreamId() == std::to_string(100));
}

TEST(DataHandlerFile, OpenClose) {
  std::string mp4_path = GetExePath() + gmp4_path;
  DataSource *psrc = nullptr;
  auto handler_wrong = FileHandler::Create(psrc, std::to_string(0), mp4_path, 30, false);
  ASSERT_TRUE(handler_wrong == nullptr);

  DataSource src(gname);
  auto handler = FileHandler::Create(&src, std::to_string(0), mp4_path, 30, false);
  ModuleParamSet param;
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(handler->Open());
  EXPECT_EQ(handler->GetStreamId(), "0");
  handler->Close();

  param["output_type"] = "cpu";
  param["decoder_type"] = "cpu";
  param["device_id"] = "-1";
  src.Open(param);
  EXPECT_TRUE(handler->Open());
  EXPECT_EQ(handler->GetStreamId(), "0");
  handler->Close();
}

TEST(DataHandlerFile, PrepareResources) {
  DataSource src(gname);
  std::string h264_path = GetExePath() + "../../modules/unitest/source/data/img.h264";
  std::string flv_path = GetExePath() + "../../modules/unitest/source/data/img.flv";
  std::string mkv_path = GetExePath() + "../../modules/unitest/source/data/img.mkv";
  std::string mp4_path = GetExePath() + "../../modules/unitest/source/data/img.mp4";
  std::string h265_path = GetExePath() + "../../modules/unitest/source/data/265.mp4";
  std::string car_path = GetExePath() + "../../modules/unitest/source/data/cars_short.mp4";

  // H264
  auto handler = FileHandler::Create(&src, std::to_string(0), h264_path, 30, false);
  auto file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
  EXPECT_TRUE(file_handler->impl_->PrepareResources());
  file_handler->impl_->ClearResources();
  // flv
  handler = FileHandler::Create(&src, std::to_string(0), flv_path, 30, false);
  file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
  EXPECT_TRUE(file_handler->impl_->PrepareResources());
  file_handler->impl_->ClearResources();
  // mkv
  handler = FileHandler::Create(&src, std::to_string(0), mkv_path, 30, false);
  file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
  EXPECT_TRUE(file_handler->impl_->PrepareResources());
  file_handler->impl_->ClearResources();
  // mp4
  handler = FileHandler::Create(&src, std::to_string(0), mp4_path, 30, false);
  file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
  EXPECT_TRUE(file_handler->impl_->PrepareResources());
  file_handler->impl_->ClearResources();
  // H265
  handler = FileHandler::Create(&src, std::to_string(0), h265_path, 30, false);
  file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
  EXPECT_TRUE(file_handler->impl_->PrepareResources());
  file_handler->impl_->ClearResources();

  ModuleParamSet param;

  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";

  // mlu decoder
  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(file_handler->Open());
  file_handler->Close();
  EXPECT_TRUE(file_handler->impl_->PrepareResources());
  file_handler->impl_->ClearResources();

  // cpu decoder
  param["decoder_type"] = "cpu";
  param["output_type"] = "cpu";
  param["device_id"] = "-1";

  EXPECT_TRUE(src.Open(param));
  auto cpu_handler = FileHandler::Create(&src, std::to_string(0), car_path, 30, false);
  auto cpu_file_handler = std::dynamic_pointer_cast<FileHandler>(cpu_handler);
  EXPECT_TRUE(cpu_file_handler->Open());
  cpu_file_handler->Close();
  EXPECT_TRUE(cpu_file_handler->impl_->PrepareResources());
  cpu_file_handler->impl_->ClearResources();
}

TEST(DataHandlerFile, ProcessMlu) {
  DataSource src(gname);
  std::string h264_path = GetExePath() + "../../modules/unitest/source/data/img.h264";
  // std::string flv_path = GetExePath() + "../../modules/unitest/source/data/img.flv";
  std::string mkv_path = GetExePath() + "../../modules/unitest/source/data/img.mkv";
  std::string mp4_path = GetExePath() + "../../modules/unitest/source/data/img.mp4";
  std::string hevc_path = GetExePath() + "../../modules/unitest/source/data/img.hevc";
  std::string car_path = GetExePath() + "../../modules/unitest/source/data/cars_short.mp4";

  // H264
  auto handler = FileHandler::Create(&src, std::to_string(0), mp4_path, 30, false);
  auto file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
  ModuleParamSet param;
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";

  EXPECT_TRUE(src.Open(param));
  file_handler->impl_->SetDecodeParam(src.GetSourceParam());
  EXPECT_TRUE(file_handler->impl_->PrepareResources());

  // img.mp4 has 5 frames
  for (uint32_t i = 0; i < 5; i++) {
    EXPECT_TRUE(file_handler->impl_->Process());
  }
  // loop is set to false, send eos and return false
  EXPECT_FALSE(file_handler->impl_->Process());

  file_handler->impl_->ClearResources();

  // set loop to true
  handler = FileHandler::Create(&src, std::to_string(0), mp4_path, 30, true);
  file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
  file_handler->impl_->SetDecodeParam(src.GetSourceParam());
  EXPECT_TRUE(file_handler->impl_->PrepareResources());

  uint32_t loop = 10;
  while (loop--) {
    // img.mp4 has 5 frames
    for (uint32_t i = 0; i < 5; i++) {
      EXPECT_TRUE(file_handler->impl_->Process());
    }
    // loop is set to true, do not send eos and return true
    EXPECT_TRUE(file_handler->impl_->Process());
  }

  file_handler->impl_->ClearResources();

  // reuse codec buffer
  param["reuse_cndec_buf"] = "true";
  handler = FileHandler::Create(&src, std::to_string(0), mp4_path, 30, false);
  file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
  EXPECT_TRUE(src.Open(param));
  file_handler->impl_->SetDecodeParam(src.GetSourceParam());
  EXPECT_TRUE(file_handler->impl_->PrepareResources());
  EXPECT_TRUE(file_handler->impl_->Process());
  file_handler->impl_->ClearResources();

  // h264
  handler = FileHandler::Create(&src, std::to_string(0), h264_path, 30, false);
  file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
  file_handler->impl_->SetDecodeParam(src.GetSourceParam());
  EXPECT_TRUE(file_handler->impl_->PrepareResources());
  EXPECT_TRUE(file_handler->impl_->Process());
  file_handler->impl_->ClearResources();

  // mkv
  handler = FileHandler::Create(&src, std::to_string(0), mkv_path, 30, false);
  file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
  file_handler->impl_->SetDecodeParam(src.GetSourceParam());
  EXPECT_TRUE(file_handler->impl_->PrepareResources());
  EXPECT_TRUE(file_handler->impl_->Process());
  file_handler->impl_->ClearResources();

  // mp4
  handler = FileHandler::Create(&src, std::to_string(0), mp4_path, 30, false);
  file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
  file_handler->impl_->SetDecodeParam(src.GetSourceParam());
  EXPECT_TRUE(file_handler->impl_->PrepareResources());
  EXPECT_TRUE(file_handler->impl_->Process());
  file_handler->impl_->ClearResources();

  // HEVC
  handler = FileHandler::Create(&src, std::to_string(0), hevc_path, 30, false);
  file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
  file_handler->impl_->SetDecodeParam(src.GetSourceParam());
  EXPECT_TRUE(file_handler->impl_->PrepareResources());
  EXPECT_TRUE(file_handler->impl_->Process());
  file_handler->impl_->ClearResources();
}

TEST(DataHandlerFile, ProcessCpu) {
  DataSource src(gname);
  std::string mp4_path = GetExePath() + "../../modules/unitest/source/data/cars_short.mp4";
  // H264
  auto handler = FileHandler::Create(&src, std::to_string(0), mp4_path, 30, false);
  auto file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
  ModuleParamSet param;
  param["output_type"] = "cpu";
  param["decoder_type"] = "cpu";

  EXPECT_TRUE(src.Open(param));
  EXPECT_TRUE(file_handler->Open());
  file_handler->Close();

  EXPECT_TRUE(file_handler->impl_->PrepareResources());
  // cars.mp4 has 11 frames
  for (uint32_t i = 0; i < 11; i++) {
    EXPECT_TRUE(file_handler->impl_->Process());
  }
  // loop is set to false, send eos and return false
  EXPECT_FALSE(file_handler->impl_->Process());

  file_handler->impl_->ClearResources();
}

}  // namespace cnstream
