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

#include "cnrt.h"
#include "cnstream_source.hpp"
#include "data_handler_file.hpp"
#include "data_handler_mem.hpp"
#include "data_source.hpp"
#include "device/mlu_context.h"
#include "ffmpeg_decoder.hpp"
#include "test_base.hpp"

namespace cnstream {

#define TEST_FFMPEG_VERSION_3_1 AV_VERSION_INT(57, 40, 100)

static constexpr const char *gname = "source";
static constexpr const char *gmp4_path = "../../modules/unitest/source/data/img.mp4";
static constexpr const char *gh264_path = "../../modules/unitest/source/data/raw.h264";
static constexpr const char *gimage_path = "../../data/images/%d.jpg";

static const int g_dev_id = 0;
static const int g_ddr_channel = 0;

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
      mlu_decoder = std::make_shared<MluDecoder>(file_handler->impl_);
    } else {
      ModuleParamSet param;
      param["output_type"] = "cpu";
      param["interval"] = "1";
      param["decoder_type"] = "cpu";

      src->Open(param);
      file_handler->impl_->SetDecodeParam(src->GetSourceParam());
      ffmpeg_cpu_decoder = std::make_shared<FFmpegCpuDecoder>(file_handler->impl_);
    }
    st = new AVStream();
    av_pkt = new AVPacket();
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
    codec_param = new AVCodecParameters();
    st->codecpar = codec_param;
    st->codecpar->codec_id = AV_CODEC_ID_H264;
    st->codecpar->width = 256;
    st->codecpar->height = 256;
#else
    codec_ctx = new AVCodecContext();
    st->codec = codec_ctx;
    st->codec->codec_id = AV_CODEC_ID_H264;
    st->codec->width = 256;
    st->codec->height = 256;
#endif
  }

  ~PrepareEnvFile() {
    delete av_pkt;
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
    delete codec_param;
#else
    delete codec_ctx;
#endif
    delete st;
    delete src;
  }

  DataSource *src;
  std::shared_ptr<FileHandler> file_handler;
  std::shared_ptr<MluDecoder> mlu_decoder;
  std::shared_ptr<FFmpegCpuDecoder> ffmpeg_cpu_decoder;
  AVStream *st;
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  AVCodecParameters *codec_param;
#else
  AVCodecContext *codec_ctx;
#endif
  AVPacket *av_pkt;
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
    mlu_decoder = std::make_shared<MluDecoder>(mem_handler->impl_);
    st = new AVStream();
    av_pkt = new AVPacket();
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
    codec_param = new AVCodecParameters();
    st->codecpar = codec_param;
    st->codecpar->codec_id = AV_CODEC_ID_H264;
    st->codecpar->width = 256;
    st->codecpar->height = 256;
#else
    codec_ctx = new AVCodecContext();
    st->codec = codec_ctx;
    st->codec->codec_id = AV_CODEC_ID_H264;
    st->codec->width = 256;
    st->codec->height = 256;
#endif
  }

  ~PrepareEnvMem() {
    delete src;
    delete av_pkt;
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
    delete codec_param;
#else
    delete codec_ctx;
#endif
    delete st;
  }

  DataSource *src;
  std::shared_ptr<ESMemHandler> mem_handler;
  std::shared_ptr<MluDecoder> mlu_decoder;
  AVStream *st;
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  AVCodecParameters *codec_param;
#else
  AVCodecContext *codec_ctx;
#endif
  AVPacket *av_pkt;
};  // PrepareEnvMem

/*
// Mlu FFmpeg Decoder
TEST(SourceMluFFmpegDecoder, CreateDestroy) {
  PrepareEnvFile env(0);
  // h264
  EXPECT_TRUE(env.mlu_decoder->Create(env.st));
  env.mlu_decoder->Destroy();

  // h265
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  env.st->codecpar->codec_id = AV_CODEC_ID_HEVC;
#else
  env.st->codec->codec_id = AV_CODEC_ID_HEVC;
#endif
  EXPECT_TRUE(env.mlu_decoder->Create(env.st));
  env.mlu_decoder->Destroy();

  // invalid
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  env.st->codecpar->codec_id = AV_CODEC_ID_NONE;
#else
  env.st->codec->codec_id = AV_CODEC_ID_NONE;
#endif
  EXPECT_FALSE(env.mlu_decoder->Create(env.st));
  env.mlu_decoder->Destroy();

  env.mlu_decoder->Destroy();
}
*/

TEST(SourceMluFFmpegDecoder, CreateDestroyJpeg) {
  PrepareEnvFile env(0, true);
  // mjpeg
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  env.st->codecpar->codec_id = AV_CODEC_ID_MJPEG;
#else
  env.st->codec->codec_id = AV_CODEC_ID_MJPEG;
#endif

  EXPECT_TRUE(env.mlu_decoder->Create(env.st));

  env.mlu_decoder->Destroy();
}

/*
TEST(SourceMluFFmpegDecoder, Process) {
  PrepareEnvFile env(0);

  EXPECT_TRUE(env.mlu_decoder->Create(env.st));

  EXPECT_TRUE(env.mlu_decoder->Process(env.av_pkt, false));
  // eos
  EXPECT_TRUE(env.mlu_decoder->Process(env.av_pkt, true));
  env.mlu_decoder->Destroy();
}
*/

TEST(SourceMluFFmpegDecoder, ProcessFrame) {
  CNS_CNRT_CHECK(cnrtInit(0));
  PrepareEnvFile env(0);

  cnvideoDecOutput out;
  out.pts = 0;
  out.frame.deviceId = g_dev_id;
  out.frame.channel = g_ddr_channel;
  out.frame.height = 256;
  out.frame.width = 256;
  out.frame.pixelFmt = CNCODEC_PIX_FMT_NV12;
  out.frame.stride[0] = 256;
  out.frame.stride[1] = 256;
  void *mlu_ptr = nullptr;
  CALL_CNRT_BY_CONTEXT(cnrtMalloc(&mlu_ptr, 256 * 256 * 3 / 2), g_dev_id, g_ddr_channel);
  out.frame.plane[0].addr = reinterpret_cast<u64_t>(mlu_ptr);
  out.frame.plane[1].addr = reinterpret_cast<u64_t>(reinterpret_cast<char *>(mlu_ptr) + 256 * 256);

  bool reused = false;
  EXPECT_EQ(env.mlu_decoder->ProcessFrame(&out, &reused), 0);
  EXPECT_FALSE(reused);

  env.mlu_decoder->Destroy();

  cnrtFree(mlu_ptr);
}

// Cpu FFmpeg Decoder
TEST(SourceCpuFFmpegDecoder, CreateDestroy) {
  int device_type = 1;
  PrepareEnvFile env(device_type);

  // h264
  EXPECT_TRUE(env.ffmpeg_cpu_decoder->Create(env.st));
  env.ffmpeg_cpu_decoder->Destroy();

  // h265
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  env.st->codecpar->codec_id = AV_CODEC_ID_HEVC;
#else
  env.st->codec->codec_id = AV_CODEC_ID_HEVC;
#endif
  EXPECT_TRUE(env.ffmpeg_cpu_decoder->Create(env.st));
  env.ffmpeg_cpu_decoder->Destroy();

  // mjpeg
  PrepareEnvFile env_jpeg(device_type, true);
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  env_jpeg.st->codecpar->codec_id = AV_CODEC_ID_MJPEG;
#else
  env_jpeg.st->codec->codec_id = AV_CODEC_ID_MJPEG;
#endif
  EXPECT_TRUE(env_jpeg.ffmpeg_cpu_decoder->Create(env_jpeg.st));
  env_jpeg.ffmpeg_cpu_decoder->Destroy();

  // invalid
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  env_jpeg.st->codecpar->codec_id = AV_CODEC_ID_NONE;
#else
  env_jpeg.st->codec->codec_id = AV_CODEC_ID_NONE;
#endif
  EXPECT_FALSE(env_jpeg.ffmpeg_cpu_decoder->Create(env_jpeg.st));
  env_jpeg.ffmpeg_cpu_decoder->Destroy();
}

TEST(SourceCpuFFmpegDecoder, Process) {
  PrepareEnvFile env(1);

  EXPECT_TRUE(env.ffmpeg_cpu_decoder->Create(env.st));

  EXPECT_TRUE(env.ffmpeg_cpu_decoder->Process(env.av_pkt, false));
  // eos
  EXPECT_FALSE(env.ffmpeg_cpu_decoder->Process(env.av_pkt, true));
  env.ffmpeg_cpu_decoder->Destroy();
}

#if 0
TEST(SourceCpuFFmpegDecoder, ProcessEmptyFrame) {
  PrepareEnv env(1);

  AVFrame *frame = nullptr;
  env.ffmpeg_cpu_decoder->ResetCount(3);
  EXPECT_FALSE(env.ffmpeg_cpu_decoder->ProcessFrame(frame));
  EXPECT_TRUE(env.ffmpeg_cpu_decoder->ProcessFrame(frame));
  EXPECT_TRUE(env.ffmpeg_cpu_decoder->ProcessFrame(frame));
  EXPECT_FALSE(env.ffmpeg_cpu_decoder->ProcessFrame(frame));

  env.ffmpeg_cpu_decoder = std::make_shared<FFmpegCpuDecoder>(env.ffmpeg_handler);
  uint32_t loop_num = 5;
  // stream id is empty string, discard frame 5 times
  while (loop_num--) {
    EXPECT_FALSE(env.ffmpeg_cpu_decoder->ProcessFrame(frame));
  }
  env.ffmpeg_cpu_decoder->Destroy();
}
#endif

TEST(SourceCpuFFmpegDecoder, ProcessFrameInvalidContext) {
  PrepareEnvFile env(1);
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  env.codec_param->format = AVPixelFormat::AV_PIX_FMT_NONE;
  env.st->codecpar = env.codec_param;
#else
  env.codec_ctx->pix_fmt = AVPixelFormat::AV_PIX_FMT_NONE;
  env.st->codec = env.codec_ctx;
#endif
  EXPECT_TRUE(env.ffmpeg_cpu_decoder->Create(env.st));

  AVFrame *frame = nullptr;
  EXPECT_FALSE(env.ffmpeg_cpu_decoder->ProcessFrame(frame));

  env.ffmpeg_cpu_decoder->Destroy();

  // create eos frame for clear stream idx
  // CNFrameInfo::Create("0", true);
}

// Mlu Mem Decoder
TEST(SourceMluRawDecoder, CreateDestroy) {
  PrepareEnvMem env;

  // h264
  EXPECT_TRUE(env.mlu_decoder->Create(env.st));
  env.mlu_decoder->Destroy();

  // destroy twice, the function will return directly.
  env.mlu_decoder->Destroy();

  // h265
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  env.st->codecpar->codec_id = AV_CODEC_ID_HEVC;
#else
  env.st->codec->codec_id = AV_CODEC_ID_HEVC;
#endif
  EXPECT_TRUE(env.mlu_decoder->Create(env.st));
  env.mlu_decoder->Destroy();
}

}  // namespace cnstream
