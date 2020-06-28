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
#include "rtsp_stream_pipe.hpp"

#include <glog/logging.h>
#include <thread>

#include "cn_video_encoder.hpp"
#include "ffmpeg_video_encoder.hpp"
#include "live_rtsp_server.hpp"
#include "rtsp_sink.hpp"
#include "util/cnstream_time_utility.hpp"
#include "video_encoder.hpp"

namespace cnstream {

struct StreamPipeCtx {
  VideoEncoder *video_encoder = nullptr;
  RtspStreaming::LiveRtspServer *live_server = nullptr;
  std::thread *server_thread = nullptr;
  bool init_flag = false;
};

static void RunServer(void *server) { ((RtspStreaming::LiveRtspServer *)server)->Run(); }

StreamPipeCtx *StreamPipeCreate(const RtspParam &rtsp_param) {
  StreamPipeCtx *pipe_ctx = new StreamPipeCtx;
  if (rtsp_param.enc_type == FFMPEG) {
    pipe_ctx->video_encoder = new FFmpegVideoEncoder(rtsp_param);
  } else {
    pipe_ctx->video_encoder = new CNVideoEncoder(rtsp_param);
  }

  pipe_ctx->live_server =
      new RtspStreaming::LiveRtspServer(pipe_ctx->video_encoder, rtsp_param.udp_port, rtsp_param.http_port);
  pipe_ctx->server_thread = new std::thread(RunServer, pipe_ctx->live_server);
  pipe_ctx->video_encoder->Start();
  pipe_ctx->init_flag = true;
  return pipe_ctx;
}

int StreamPipePutPacket(StreamPipeCtx *ctx, uint8_t *data, int64_t timestamp) {
  if (!ctx->init_flag) {
    LOG(INFO) << "Init stream pipe firstly\n";
    return -1;
  }
  ctx->video_encoder->SendFrame(data, timestamp);
  return 0;
}

/*
int StreamPipePutPacketMlu(StreamPipeCtx *ctx, void *y, void *uv, int64_t timestamp) {
  if (!ctx->init_flag) {
    LOG(INFO) << "Init stream pipe firstly!";
    return -1;
  }
  ctx->video_encoder->SendFrame(y, uv, timestamp);
  return 0;
}
*/

int StreamPipeClose(StreamPipeCtx *ctx) {
  if (!ctx->init_flag) {
    LOG(INFO) << "Init stream pipe firstly\n";
    return -1;
  }
  if (ctx->live_server) {
    ctx->live_server->SignalExit();
  }

  if (ctx->server_thread && ctx->server_thread->joinable()) {
    ctx->server_thread->join();
    delete ctx->server_thread;
    ctx->server_thread = nullptr;
  }

  LOG(INFO) << "live server release";
  if (ctx->video_encoder) {
    ctx->video_encoder->Stop();
    delete ctx->video_encoder;
    ctx->video_encoder = nullptr;
  }

  LOG(INFO) << "Stream Pipe close";
  delete ctx;
  return 0;
}

}  // namespace cnstream
