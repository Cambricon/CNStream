/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#include "video_encoder_ffmpeg.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include "cnstream_logging.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace cnstream {

namespace video {

static const char *pf_str[] = {"I420", "NV12", "NV21", "BGR", "RGB"};
static const char *ct_str[] = {"H264", "H265", "MPEG4", "JPEG"};

#define SPECIFIC_CODEC

#define VERSION_LAVC_ALLOC_PACKET AV_VERSION_INT(57, 20, 102)

struct EncodingInfo {
  int64_t pts, dts;
  int64_t start_tick, end_tick;
  void *user_data;
};

struct VideoEncoderFFmpegPrivate {
  std::thread thread;
  std::mutex input_mtx;
  std::condition_variable data_cv;
  std::condition_variable free_cv;
  std::queue<AVFrame *> data_q;
  std::queue<AVFrame *> free_q;
  std::list<AVFrame *> list;
  std::mutex info_mtx;
  std::unordered_map<int64_t, EncodingInfo> encoding_info;
  std::atomic<bool> eos_got{false};
  std::atomic<bool> eos_sent{false};
  std::atomic<bool> encoding{false};
  int64_t frame_count = 0;
  int64_t packet_count = 0;
  int64_t data_index = 0;
  uint32_t input_alignment = 32;

  ::AVPixelFormat pixel_format = AV_PIX_FMT_YUV420P;
  ::AVCodecID codec_id = AV_CODEC_ID_H264;
  AVCodecContext *codec_ctx = nullptr;
  AVCodec *codec = nullptr;
  AVDictionary *opts = nullptr;
  AVFrame *frame = nullptr;
  AVPacket *packet = nullptr;
  SwsContext *sws_ctx = nullptr;
};

static inline int64_t CurrentTick() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

VideoEncoderFFmpeg::VideoEncoderFFmpeg(const Param &param) : VideoEncoderBase(param) {
  LOGI(VideoEncoderFFmpeg) << "VideoEncoderFFmpeg(" << param.width << "x" << param.height << ", " <<
      pf_str[param_.pixel_format] << ", " << ct_str[param_.codec_type] << ")";
  avcodec_register_all();
  priv_.reset(new (std::nothrow) VideoEncoderFFmpegPrivate);
}

VideoEncoderFFmpeg::~VideoEncoderFFmpeg() {
  Stop();
  priv_.reset();
}

int VideoEncoderFFmpeg::Start() {
  WriteLockGuard slk(state_mtx_);
  if (state_ != IDLE) {
    LOGW(VideoEncoderFFmpeg) << "Start() state != IDLE";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  state_ = STARTING;

  int ret = 0;
  const char *codec_name;

  if (param_.input_buffer_count < 3) {
    LOGW(VideoEncoderFFmpeg) << "Start() input buffer count must no fewer than 3";
    param_.input_buffer_count = 3;
  }

  param_.width = param_.width % 2 ? param_.width - 1 : param_.width;
  param_.height = param_.height % 2 ? param_.height - 1 : param_.height;
  param_.frame_rate = param_.frame_rate > 0 ? param_.frame_rate : 30;
  param_.frame_rate = param_.frame_rate < 120 ? param_.frame_rate : 120;
  param_.time_base = param_.time_base > 0 ? param_.time_base : 1000;
  param_.bit_rate = param_.bit_rate < 0x40000 ? 0x40000 : param_.bit_rate;
  param_.gop_size = param_.gop_size < 8 ? 8 : param_.gop_size;

  switch (param_.pixel_format) {
    case VideoPixelFormat::I420:
      priv_->pixel_format = AV_PIX_FMT_YUV420P;
      break;
    case VideoPixelFormat::NV12:
      priv_->pixel_format = AV_PIX_FMT_NV12;
      break;
    case VideoPixelFormat::NV21:
      priv_->pixel_format = AV_PIX_FMT_NV21;
      break;
    default:
      LOGE(VideoEncoderFFmpeg) << "Start() unsupported pixel format: " << pf_str[param_.pixel_format];
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_PARAMETERS;
  }
  switch (param_.codec_type) {
    case VideoCodecType::AUTO:
    case VideoCodecType::H264:
      priv_->codec_id = AV_CODEC_ID_H264;
      codec_name = "libx264";
      break;
    case VideoCodecType::H265:
      priv_->codec_id = AV_CODEC_ID_HEVC;
      codec_name = "libx265";
      break;
    case VideoCodecType::MPEG4:
      priv_->codec_id = AV_CODEC_ID_MPEG4;
      codec_name = "mpeg4";
      break;
    case VideoCodecType::JPEG:
      priv_->codec_id = AV_CODEC_ID_MJPEG;
      codec_name = "mjpeg";
      break;
    default:
      LOGE(VideoEncoderFFmpeg) << "Start() unsupported codec type: " << ct_str[param_.codec_type];
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_PARAMETERS;
  }

#ifndef SPECIFIC_CODEC
  priv_->codec = avcodec_find_encoder(priv_->codec_id);
  codec_name = priv_->codec->name;
  LOGI(VideoEncoderFFmpeg) << "Start() avcodec_find_encoder: " << codec_name;
#else
  priv_->codec = avcodec_find_encoder_by_name(codec_name);
#endif
  if (!priv_->codec) {
    LOGE(VideoEncoderFFmpeg) << "Start() avcodec_find_encoder \"" << codec_name << "\" failed";
    Destroy();
    state_ = IDLE;
    return cnstream::VideoEncoder::ERROR_FAILED;
  }

  priv_->codec_ctx = avcodec_alloc_context3(priv_->codec);
  priv_->codec_ctx->codec_id = priv_->codec_id;
  priv_->codec_ctx->width = param_.width;
  priv_->codec_ctx->height = param_.height;
  priv_->codec_ctx->framerate = av_d2q(param_.frame_rate, 60000);
  priv_->codec_ctx->time_base.num = priv_->codec_ctx->framerate.den;
  priv_->codec_ctx->time_base.den = priv_->codec_ctx->framerate.num;
  priv_->codec_ctx->bit_rate = param_.bit_rate;
  priv_->codec_ctx->gop_size = param_.gop_size;
  priv_->codec_ctx->pix_fmt = priv_->codec_id == AV_CODEC_ID_MJPEG ? AV_PIX_FMT_YUVJ420P : AV_PIX_FMT_YUV420P;
  priv_->codec_ctx->max_b_frames = priv_->codec_id == AV_CODEC_ID_MJPEG ? 0 : 1;

  if (!strcmp(priv_->codec->name, "libx264") || !strcmp(priv_->codec->name, "libx265")) {
    av_dict_set(&priv_->opts, "preset", "superfast", 0);
    av_dict_set(&priv_->opts, "tune", "zerolatency", 0);
    if (priv_->codec_id == AV_CODEC_ID_H264) {
      av_dict_set(&priv_->opts, "profile", "high", 0);
      av_dict_set(&priv_->opts, "level", "5.1", 0);
    } else {
      av_dict_set(&priv_->opts, "level-idc", "5.1", 0);
      av_dict_set(&priv_->opts, "high-tier", "true", 0);
    }
  }
  ret = avcodec_open2(priv_->codec_ctx, priv_->codec, &priv_->opts);
  if (ret < 0) {
    LOGE(VideoEncoderFFmpeg) << "Start() avcodec_open2 failed, ret=" << ret;
    Destroy();
    state_ = IDLE;
    return cnstream::VideoEncoder::ERROR_FAILED;
  }

  if (priv_->pixel_format != priv_->codec_ctx->pix_fmt &&
      !(priv_->pixel_format == AV_PIX_FMT_YUV420P && priv_->codec_ctx->pix_fmt == AV_PIX_FMT_YUVJ420P)) {
    priv_->frame = av_frame_alloc();
    priv_->frame->width = priv_->codec_ctx->width;
    priv_->frame->height = priv_->codec_ctx->height;
    priv_->frame->format = priv_->codec_ctx->pix_fmt;
    ret = av_frame_get_buffer(priv_->frame, priv_->input_alignment);
    if (ret < 0) {
      LOGE(VideoEncoderFFmpeg) << "Start() av_frame_get_buffer failed, ret=" << ret;
      Destroy();
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
    priv_->sws_ctx = sws_getContext(priv_->frame->width, priv_->frame->height, priv_->pixel_format, priv_->frame->width,
                                    priv_->frame->height, static_cast<AVPixelFormat>(priv_->frame->format),
                                    SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!priv_->sws_ctx) {
      LOGE(VideoEncoderFFmpeg) << "Start() sws_getContext failed";
      Destroy();
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
  }
#if LIBAVCODEC_VERSION_INT < VERSION_LAVC_ALLOC_PACKET
  priv_->packet = reinterpret_cast<AVPacket *>(av_mallocz(sizeof(AVPacket)));
#else
  priv_->packet = av_packet_alloc();
#endif
  av_init_packet(priv_->packet);

  state_ = RUNNING;
  priv_->thread = std::thread(&VideoEncoderFFmpeg::Loop, this);
  return cnstream::VideoEncoder::SUCCESS;
}

int VideoEncoderFFmpeg::Stop() {
  UniqueWriteLock slk(state_mtx_);
  if (state_ != RUNNING) {
    // LOGW(VideoEncoderFFmpeg) << "Stop() state != RUNNING";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  std::unique_lock<std::mutex> lk(priv_->input_mtx);
  state_ = STOPPING;
  lk.unlock();
  slk.Unlock();

  priv_->free_cv.notify_all();
  priv_->data_cv.notify_all();
  if (priv_->thread.joinable()) priv_->thread.join();

  slk.Lock();
  lk.lock();
  AVFrame *frame;
  while (!priv_->data_q.empty()) {
    frame = priv_->data_q.front();
    av_frame_free(&frame);
    priv_->data_q.pop();
  }
  while (!priv_->free_q.empty()) {
    frame = priv_->free_q.front();
    av_frame_free(&frame);
    priv_->free_q.pop();
  }
  if (!priv_->list.empty()) {
    LOGW(VideoEncoderFFmpeg) << "Stop() " << priv_->list.size() << " frame buffers still outside";
    for (auto &frame : priv_->list) {
      av_frame_free(&frame);
    }
    priv_->list.clear();
  }

  Destroy();
  priv_->eos_got = priv_->eos_sent = false;
  state_ = IDLE;
  return cnstream::VideoEncoder::SUCCESS;
}

int VideoEncoderFFmpeg::RequestFrameBuffer(VideoFrame *frame, int timeout_ms) {
  ReadLockGuard slk(state_mtx_);
  if (state_ != RUNNING) {
    LOGW(VideoEncoderFFmpeg) << "RequestFrameBuffer() not running";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  if (priv_->eos_got) {
    LOGE(VideoEncoderFFmpeg) << "RequestFrameBuffer() EOS got already";
    return cnstream::VideoEncoder::ERROR_FAILED;
  }
  if (!frame) return cnstream::VideoEncoder::ERROR_PARAMETERS;

  AVFrame *avframe = nullptr;
  std::unique_lock<std::mutex> lk(priv_->input_mtx);
  if (!priv_->free_q.empty()) {
    avframe = priv_->free_q.front();
    priv_->free_q.pop();
  } else {
    uint32_t buffer_count = priv_->data_q.size() + (priv_->encoding ? 1 : 0);
    if (buffer_count >= param_.input_buffer_count) {
      if (timeout_ms == 0) {
        return cnstream::VideoEncoder::ERROR_FAILED;
      } else if (timeout_ms < 0) {
        priv_->free_cv.wait(lk, [this]() { return (state_ != RUNNING || !priv_->free_q.empty()); });
      } else {
        if (false == priv_->free_cv.wait_for(lk, std::chrono::milliseconds(timeout_ms),
                                             [this]() { return (state_ != RUNNING || !priv_->free_q.empty()); })) {
          LOGW(VideoEncoderFFmpeg) << "RequestFrameBuffer() wait for " << timeout_ms << " ms timeout";
          return cnstream::VideoEncoder::ERROR_TIMEOUT;
        }
      }
      if (state_ != RUNNING) return cnstream::VideoEncoder::ERROR_STATE;
      avframe = priv_->free_q.front();
      priv_->free_q.pop();
      // LOGI(VideoEncoderFFmpeg) << "RequestFrameBuffer() use allocated frame";
    } else {
      avframe = av_frame_alloc();
      avframe->width = param_.width;
      avframe->height = param_.height;
      avframe->format = priv_->pixel_format;
      int ret = av_frame_get_buffer(avframe, priv_->input_alignment);
      if (ret < 0) {
        LOGE(VideoEncoderFFmpeg) << "RequestFrameBuffer() av_frame_get_buffer failed, ret=" << ret;
        av_frame_free(&avframe);
        return cnstream::VideoEncoder::ERROR_FAILED;
      }
      // LOGI(VideoEncoderFFmpeg) << "RequestFrameBuffer() alloc new frame(" << total_buffer_count << ")";
    }
  }

  frame->width        = avframe->width;
  frame->height       = avframe->height;
  frame->data[0]      = avframe->data[0];
  frame->stride[0]    = avframe->linesize[0];
  frame->data[1]      = avframe->data[1];
  frame->stride[1]    = avframe->linesize[1];
  if (param_.pixel_format == VideoPixelFormat::I420) {
    frame->data[2]    = avframe->data[2];
    frame->stride[2]  = avframe->linesize[2];
  }
  frame->pixel_format = param_.pixel_format;

  priv_->list.push_back(avframe);
  return cnstream::VideoEncoder::SUCCESS;
}

int VideoEncoderFFmpeg::SendFrame(const VideoFrame *frame, int timeout_ms) {
  ReadLockGuard slk(state_mtx_);
  if (state_ != RUNNING) {
    LOGW(VideoEncoderFFmpeg) << "SendFrame() not running";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  if (priv_->eos_got) {
    LOGE(VideoEncoderFFmpeg) << "SendFrame() EOS got already";
    return cnstream::VideoEncoder::ERROR_FAILED;
  }
  if (!frame) return cnstream::VideoEncoder::ERROR_PARAMETERS;

  AVFrame *avframe = nullptr;
  std::unique_lock<std::mutex> lk(priv_->input_mtx);
  if (frame->HasEOS()) {
    LOGI(VideoEncoderFFmpeg) << "SendFrame() Send EOS";
    priv_->eos_got = true;
    if (!frame->data[0]) {
      lk.unlock();
      priv_->data_cv.notify_one();
      return cnstream::VideoEncoder::SUCCESS;
    }
  } else if (!frame->data[0]) {
    LOGE(VideoEncoderFFmpeg) << "SendFrame() Bad frame data pointer";
    return cnstream::VideoEncoder::ERROR_PARAMETERS;
  }

  if (!priv_->list.empty()) {
    auto av_frame =
        std::find_if(priv_->list.begin(), priv_->list.end(), [frame, this](const AVFrame *f) {
          return ((param_.pixel_format == VideoPixelFormat::I420 && frame->data[0] == f->data[0] &&
                   frame->data[1] == f->data[1] && frame->data[2] == f->data[2]) ||
                 ((param_.pixel_format == VideoPixelFormat::NV12 || param_.pixel_format == VideoPixelFormat::NV21) &&
                   frame->data[0] == f->data[0] && frame->data[1] == f->data[1]));
        });
    if (av_frame != priv_->list.end()) {
      avframe = *av_frame;
      priv_->list.erase(av_frame);
    }
  }
  if (!avframe) {
    if (!priv_->free_q.empty()) {
      avframe = priv_->free_q.front();
      priv_->free_q.pop();
    } else {
      uint32_t total_buffer_count = priv_->data_q.size() + (priv_->encoding ? 1 : 0);
      if (total_buffer_count >= param_.input_buffer_count) {
        if (timeout_ms == 0) {
          return cnstream::VideoEncoder::ERROR_FAILED;
        } else if (timeout_ms < 0) {
          priv_->free_cv.wait(lk, [this]() { return (state_ != RUNNING || !priv_->free_q.empty()); });
        } else {
          if (false == priv_->free_cv.wait_for(lk, std::chrono::milliseconds(timeout_ms),
                                               [this]() { return (state_ != RUNNING || !priv_->free_q.empty()); })) {
            LOGW(VideoEncoderFFmpeg) << "SendFrame() wait for " << timeout_ms << " ms timeout";
            return cnstream::VideoEncoder::ERROR_TIMEOUT;
          }
        }
        if (state_ != RUNNING) return cnstream::VideoEncoder::ERROR_STATE;
        avframe = priv_->free_q.front();
        priv_->free_q.pop();
        // LOGI(VideoEncoderFFmpeg) << "SendFrame() use allocated frame";
      } else {
        avframe = av_frame_alloc();
        avframe->width = frame->width;
        avframe->height = frame->height;
        avframe->format = priv_->pixel_format;
        int ret = av_frame_get_buffer(avframe, priv_->input_alignment);
        if (ret < 0) {
          LOGE(VideoEncoderFFmpeg) << "SendFrame() av_frame_get_buffer failed, ret=" << ret;
          av_frame_free(&avframe);
          return cnstream::VideoEncoder::ERROR_FAILED;
        }
        // LOGI(VideoEncoderFFmpeg) << "SendFrame() alloc new frame(" << total_buffer_count << ")";
      }
    }

    const uint8_t *data[4] = { frame->data[0], frame->data[1], frame->data[2], nullptr };
    const int linesizes[4] = { static_cast<int>(frame->stride[0]), static_cast<int>(frame->stride[1]),
                               static_cast<int>(frame->stride[2]), 0 };
    av_image_copy(avframe->data, avframe->linesize, data, linesizes, priv_->pixel_format, avframe->width,
                  avframe->height);
  }

  avframe->pts = (frame->pts == INVALID_TIMESTAMP ? AV_NOPTS_VALUE : frame->pts);
  avframe->pkt_pts = avframe->pts;
  avframe->pkt_dts = (frame->dts == INVALID_TIMESTAMP ? AV_NOPTS_VALUE : frame->dts);
  avframe->opaque = frame->user_data;
  priv_->data_q.push(avframe);
  lk.unlock();
  priv_->data_cv.notify_one();

  LOGT(VideoEncoderFFmpeg) << "SendFrame() pts=" << frame->pts << ", dts=" << frame->dts;
  return cnstream::VideoEncoder::SUCCESS;
}

int VideoEncoderFFmpeg::GetPacket(VideoPacket *packet, PacketInfo *info) {
  ReadLockGuard slk(state_mtx_);
  if (state_ != RUNNING) {
    LOGW(VideoEncoderFFmpeg) << "GetPacket() not running";
    return cnstream::VideoEncoder::ERROR_STATE;
  }

  return VideoEncoderBase::GetPacket(packet, info);
}

bool VideoEncoderFFmpeg::GetPacketInfo(int64_t index, PacketInfo *info) {
  if (!info) return false;

  std::lock_guard<std::mutex> lk(priv_->info_mtx);
  if (priv_->encoding_info.count(index) == 0) {
    LOGE(VideoEncoderFFmpeg) << "GetPacketInfo() find index: " << index << " failed";
    return false;
  }
  auto &enc_info = priv_->encoding_info[index];
  info->start_tick = enc_info.start_tick;
  info->end_tick = enc_info.end_tick;
  priv_->encoding_info.erase(index);
  return true;
}

void VideoEncoderFFmpeg::Destroy() {
  if (priv_->codec_ctx) {
    avcodec_close(priv_->codec_ctx);
    priv_->codec_ctx = nullptr;
  }
  if (priv_->opts) {
    av_dict_free(&priv_->opts);
    priv_->opts = nullptr;
  }
  if (priv_->sws_ctx) {
    sws_freeContext(priv_->sws_ctx);
    priv_->sws_ctx = nullptr;
  }
  if (priv_->frame) {
    av_frame_free(&priv_->frame);
    priv_->frame = nullptr;
  }
  if (priv_->packet) {
    av_packet_unref(priv_->packet);
    av_free(priv_->packet);
    priv_->packet = nullptr;
  }
}

void VideoEncoderFFmpeg::Loop() {
  int ret;
  AVFrame *frame = nullptr;

  while (state_ == RUNNING) {
    std::unique_lock<std::mutex> lk(priv_->input_mtx);
    priv_->data_cv.wait(
        lk, [this]() { return (state_ != RUNNING || !priv_->data_q.empty() || (priv_->eos_got && !priv_->eos_sent)); });
    if (state_ != RUNNING) break;

    if (!priv_->data_q.empty()) {
      frame = priv_->data_q.front();
      priv_->data_q.pop();
      priv_->encoding = true;
      lk.unlock();
      // Color convertion
      if (priv_->sws_ctx) {
        ret = sws_scale(priv_->sws_ctx, frame->data, frame->linesize, 0, frame->height, priv_->frame->data,
                        priv_->frame->linesize);
        if (ret < 0) {
          LOGE(VideoEncoderFFmpeg) << "Loop() sws_scale failed, ret=" << ret;
          lk.lock();
          priv_->free_q.push(frame);
          priv_->encoding = false;
          lk.unlock();
          priv_->free_cv.notify_one();
          continue;
        }
        priv_->frame->pts = frame->pts;
        priv_->frame->pkt_pts = frame->pkt_pts;
        priv_->frame->pkt_dts = frame->pkt_dts;
        lk.lock();
        priv_->free_q.push(frame);
        priv_->encoding = false;
        lk.unlock();
        priv_->free_cv.notify_one();
        frame = priv_->frame;
      }
    } else {
      if (priv_->eos_sent) break;
      frame = nullptr;
      lk.unlock();
    }

    if (frame) {
      std::lock_guard<std::mutex> lk(priv_->info_mtx);
      if (frame->pts == AV_NOPTS_VALUE) {
        frame->pts = priv_->frame_count * param_.time_base / param_.frame_rate;
        frame->pkt_pts = frame->pts;
      }

      priv_->encoding_info[priv_->data_index].pts = frame->pkt_pts;
      priv_->encoding_info[priv_->data_index].dts = frame->pkt_dts;
      priv_->encoding_info[priv_->data_index].start_tick = CurrentTick();
      priv_->encoding_info[priv_->data_index].end_tick = 0;
      priv_->encoding_info[priv_->data_index].user_data = frame->opaque;

      frame->pts = priv_->data_index++;
      frame->pkt_pts = frame->pts;
      priv_->frame_count++;
    }

    do {
      int got_packet = 0;
      ret = avcodec_encode_video2(priv_->codec_ctx, priv_->packet, frame, &got_packet);
      if (ret < 0) {
        LOGE(VideoEncoderFFmpeg) << "Loop() avcodec_encode_video2 failed, ret=" << ret;
        break;
      }
      if (!priv_->sws_ctx && frame != nullptr) {
        lk.lock();
        priv_->free_q.push(frame);
        priv_->encoding = false;
        lk.unlock();
        priv_->free_cv.notify_one();
      }
      void *user_data = nullptr;
      if (!ret && got_packet && priv_->packet->size) {
        // find out packet and update encoding info
        std::unique_lock<std::mutex> lk(priv_->info_mtx);
        int64_t index = priv_->packet->pts;
        auto info = priv_->encoding_info.find(index);
        if (info != priv_->encoding_info.end()) {
          info->second.end_tick = CurrentTick();
          priv_->packet->pts = info->second.pts;
          if (info->second.dts == AV_NOPTS_VALUE) {
            priv_->packet->dts = (priv_->packet_count - 2) * param_.time_base / param_.frame_rate;
          } else {
            priv_->packet->dts = info->second.dts;
          }
          user_data = info->second.user_data;
        } else {
          LOGE(VideoEncoderFFmpeg) << "Loop() restore encoding info failed, index=" << index;
          return;
        }
        lk.unlock();
        LOGT(VideoEncoderFFmpeg) << "Loop() got packet: size=" << priv_->packet->size << ", pts=" << priv_->packet->pts
                                 << ", dts=" << priv_->packet->dts << ", user_data=" << user_data
                                 << ((priv_->packet->flags & AV_PKT_FLAG_KEY) ? " [K]" : "");
        VideoPacket packet;
        memset(&packet, 0, sizeof(VideoPacket));
        packet.data = priv_->packet->data;
        packet.size = priv_->packet->size;
        packet.pts = priv_->packet->pts;
        packet.dts = priv_->packet->dts;
        packet.user_data = user_data;
        if (priv_->packet->flags & AV_PKT_FLAG_KEY) packet.SetKey();
        IndexedVideoPacket vpacket;
        vpacket.packet = packet;
        vpacket.index = index;
        PushBuffer(&vpacket);
        priv_->packet_count++;
        av_packet_unref(priv_->packet);
        std::lock_guard<std::mutex> cblk(cb_mtx_);
        if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_DATA);
      }
      std::unique_lock<std::mutex> lk(priv_->input_mtx);
      if (!priv_->data_q.empty() || !priv_->eos_got) {
        break;
      } else if (ret != 0 || !got_packet) {
        if (priv_->eos_sent) break;
        priv_->eos_sent = true;
        lk.unlock();
        std::lock_guard<std::mutex> cblk(cb_mtx_);
        LOGI(VideoEncoderFFmpeg) << "Loop() Callback(EVENT_EOS)";
        if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_EOS);
        break;
      }
      frame = nullptr;
    } while (true);
  }
}

}  // namespace video

}  // namespace cnstream
