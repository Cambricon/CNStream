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

#include "data_source.hpp"
#include "test_base.hpp"
#include "data_handler_mem.hpp"
#include "data_handler.hpp"

#define TestSize 10

namespace cnstream {

static constexpr const char *gname = "source";
static constexpr const char *gvideo_path = "../../data/videos/cars.mp4";

static void ResetParam(ModuleParamSet &param) {  // NOLINT
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

TEST(DataHandlerMem, Write) {
  DataSource src(gname);
  ModuleParamSet param;
  ResetParam(param);
  ASSERT_TRUE(src.CheckParamSet(param));
  ASSERT_TRUE(src.Open(param));
  auto handler = std::make_shared<DataHandlerMem>(&src, std::to_string(0), "filename", 30);
  ASSERT_TRUE(handler != nullptr);
  EXPECT_TRUE(handler->module_ == &src);
  EXPECT_TRUE(handler->stream_id_ == std::to_string(0));
  handler->running_.store(1);
  std::string video_path = GetExePath() + gvideo_path;
  FILE *fp = fopen(video_path.c_str(), "rb");
  ASSERT_TRUE(fp != nullptr);
  unsigned char buf[4096];
  int size = fread(buf, 1, 4096, fp);
  EXPECT_EQ(handler->Write(buf, size), size);
  fclose(fp);
}

}  // namespace cnstream

