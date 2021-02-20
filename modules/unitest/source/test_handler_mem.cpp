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

#include "data_handler_mem.hpp"
#include "data_source.hpp"
#include "test_base.hpp"

#define TestSize 10

namespace cnstream {

static constexpr const char *gname = "source";
static constexpr const char *gh264_path = "../../modules/unitest/source/data/raw.h264";

static void ResetParam(ModuleParamSet &param) {  // NOLINT
  param["output_type"] = "mlu";
  param["device_id"] = "0";
  param["interval"] = "1";
  param["decoder_type"] = "mlu";
  param["input_buf_number"] = "32";   // Max is 32 due to Codec's limitation
  param["output_buf_number"] = "32";  // Max is 32 due to Codec's limitation
}

TEST(DataHandlerMem, Write) {
  DataSource src(gname);
  ModuleParamSet param;
  ResetParam(param);
  ASSERT_TRUE(src.CheckParamSet(param));
  ASSERT_TRUE(src.Open(param));
  auto handler = ESMemHandler::Create(&src, std::to_string(0));
  ASSERT_TRUE(handler != nullptr);
  EXPECT_TRUE(handler->GetStreamId() == std::to_string(0));
  auto memHandler = std::dynamic_pointer_cast<cnstream::ESMemHandler>(handler);
  EXPECT_EQ(memHandler->SetDataType(ESMemHandler::H264), 0);
  EXPECT_EQ(memHandler->Open(), true);
  std::string video_path = GetExePath() + gh264_path;
  FILE *fp = fopen(video_path.c_str(), "rb");
  ASSERT_TRUE(fp != nullptr);
  unsigned char buf[4096];
  while (!feof(fp)) {
    int size = fread(buf, 1, 4096, fp);
    EXPECT_EQ(memHandler->Write(buf, size), 0);
  }

  std::chrono::seconds sec(2);
  std::this_thread::sleep_for(sec);
  memHandler->Close();
  fclose(fp);
}

}  // namespace cnstream
