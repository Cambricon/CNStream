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

#include <algorithm>
#include <chrono>

#include "cnstream_logging.hpp"

#include "video_encoder_ffmpeg.hpp"

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

static inline int64_t CurrentTick() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

VideoEncoderFFmpeg::VideoEncoderFFmpeg(const Param &param) : VideoEncoderBase(param) {
  LOGI(VideoEncoderFFmpeg) << "VideoEncoderFFmpeg(" << param.width << "x" << param.height << ", " <<
      pf_str[param_.pixel_format] << ", " << ct_str[param_.codec_type] << ")";
  avcodec_register_all();
}

VideoEncoderFFmpeg::~VideoEncoderFFmpeg() { Stop(); }

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
  param_.frame_rate = param_.frame_rate > 0 ? param_.frame_rate : 30;
  param_.frame_rate = param_.frame_rate < 120 ? param_.frame_rate : 120;
  param_.time_base = param_.time_base > 0 ? param_.time_base : 1000;
  param_.bit_rate = param_.bit_rate < 0x40000 ? 0x40000 : param_.bit_rate;
  param_.gop_size = param_.gop_size < 8 ? 8 : param_.gop_size;

  switch (param_.pixel_format) {
    case VideoPixelFormat::I420:
      pixel_format_ = AV_PIX_FMT_YUV420P;
      break;
    case VideoPixelFormat::NV12:
      pixel_format_ = AV_PIX_FMT_NV12;
      break;
    case VideoPixelFormat::NV21:
      pixel_format_ = AV_PIX_FMT_NV21;
      break;
    default:
      LOGE(VideoEncoderFFmpeg) << "Start() unsupported pixel format: " << pf_str[param_.pixel_format];
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_PARAMETERS;
  }
  switch (param_.codec_type) {
    case VideoCodecType::AUTO:
    case VideoCodecType::H264:
      codec_id_ = AV_CODEC_ID_H264; codec_name = "libx264";
      break;
    case VideoCodecType::H265:
      codec_id_ = AV_CODEC_ID_HEVC; codec_name = "libx265";
      break;
    case VideoCodecType::MPEG4:
      codec_id_ = AV_CODEC_ID_MPEG4; codec_name = "mpeg4";
      break;
    case VideoCodecType::JPEG:
      codec_id_ = AV_CODEC_ID_MJPEG; codec_name = "mjpeg";
      break;
    default:
      LOGE(VideoEncoderFFmpeg) << "Start() unsupported codec type: " << ct_str[param_.codec_type];
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_PARAMETERS;
  }

#ifndef SPECIFIC_CODEC
  codec_ = avcodec_find_encoder(codec_id_);
  codec_name = codec_->name;
  LOGI(VideoEncoderFFmpeg) << "Start() avcodec_find_encoder: " << codec_name;
#else
  codec_ = avcodec_find_encoder_by_name(codec_name);
#endif
  if (!codec_) {
    LOGE(VideoEncoderFFmpeg) << "Start() avcodec_find_encoder \"" << codec_name << "\" failed";
    Destroy();
    state_ = IDLE;
    return cnstream::VideoEncoder::ERROR_FAILED;
  }

  codec_ctx_ = avcodec_alloc_context3(codec_);
  codec_ctx_->codec_id = codec_id_;
  codec_ctx_->width = param_.width;
  codec_ctx_->height = param_.height;
  codec_ctx_->framerate = av_d2q(param_.frame_rate, 60000);
  codec_ctx_->time_base.num = codec_ctx_->framerate.den;
  codec_ctx_->time_base.den = codec_ctx_->framerate.num;
  codec_ctx_->bit_rate = param_.bit_rate;
  codec_ctx_->gop_size = param_.gop_size;
  codec_ctx_->pix_fmt = codec_id_ == AV_CODEC_ID_MJPEG ? AV_PIX_FMT_YUVJ420P : AV_PIX_FMT_YUV420P;
  codec_ctx_->max_b_frames = codec_id_ == AV_CODEC_ID_MJPEG ? 0 : 1;

  if (!strcmp(codec_->name, "libx264") || !strcmp(codec_->name, "libx265")) {
    av_dict_set(&opts_, "preset", "superfast", 0);
    av_dict_set(&opts_, "tune", "zerolatency", 0);
    if (codec_id_ == AV_CODEC_ID_H264) {
      av_dict_set(&opts_, "profile", "high", 0);
      av_dict_set(&opts_, "level", "5.1", 0);
    } else {
      av_dict_set(&opts_, "level-idc", "5.1", 0);
      av_dict_set(&opts_, "high-tier", "true", 0);
    }
  }
  ret = avcodec_open2(codec_ctx_, codec_, &opts_);
  if (ret < 0) {
    LOGE(VideoEncoderFFmpeg) << "Start() avcodec_open2 failed, ret=" << ret;
    Destroy();
    state_ = IDLE;
    return cnstream::VideoEncoder::ERROR_FAILED;
  }

  if (pixel_format_ != codec_ctx_->pix_fmt &&
      !(pixel_format_ == AV_PIX_FMT_YUV420P && codec_ctx_->pix_fmt == AV_PIX_FMT_YUVJ420P)) {
    frame_ = av_frame_alloc();
    frame_->width = codec_ctx_->width;
    frame_->height = codec_ctx_->height;
    frame_->format = codec_ctx_->pix_fmt;
    ret = av_frame_get_buffer(frame_, input_alignment_);
    if (ret < 0) {
      LOGE(VideoEncoderFFmpeg) << "Start() av_frame_get_buffer failed, ret=" << ret;
      Destroy();
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
    sws_ctx_ = sws_getContext(frame_->width, frame_->height, pixel_format_,
                              frame_->width, frame_->height, static_cast<AVPixelFormat>(frame_->format),
                              SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx_) {
      LOGE(VideoEncoderFFmpeg) << "Start() sws_getContext failed";
      Destroy();
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
  }
#if LIBAVCODEC_VERSION_INT < VERSION_LAVC_ALLOC_PACKET
    packet_ = reinterpret_cast<AVPacket *>(av_mallocz(sizeof(AVPacket)));
#else
    packet_ = av_packet_alloc();
#endif
  av_init_packet(packet_);

  state_ = RUNNING;
  thread_ = std::thread(&VideoEncoderFFmpeg::Loop, this);
  return cnstream::VideoEncoder::SUCCESS;
}

int VideoEncoderFFmpeg::Stop() {
  UniqueWriteLock slk(state_mtx_);
  if (state_ != RUNNING) {
    // LOGW(VideoEncoderFFmpeg) << "Stop() state != RUNNING";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  std::unique_lock<std::mutex> lk(input_mtx_);
  state_ = STOPPING;
  lk.unlock();
  slk.Unlock();

  free_cv_.notify_all();
  data_cv_.notify_all();
  if (thread_.joinable()) thread_.join();

  slk.Lock();
  lk.lock();
  AVFrame *frame;
  while (!data_q_.empty()) {
    frame = data_q_.front();
    av_frame_free(&frame);
    data_q_.pop();
  }
  while (!free_q_.empty()) {
    frame = free_q_.front();
    av_frame_free(&frame);
    free_q_.pop();
  }
  if (!list_.empty()) {
    LOGW(VideoEncoderFFmpeg) << "Stop() " << list_.size() << " frame buffers still outside";
    for (auto &frame : list_) {
      av_frame_free(&frame);
    }
    list_.clear();
  }

  Destroy();
  eos_got_ = eos_sent_ = false;
  state_ = IDLE;
  return cnstream::VideoEncoder::SUCCESS;
}

int VideoEncoderFFmpeg::RequestFrameBuffer(VideoFrame *frame, int timeout_ms) {
  ReadLockGuard slk(state_mtx_);
  if (state_ != RUNNING) {
    LOGW(VideoEncoderFFmpeg) << "RequestFrameBuffer() not running";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  if (eos_got_) {
    LOGE(VideoEncoderFFmpeg) << "RequestFrameBuffer() EOS got already";
    return cnstream::VideoEncoder::ERROR_FAILED;
  }
  if (!frame) return cnstream::VideoEncoder::ERROR_PARAMETERS;

  AVFrame *avframe = nullptr;
  std::unique_lock<std::mutex> lk(input_mtx_);
  if (!free_q_.empty()) {
    avframe = free_q_.front();
    free_q_.pop();
  } else {
    uint32_t buffer_count = data_q_.size() + (encoding_ ? 1 : 0);
    if (buffer_count >= param_.input_buffer_count) {
      if (timeout_ms == 0) {
        return cnstream::VideoEncoder::ERROR_FAILED;
      } else if (timeout_ms < 0) {
        free_cv_.wait(lk, [this] () { return (state_ != RUNNING || !free_q_.empty()); });
      } else {
        if (false == free_cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms),
            [this] () { return (state_ != RUNNING || !free_q_.empty()); })) {
          LOGW(VideoEncoderFFmpeg) << "RequestFrameBuffer() wait for " << timeout_ms << " ms timeout";
          return cnstream::VideoEncoder::ERROR_TIMEOUT;
        }
      }
      if (state_ != RUNNING) return cnstream::VideoEncoder::ERROR_STATE;
      avframe = free_q_.front();
      free_q_.pop();
      // LOGI(VideoEncoderFFmpeg) << "RequestFrameBuffer() use allocated frame";
    } else {
      avframe = av_frame_alloc();
      avframe->width = param_.width;
      avframe->height = param_.height;
      avframe->format = pixel_format_;
      int ret = av_frame_get_buffer(avframe, input_alignment_);
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

  list_.push_back(avframe);
  return cnstream::VideoEncoder::SUCCESS;
}

int VideoEncoderFFmpeg::SendFrame(const VideoFrame *frame, int timeout_ms) {
  ReadLockGuard slk(state_mtx_);
  if (state_ != RUNNING) {
    LOGW(VideoEncoderFFmpeg) << "SendFrame() not running";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  if (eos_got_) {
    LOGE(VideoEncoderFFmpeg) << "SendFrame() EOS got already";
    return cnstream::VideoEncoder::ERROR_FAILED;
  }
  if (!frame) return cnstream::VideoEncoder::ERROR_PARAMETERS;

  AVFrame *avframe = nullptr;
  std::unique_lock<std::mutex> lk(input_mtx_);
  if (frame->HasEOS()) {
    LOGI(VideoEncoderFFmpeg) << "SendFrame() Send EOS";
    eos_got_ = true;
    if (!frame->data[0]) {
      lk.unlock();
      data_cv_.notify_one();
      return cnstream::VideoEncoder::SUCCESS;
    }
  } else if (!frame->data[0]) {
    LOGE(VideoEncoderFFmpeg) << "SendFrame() Bad frame data pointer";
    return cnstream::VideoEncoder::ERROR_PARAMETERS;
  }

  if (!list_.empty()) {
    auto av_frame = std::find_if(list_.begin(), list_.end(),
        [frame, this] (const AVFrame *f) {
          return ((param_.pixel_format == VideoPixelFormat::I420 && frame->data[0] == f->data[0] &&
                   frame->data[1] == f->data[1] && frame->data[2] == f->data[2]) ||
                 ((param_.pixel_format == VideoPixelFormat::NV12 || param_.pixel_format == VideoPixelFormat::NV21) &&
                   frame->data[0] == f->data[0] && frame->data[1] == f->data[1]));
        });
    if (av_frame != list_.end()) {
      avframe = *av_frame;
      list_.erase(av_frame);
    }
  }
  if (!avframe) {
    if (!free_q_.empty()) {
      avframe = free_q_.front();
      free_q_.pop();
    } else {
      uint32_t total_buffer_count = data_q_.size() + (encoding_ ? 1 : 0);
      if (total_buffer_count >= param_.input_buffer_count) {
        if (timeout_ms == 0) {
          return cnstream::VideoEncoder::ERROR_FAILED;
        } else if (timeout_ms < 0) {
          free_cv_.wait(lk, [this] () { return (state_ != RUNNING || !free_q_.empty()); });
        } else {
          if (false == free_cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms),
              [this] () { return (state_ != RUNNING || !free_q_.empty()); })) {
            LOGW(VideoEncoderFFmpeg) << "SendFrame() wait for " << timeout_ms << " ms timeout";
            return cnstream::VideoEncoder::ERROR_TIMEOUT;
          }
        }
        if (state_ != RUNNING) return cnstream::VideoEncoder::ERROR_STATE;
        avframe = free_q_.front();
        free_q_.pop();
        // LOGI(VideoEncoderFFmpeg) << "SendFrame() use allocated frame";
      } else {
        avframe = av_frame_alloc();
        avframe->width = frame->width;
        avframe->height = frame->height;
        avframe->format = pixel_format_;
        int ret = av_frame_get_buffer(avframe, input_alignment_);
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
    av_image_copy(avframe->data, avframe->linesize, data, linesizes, pixel_format_, avframe->width, avframe->height);
  }

  avframe->pts = (frame->pts == INVALID_TIMESTAMP ? AV_NOPTS_VALUE : frame->pts);
  avframe->pkt_pts = avframe->pts;
  avframe->pkt_dts = (frame->dts == INVALID_TIMESTAMP ? AV_NOPTS_VALUE : frame->dts);
  data_q_.push(avframe);
  lk.unlock();
  data_cv_.notify_one();

  // LOGI(VideoEncoderFFmpeg) << "SendFrame() pts=" << frame->pts << ", dts=" << frame->dts ;
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

bool VideoEncoderFFmpeg::GetPacketInfo(int64_t pts, PacketInfo *info) {
  if (!info) return false;

  std::lock_guard<std::mutex> lk(info_mtx_);
  for (auto info_it = encoding_info_.begin(); info_it != encoding_info_.end(); ++info_it) {
    if (info_it->second.pts == pts) {
      info->start_tick = info_it->second.start_tick;
      info->end_tick   = info_it->second.end_tick;
      encoding_info_.erase(info_it);
      return true;
    }
  }
  return false;
}

void VideoEncoderFFmpeg::Destroy() {
  if (codec_ctx_) {
    avcodec_close(codec_ctx_);
    codec_ctx_ = nullptr;
  }
  if (opts_) {
    av_dict_free(&opts_);
    opts_ = nullptr;
  }
  if (sws_ctx_) {
    sws_freeContext(sws_ctx_);
    sws_ctx_ = nullptr;
  }
  if (frame_) {
    av_frame_free(&frame_);
    frame_ = nullptr;
  }
  if (packet_) {
    av_packet_unref(packet_);
    av_free(packet_);
    packet_ = nullptr;
  }
}

void VideoEncoderFFmpeg::Loop() {
  int ret;
  AVFrame *frame = nullptr;

  while (state_ == RUNNING) {
    std::unique_lock<std::mutex> lk(input_mtx_);
    data_cv_.wait(lk, [this] () { return (state_ != RUNNING || !data_q_.empty() || (eos_got_ && !eos_sent_)); });
    if (state_ != RUNNING) break;

    if (!data_q_.empty()) {
      frame = data_q_.front();
      data_q_.pop();
      encoding_ = true;
      lk.unlock();
      // Color convertion
      if (sws_ctx_) {
        ret = sws_scale(sws_ctx_, frame->data, frame->linesize, 0, frame->height, frame_->data, frame_->linesize);
        if (ret < 0) {
          LOGE(VideoEncoderFFmpeg) << "Loop() sws_scale failed, ret=" << ret;
          lk.lock(); free_q_.push(frame); encoding_ = false; lk.unlock();
          free_cv_.notify_one();
          continue;
        }
        frame_->pts = frame->pts;
        frame_->pkt_pts = frame->pkt_pts;
        frame_->pkt_dts = frame->pkt_dts;
        lk.lock(); free_q_.push(frame); encoding_ = false; lk.unlock();
        free_cv_.notify_one();
        frame = frame_;
      }
      // LOGI(VideoEncoderFFmpeg) << "Loop() frame pts=" << frame->pts;
    } else {
      if (eos_sent_) break;
      frame = nullptr;
      lk.unlock();
    }

    if (frame) {
      std::lock_guard<std::mutex> lk(info_mtx_);
      if (frame->pts == AV_NOPTS_VALUE) {
        frame->pts = frame_count_ * param_.time_base / param_.frame_rate;
        frame->pkt_pts = frame->pts;
      }
      encoding_info_[data_index_] = (EncodingInfo){ frame->pkt_pts, frame->pkt_dts, CurrentTick(), 0 };
      frame->pts = data_index_++;
      frame->pkt_pts = frame->pts;
      frame_count_++;
    }

    do {
      int got_packet = 0;
      ret = avcodec_encode_video2(codec_ctx_, packet_, frame, &got_packet);
      if (ret < 0) {
        LOGE(VideoEncoderFFmpeg) << "Loop() avcodec_encode_video2 failed, ret=" << ret;
        break;
      }
      if (!sws_ctx_ && frame != nullptr) {
        lk.lock(); free_q_.push(frame); encoding_ = false; lk.unlock();
        free_cv_.notify_one();
      }
      if (!ret && got_packet && packet_->size) {
        // find out packet and update encoding info
        std::unique_lock<std::mutex> ilk(info_mtx_);
        int64_t index = packet_->pts;
        auto info_it = encoding_info_.find(index);
        if (info_it != encoding_info_.end()) {
          info_it->second.end_tick = CurrentTick();
          packet_->pts = info_it->second.pts;
          if (info_it->second.dts == AV_NOPTS_VALUE) {
            packet_->dts = (packet_count_ - 2) * param_.time_base / param_.frame_rate;
          } else {
            packet_->dts = info_it->second.dts;
          }
        } else {
          LOGW(VideoEncoderFFmpeg) << "Loop() restore encoding info failed, index=" << index;
        }
        ilk.unlock();
        // LOGI(VideoEncoderFFmpeg) << "Loop() got packet: size=" << packet_->size <<
        //     ", pts=" << packet_->pts << ", dts=" << packet_->dts <<
        //     ((packet_->flags & AV_PKT_FLAG_KEY) ? " [K]" : "");
        VideoPacket packet;
        memset(&packet, 0, sizeof(VideoPacket));
        packet.data = packet_->data;
        packet.size = packet_->size;
        packet.pts = packet_->pts;
        packet.dts = packet_->dts;
        if (packet_->flags & AV_PKT_FLAG_KEY) packet.SetKey();
        PushBuffer(&packet);
        packet_count_++;
        av_packet_unref(packet_);
        std::lock_guard<std::mutex> lk(cb_mtx_);
        if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_DATA);
      }
      std::unique_lock<std::mutex> lk(input_mtx_);
      if (!data_q_.empty() || !eos_got_) {
        break;
      } else if (ret != 0 || !got_packet) {
        if (eos_sent_) break;
        eos_sent_ = true;
        lk.unlock();
        std::lock_guard<std::mutex> lk(cb_mtx_);
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
