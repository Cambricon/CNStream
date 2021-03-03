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

#include "cnrt.h"
#include "cnstream_source.hpp"
#include "data_handler_file.hpp"
#include "data_handler_mem.hpp"
#include "data_source.hpp"
#include "util/video_decoder.hpp"
#include "test_base.hpp"

namespace cnstream {

static constexpr const char *gname = "source";
static constexpr const char *gmp4_path = "../../modules/unitest/source/data/img.mp4";
static constexpr const char *gh264_path = "../../modules/unitest/source/data/raw.h264";
static constexpr const char *gimage_path = "../../data/images/%d.jpg";

class PrepareEnvFile {
 public:
  // device = 0 mlu, device = 1 cpu
  explicit PrepareEnvFile(int device, bool img = false) {
    std::string mp4_path = GetExePath() + gmp4_path;
    std::string image_path = GetExePath() + gimage_path;
    src = new DataSource(gname);
    if (img == false) {
      auto handler = FileHandler::Create(src, "0", mp4_path, 30, false);
      file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
    } else {
      auto handler = FileHandler::Create(src, "0", image_path, 30, false);
      file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
    }
    if (device == 0) {
      ModuleParamSet param;
      param["output_type"] = "mlu";
      param["interval"] = "1";
      param["decoder_type"] = "mlu";
      param["device_id"] = "0";
      param["reuse_cndec_buf"] = "false";

      src->Open(param);
      file_handler->impl_->SetDecodeParam(src->GetSourceParam());
      mlu_decoder = std::make_shared<MluDecoder>("fake_id", file_handler->impl_);
    } else {
      ModuleParamSet param;
      param["output_type"] = "cpu";
      param["interval"] = "1";
      param["decoder_type"] = "cpu";

      src->Open(param);
      file_handler->impl_->SetDecodeParam(src->GetSourceParam());
      ffmpeg_cpu_decoder = std::make_shared<FFmpegCpuDecoder>("fake_id", file_handler->impl_);
    }
    info.codec_id = AV_CODEC_ID_H264;
  }

  ~PrepareEnvFile() {
    if (src) delete src;
  }

  DataSource *src = nullptr;
  std::shared_ptr<FileHandler> file_handler;
  std::shared_ptr<MluDecoder> mlu_decoder;
  std::shared_ptr<FFmpegCpuDecoder> ffmpeg_cpu_decoder;
  VideoInfo info;
  ExtraDecoderInfo extra;
  VideoEsPacket pkt;
};  // PrepareEnvFile

class PrepareEnvMem {
 public:
  PrepareEnvMem() {
    std::string h264_path = GetExePath() + gh264_path;
    src = new DataSource(gname);
    auto handler = ESMemHandler::Create(src, "0");
    mem_handler = std::dynamic_pointer_cast<ESMemHandler>(handler);
    ModuleParamSet param;
    param["output_type"] = "mlu";
    param["interval"] = "1";
    param["decoder_type"] = "mlu";
    param["device_id"] = "0";
    param["reuse_cndec_buf"] = "false";

    src->Open(param);
    mem_handler->impl_->SetDecodeParam(src->GetSourceParam());
    mlu_decoder = std::make_shared<MluDecoder>("fake_id", mem_handler->impl_);

    info.codec_id = AV_CODEC_ID_H264;
  }

  ~PrepareEnvMem() {
    if (src) delete src;
  }

  DataSource *src = nullptr;
  std::shared_ptr<ESMemHandler> mem_handler;
  std::shared_ptr<MluDecoder> mlu_decoder;

  VideoInfo info;
  ExtraDecoderInfo extra;
  VideoEsPacket pkt;
};  // PrepareEnvMem

TEST(SourceMluDecoder, CreateDestroyJpeg) {
  PrepareEnvFile env(0, true);
  // mjpeg
  env.info.codec_id = AV_CODEC_ID_MJPEG;
  EXPECT_TRUE(env.mlu_decoder->Create(&env.info, &env.extra));
  env.mlu_decoder->Destroy();
}
// Cpu FFmpeg Decoder
TEST(SourceCpuFFmpegDecoder, CreateDestroy) {
  int device_type = 1;
  PrepareEnvFile env(device_type);

  // h264
  EXPECT_TRUE(env.ffmpeg_cpu_decoder->Create(&env.info, &env.extra));
  env.ffmpeg_cpu_decoder->Destroy();

  // h265
  env.info.codec_id = AV_CODEC_ID_HEVC;
  EXPECT_TRUE(env.ffmpeg_cpu_decoder->Create(&env.info, &env.extra));
  env.ffmpeg_cpu_decoder->Destroy();

  // mjpeg
  PrepareEnvFile env_jpeg(device_type, true);
  env_jpeg.info.codec_id = AV_CODEC_ID_MJPEG;
  EXPECT_TRUE(env_jpeg.ffmpeg_cpu_decoder->Create(&env_jpeg.info, &env_jpeg.extra));
  env_jpeg.ffmpeg_cpu_decoder->Destroy();

  // invalid
  env_jpeg.info.codec_id = AV_CODEC_ID_NONE;
  EXPECT_FALSE(env_jpeg.ffmpeg_cpu_decoder->Create(&env_jpeg.info, &env_jpeg.extra));
  env_jpeg.ffmpeg_cpu_decoder->Destroy();
}

TEST(SourceCpuFFmpegDecoder, Process) {
  PrepareEnvFile env(1);
  EXPECT_TRUE(env.ffmpeg_cpu_decoder->Create(&env.info, &env.extra));
  EXPECT_FALSE(env.ffmpeg_cpu_decoder->Process(&env.pkt));  // EOS
  env.ffmpeg_cpu_decoder->Destroy();
}

// Mlu Mem Decoder
TEST(SourceMluDecoder, CreateDestroy) {
  PrepareEnvMem env;
  // h264
  env.info.codec_id = AV_CODEC_ID_H264;
  EXPECT_TRUE(env.mlu_decoder->Create(&env.info, &env.extra));
  env.mlu_decoder->Destroy();

  // destroy twice, the function will return directly.
  env.mlu_decoder->Destroy();

  env.info.codec_id = AV_CODEC_ID_HEVC;
  EXPECT_TRUE(env.mlu_decoder->Create(&env.info, &env.extra));
  env.mlu_decoder->Destroy();
}

}  // namespace cnstream
