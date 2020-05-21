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
#include "data_handler_ffmpeg.hpp"
#include "data_handler_raw.hpp"
#include "data_source.hpp"
#include "easyinfer/mlu_context.h"
#include "ffmpeg_decoder.hpp"
#include "test_base.hpp"

namespace cnstream {

#define TEST_FFMPEG_VERSION_3_1 AV_VERSION_INT(57, 40, 100)

static constexpr const char *gname = "source";
static constexpr const char *gmp4_path = "../../modules/unitest/source/data/img.mp4";
static constexpr const char *gimage_path = "../../data/images/%d.jpg";

class PrepareEnv {
 public:
  // device = 0 mlu, device = 1 cpu
  explicit PrepareEnv(int device, bool img = false) {
    std::string mp4_path = GetExePath() + gmp4_path;
    std::string image_path = GetExePath() + gimage_path;
    src = new DataSource(gname);
    if (img == false) {
      ffmpeg_handler = new DataHandlerFFmpeg(src, "0", mp4_path, 30, false);
    } else {
      ffmpeg_handler = new DataHandlerFFmpeg(src, "0", image_path, 30, false);
    }
    if (device == 0) {
      ModuleParamSet param;
      param["source_type"] = "ffmpeg";
      param["output_type"] = "mlu";
      param["decoder_type"] = "mlu";
      param["device_id"] = "0";

      src->Open(param);
      OpenHandler(0);
      ffmpeg_mlu_decoder = std::make_shared<FFmpegMluDecoder>(*ffmpeg_handler);
    } else {
      ModuleParamSet param;
      param["source_type"] = "ffmpeg";
      param["output_type"] = "cpu";
      param["decoder_type"] = "cpu";

      src->Open(param);
      OpenHandler(1);
      ffmpeg_cpu_decoder = std::make_shared<FFmpegCpuDecoder>(*ffmpeg_handler);
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

  void OpenHandler(int device) {
    if (device == 0) {
      ffmpeg_handler->dev_ctx_.dev_type = DevContext::MLU;
      ffmpeg_handler->dev_ctx_.dev_id = 0;
    } else {
      ffmpeg_handler->dev_ctx_.dev_type = DevContext::CPU;
      ffmpeg_handler->dev_ctx_.dev_id = -1;
    }
    ffmpeg_handler->dev_ctx_.ddr_channel = ffmpeg_handler->stream_index_ % 4;
  }

  ~PrepareEnv() {
    delete av_pkt;
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
    delete codec_param;
#else
    delete codec_ctx;
#endif
    delete st;
    delete ffmpeg_handler;
    delete src;
  }

  DataSource *src;
  DataHandlerFFmpeg *ffmpeg_handler;
  std::shared_ptr<FFmpegMluDecoder> ffmpeg_mlu_decoder;
  std::shared_ptr<FFmpegCpuDecoder> ffmpeg_cpu_decoder;
  AVStream *st;
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  AVCodecParameters *codec_param;
#else
  AVCodecContext *codec_ctx;
#endif
  AVPacket *av_pkt;
};  // PrepareEnv

class PrepareEnvRaw {
 public:
  PrepareEnvRaw() {
    std::string mp4_path = GetExePath() + gmp4_path;
    src = new DataSource(gname);
    raw_handler = new DataHandlerRaw(src, "0", mp4_path, 30, false);

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

    src->Open(param);
    OpenHandler(0);

    raw_mlu_decoder = std::make_shared<RawMluDecoder>(*raw_handler);
    decoder_ctx.height = 256;
    decoder_ctx.width = 256;
    decoder_ctx.codec_id = DecoderContext::CN_CODEC_ID_H264;
    raw_pkt = new RawPacket();
  }

  void OpenHandler(int device) {
    if (device == 0) {
      raw_handler->dev_ctx_.dev_type = DevContext::MLU;
      raw_handler->dev_ctx_.dev_id = 0;
    } else {
      raw_handler->dev_ctx_.dev_type = DevContext::CPU;
      raw_handler->dev_ctx_.dev_id = -1;
    }
    raw_handler->dev_ctx_.ddr_channel = raw_handler->stream_index_ % 4;
  }
  ~PrepareEnvRaw() {
    delete raw_pkt;
    delete raw_handler;
    delete src;
  }

  DataSource *src;
  DataHandlerRaw *raw_handler;
  std::shared_ptr<RawMluDecoder> raw_mlu_decoder;
  DecoderContext decoder_ctx;
  RawPacket *raw_pkt;
};  // PrepareEnvRaw

/*
// Mlu FFmpeg Decoder
TEST(SourceMluFFmpegDecoder, CreateDestroy) {
  PrepareEnv env(0);
  // h264
  EXPECT_TRUE(env.ffmpeg_mlu_decoder->Create(env.st));
  env.ffmpeg_mlu_decoder->Destroy();

  // h265
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  env.st->codecpar->codec_id = AV_CODEC_ID_HEVC;
#else
  env.st->codec->codec_id = AV_CODEC_ID_HEVC;
#endif
  EXPECT_TRUE(env.ffmpeg_mlu_decoder->Create(env.st));
  env.ffmpeg_mlu_decoder->Destroy();

  // invalid
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  env.st->codecpar->codec_id = AV_CODEC_ID_NONE;
#else
  env.st->codec->codec_id = AV_CODEC_ID_NONE;
#endif
  EXPECT_FALSE(env.ffmpeg_mlu_decoder->Create(env.st));
  env.ffmpeg_mlu_decoder->Destroy();

  env.ffmpeg_mlu_decoder->Destroy();
}
*/

TEST(SourceMluFFmpegDecoder, CreateDestroyJpeg) {
  PrepareEnv env(0, true);
  // mjpeg
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  env.st->codecpar->codec_id = AV_CODEC_ID_MJPEG;
#else
  env.st->codec->codec_id = AV_CODEC_ID_MJPEG;
#endif

  EXPECT_TRUE(env.ffmpeg_mlu_decoder->Create(env.st));

  env.ffmpeg_mlu_decoder->Destroy();
}

/*
TEST(SourceMluFFmpegDecoder, Process) {
  PrepareEnv env(0);

  EXPECT_TRUE(env.ffmpeg_mlu_decoder->Create(env.st));

  EXPECT_TRUE(env.ffmpeg_mlu_decoder->Process(env.av_pkt, false));
  // eos
  EXPECT_TRUE(env.ffmpeg_mlu_decoder->Process(env.av_pkt, true));
  env.ffmpeg_mlu_decoder->Destroy();
}
*/

TEST(SourceMluFFmpegDecoder, ProcessEmptyFrame) {
  PrepareEnv env(0);

  delete env.ffmpeg_handler;
  env.ffmpeg_handler = new DataHandlerFFmpeg(env.src, "", "", 30, false);
  env.ffmpeg_mlu_decoder = std::make_shared<FFmpegMluDecoder>(*env.ffmpeg_handler);
  edk::CnFrame frame;
  bool reused = false;
  uint32_t loop_num = 5;
  // stream id is empty string, discard frame 5 times
  while (loop_num--) {
    EXPECT_EQ(env.ffmpeg_mlu_decoder->ProcessFrame(frame, &reused), -1);
    EXPECT_FALSE(reused);
  }
  env.ffmpeg_mlu_decoder->Destroy();
}

TEST(SourceMluFFmpegDecoder, ProcessFrame) {
  PrepareEnv env(0);

  edk::CnFrame frame;
  frame.buf_id = 1;
  frame.height = 256;
  frame.width = 256;
  frame.pformat = edk::PixelFmt::NV12;
  frame.strides[0] = 256;
  frame.strides[1] = 256;
  void *mlu_ptr = nullptr;
  cnrtMalloc(&mlu_ptr, 256 * 256 * 3 / 2);
  frame.ptrs[0] = mlu_ptr;
  frame.ptrs[1] = reinterpret_cast<void *>(reinterpret_cast<char *>(mlu_ptr) + 256 * 256);
  bool reused = false;
  EXPECT_EQ(env.ffmpeg_mlu_decoder->ProcessFrame(frame, &reused), 0);
  EXPECT_FALSE(reused);

  env.ffmpeg_mlu_decoder->Destroy();

  // create eos frame for clear stream idx
  CNFrameInfo::Create("0", true);
  cnrtFree(mlu_ptr);
}

// Cpu FFmpeg Decoder
TEST(SourceCpuFFmpegDecoder, CreateDestroy) {
  int device_type = 1;
  PrepareEnv env(device_type);

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
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  env.st->codecpar->codec_id = AV_CODEC_ID_MJPEG;
#else
  env.st->codec->codec_id = AV_CODEC_ID_MJPEG;
#endif
  delete env.ffmpeg_handler;

  std::string image_path = GetExePath() + gimage_path;
  env.ffmpeg_handler = new DataHandlerFFmpeg(env.src, "0", image_path, 30, false);

  env.OpenHandler(device_type);
  env.ffmpeg_cpu_decoder = std::make_shared<FFmpegCpuDecoder>(*env.ffmpeg_handler);
  EXPECT_TRUE(env.ffmpeg_cpu_decoder->Create(env.st));
  env.ffmpeg_cpu_decoder->Destroy();

  // invalid
#if LIBAVFORMAT_VERSION_INT >= TEST_FFMPEG_VERSION_3_1
  env.st->codecpar->codec_id = AV_CODEC_ID_NONE;
#else
  env.st->codec->codec_id = AV_CODEC_ID_NONE;
#endif
  EXPECT_FALSE(env.ffmpeg_cpu_decoder->Create(env.st));
  env.ffmpeg_cpu_decoder->Destroy();
}

TEST(SourceCpuFFmpegDecoder, Process) {
  PrepareEnv env(1);

  EXPECT_TRUE(env.ffmpeg_cpu_decoder->Create(env.st));

  EXPECT_TRUE(env.ffmpeg_cpu_decoder->Process(env.av_pkt, false));
  // eos
  EXPECT_FALSE(env.ffmpeg_cpu_decoder->Process(env.av_pkt, true));
  env.ffmpeg_cpu_decoder->Destroy();
}

TEST(SourceCpuFFmpegDecoder, ProcessEmptyFrame) {
  PrepareEnv env(1);

  delete env.ffmpeg_handler;
  env.ffmpeg_handler = new DataHandlerFFmpeg(env.src, "", "", 30, false);
  env.ffmpeg_cpu_decoder = std::make_shared<FFmpegCpuDecoder>(*env.ffmpeg_handler);
  AVFrame *frame = nullptr;
  env.ffmpeg_cpu_decoder->ResetCount(3);
  EXPECT_FALSE(env.ffmpeg_cpu_decoder->ProcessFrame(frame));
  EXPECT_TRUE(env.ffmpeg_cpu_decoder->ProcessFrame(frame));
  EXPECT_TRUE(env.ffmpeg_cpu_decoder->ProcessFrame(frame));
  EXPECT_FALSE(env.ffmpeg_cpu_decoder->ProcessFrame(frame));

  env.ffmpeg_cpu_decoder = std::make_shared<FFmpegCpuDecoder>(*env.ffmpeg_handler);
  uint32_t loop_num = 5;
  // stream id is empty string, discard frame 5 times
  while (loop_num--) {
    EXPECT_FALSE(env.ffmpeg_cpu_decoder->ProcessFrame(frame));
  }
  env.ffmpeg_cpu_decoder->Destroy();
}

TEST(SourceCpuFFmpegDecoder, ProcessFrameInvalidContext) {
  PrepareEnv env(1);
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
  CNFrameInfo::Create("0", true);
}

/*
// Mlu Raw Decoder
TEST(SourceMluRawDecoder, CreateDestroy) {
  PrepareEnvRaw env;

  // h264
  EXPECT_TRUE(env.raw_mlu_decoder->Create(&env.decoder_ctx));
  env.raw_mlu_decoder->Destroy();

  // destroy twice, the function will return directly.
  env.raw_mlu_decoder->Destroy();

  // h265
  env.decoder_ctx.codec_id = DecoderContext::CN_CODEC_ID_HEVC;
  EXPECT_TRUE(env.raw_mlu_decoder->Create(&env.decoder_ctx));
  env.raw_mlu_decoder->Destroy();

  // jpeg
  delete env.raw_handler;

  std::string image_path = GetExePath() + gimage_path;
  env.raw_handler = new DataHandlerRaw(env.src, "0", image_path, 30, false);
  env.OpenHandler(0);
  env.raw_mlu_decoder = std::make_shared<RawMluDecoder>(*env.raw_handler);
  env.decoder_ctx.codec_id = DecoderContext::CN_CODEC_ID_JPEG;
  EXPECT_TRUE(env.raw_mlu_decoder->Create(&env.decoder_ctx));
  env.raw_mlu_decoder->Destroy();

  // unsupported
  env.decoder_ctx.codec_id = DecoderContext::CN_CODEC_ID_RAWVIDEO;
  EXPECT_FALSE(env.raw_mlu_decoder->Create(&env.decoder_ctx));
}
*/

/*
TEST(SourceMluRawDecoder, Process) {
  PrepareEnvRaw env;

  EXPECT_TRUE(env.raw_mlu_decoder->Create(&env.decoder_ctx));

  EXPECT_TRUE(env.raw_mlu_decoder->Process(env.raw_pkt, false));
  // eos
  EXPECT_TRUE(env.raw_mlu_decoder->Process(env.raw_pkt, true));
  env.raw_mlu_decoder->Destroy();
}
*/

TEST(SourceMluRawDecoder, ProcessEmptyFrame) {
  PrepareEnvRaw env;

  delete env.raw_handler;
  env.raw_handler = new DataHandlerRaw(env.src, "", "", 30, false);
  env.raw_mlu_decoder = std::make_shared<RawMluDecoder>(*env.raw_handler);
  edk::CnFrame frame;
  bool reused = false;
  uint32_t loop_num = 5;
  // stream id is empty string, discard frame 5 times
  while (loop_num--) {
    EXPECT_EQ(env.raw_mlu_decoder->ProcessFrame(frame, &reused), -1);
    EXPECT_FALSE(reused);
  }
  env.raw_mlu_decoder->Destroy();
}

TEST(SourceMluRawDecoder, ProcessFrame) {
  PrepareEnvRaw env;

  edk::CnFrame frame;
  frame.buf_id = 1;
  frame.height = 256;
  frame.width = 256;
  frame.pformat = edk::PixelFmt::NV21;
  frame.strides[0] = 256;
  frame.strides[1] = 256;
  void *mlu_ptr = nullptr;
  cnrtMalloc(&mlu_ptr, 256 * 256 * 3 / 2);
  frame.ptrs[0] = mlu_ptr;
  frame.ptrs[1] = reinterpret_cast<void *>(reinterpret_cast<char *>(mlu_ptr) + 256 * 256);
  bool reused = false;
  EXPECT_EQ(env.raw_mlu_decoder->ProcessFrame(frame, &reused), 0);
  EXPECT_FALSE(reused);

  env.raw_mlu_decoder->Destroy();

  // create eos frame for clear stream idx
  CNFrameInfo::Create("0", true);
  cnrtFree(mlu_ptr);
}

}  // namespace cnstream
