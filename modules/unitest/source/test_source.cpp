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
#include "test_base.hpp"

namespace cnstream {

static constexpr const char *gname = "source";
static constexpr const char *gvideo_path = "../../samples/data/videos/cars.mp4";
static constexpr const char *gimage_path = "../../samples/data/images/%d.jpg";

TEST(Source, Construct) {
  std::shared_ptr<Module> src = std::make_shared<DataSource>(gname);
  EXPECT_STREQ(src->GetName().c_str(), gname);
}

TEST(Source, OpenClose) {
  std::shared_ptr<Module> src = std::make_shared<DataSource>(gname);
  ModuleParamSet param;

  // invalid source type
  param["source_type"] = "foo";
  EXPECT_FALSE(src->Open(param));
  param.clear();

  // invalid output type
  param["source_type"] = "ffmpeg";
  param["output_type"] = "bar";
  EXPECT_FALSE(src->Open(param));
  param.clear();

  // mlu decode with device id unset
  param["source_type"] = "ffmpeg";
  param["output_type"] = "mlu";
  EXPECT_FALSE(src->Open(param));
  param.clear();
  param["source_type"] = "ffmpeg";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  EXPECT_FALSE(src->Open(param));
  param.clear();

  // negative interval
  param["source_type"] = "ffmpeg";
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["interval"] = "-1";
  EXPECT_FALSE(src->Open(param));
  param.clear();

  // invalid decode type
  param["source_type"] = "ffmpeg";
  param["output_type"] = "mlu";
  param["decoder_type"] = "blabla";
  param["device_id"] = "0";
  EXPECT_FALSE(src->Open(param));
  param.clear();

  // raw decode without chunk params
  param["source_type"] = "raw";
  param["output_type"] = "cpu";
  param["decoder_type"] = "cpu";
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

  param["source_type"] = "raw";
  param["output_type"] = "cpu";
  param["decoder_type"] = "cpu";
  param["chunk_size"] = "16384";
  param["width"] = "1920";
  param["height"] = "1080";
  param["interlaced"] = "0";
  EXPECT_TRUE(src->Open(param));
  param.clear();
  src->Close();
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
  EXPECT_EQ(src->AddImageSource(stream_id4, image_path, false), 0);

  EXPECT_NE(src->AddVideoSource(stream_id3, video_path, 24), 0);
  EXPECT_NE(src->AddImageSource(stream_id4, image_path, false), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  EXPECT_EQ(src->RemoveSource(stream_id1), 0);
  EXPECT_EQ(src->RemoveSource(stream_id2), 0);

  EXPECT_EQ(src->AddVideoSource(stream_id1, video_path, 24), 0);
  EXPECT_EQ(src->AddImageSource(stream_id2, image_path, false), 0);

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
  param["source_type"] = "ffmpeg";
  param["output_type"] = "cpu";
  param["decoder_type"] = "cpu";
  param["device_id"] = "0";
  ASSERT_TRUE(src->Open(param));

  // add source
  EXPECT_EQ(src->AddVideoSource(stream_id1, video_path, 23), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id2, video_path, 24, true), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id3, video_path, 25, false), 0);

  EXPECT_NE(src->AddVideoSource(stream_id3, video_path, 26), 0);
  EXPECT_NE(src->AddVideoSource(stream_id1, video_path, 27), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  EXPECT_EQ(src->RemoveSource(stream_id1), 0);
  EXPECT_EQ(src->RemoveSource(stream_id2), 0);

  EXPECT_EQ(src->AddVideoSource(stream_id1, video_path, 22), 0);
  EXPECT_EQ(src->AddVideoSource(stream_id2, video_path, 21), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  src->Close();
}

}  // namespace cnstream

