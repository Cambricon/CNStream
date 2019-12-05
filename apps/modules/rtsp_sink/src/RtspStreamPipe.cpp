#include "RtspStreamPipe.h"
#include <iostream>
#include <thread>
#include "CNVideoEncoder.h"
#include "FFmpegVideoEncoder.h"
#include "LiveRTSPServer.h"
#include "NanoTimer.h"
#include "VideoEncoder.h"
struct StreamPipeCtx {
  VideoEncoder *video_encoder_ = nullptr;
  RTSPStreaming::LiveRTSPServer *live_server_ = nullptr;
  std::thread *server_thread_ = nullptr;
  bool init_flag = false;
};

static void RunServer(void *server) { ((RTSPStreaming::LiveRTSPServer *)server)->Run(); }

StreamPipeCtx *StreamPipeCreate(StreamContext *ctx) {
  StreamPipeCtx *pipeCtx = new StreamPipeCtx;
  int width = ctx->width_out;
  int height = ctx->height_out;
  VideoEncoder::PictureFormat format;
  switch (ctx->format) {
    case ColorFormat_YUV420:
      format = VideoEncoder::YUV420P;
      break;
    case ColorFormat_BGR24:
      format = VideoEncoder::BGR24;
      break;
    default:
      format = VideoEncoder::BGR24;
      break;
  }
  VideoEncoder::CodecType codec;
  switch (ctx->codec) {
    case VideoCodec_H264:
      codec = VideoEncoder::H264;
      break;
    case VideoCodec_HEVC:
      codec = VideoEncoder::HEVC;
      break;
    default:
      codec = VideoEncoder::H264;
      break;
  }
  int gop = ctx->gop;
  int bps = ctx->kbps * 1000;
  int fps = ctx->fps;
  if (ctx->hw == FFMPEG) {
    std::cout << "Use FFMPEG encoder" << std::endl;
    pipeCtx->video_encoder_ = new FFmpegVideoEncoder(width, height, format, codec, fps, gop, bps);
  } else {
    std::cout << "Use MLU encoder" << std::endl;
    pipeCtx->video_encoder_ = new CNVideoEncoder(width, height, format, codec, fps, gop, bps);
  }
  int udp_port = ctx->udp_port;
  int http_port = ctx->http_port;
  pipeCtx->live_server_ = new RTSPStreaming::LiveRTSPServer(pipeCtx->video_encoder_, udp_port, http_port);
  pipeCtx->server_thread_ = new std::thread(RunServer, pipeCtx->live_server_);
  pipeCtx->video_encoder_->Start();
  pipeCtx->init_flag = true;
  return pipeCtx;
}
int StreamPipePutPacket(StreamPipeCtx *ctx, uint8_t *data, int64_t timestamp) {
  if (!ctx->init_flag) {
    std::cout << "Init stream pipe firstly\n" << std::endl;
    return -1;
  }
  NanoTimer timer;
  timer.Start();
  ctx->video_encoder_->SendFrame(data, timestamp);
  // std::cout<<"Put Packet need: "<<timer.GetElapsed_ms()<<" ms"<<std::endl;
  return 0;
}
int StreamPipeClose(StreamPipeCtx *ctx) {
  if (!ctx->init_flag) {
    std::cout << "Init stream pipe firstly\n" << std::endl;
    return -1;
  }
  if (ctx->live_server_) {
    ctx->live_server_->SignalExit();
  }

  // std::cout<<"live server release"<<std::endl;
  if (ctx->video_encoder_) {
    ctx->video_encoder_->Stop();
    delete ctx->video_encoder_;
    ctx->video_encoder_ = nullptr;
  }

  if (ctx->server_thread_ && ctx->server_thread_->joinable()) {
    ctx->server_thread_->join();
    delete ctx->server_thread_;
    ctx->server_thread_ = nullptr;
  }
  std::cout << "Stream Pipe close" << std::endl;
  delete ctx;
  return 0;
}
