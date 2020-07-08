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

#include "ffmpeg_video_encoder.hpp"

#include <glog/logging.h>
#include <string.h>

#define OUTPUT_BUFFER_SIZE 0x200000

namespace cnstream {

FFmpegVideoEncoder::FFmpegVideoFrame::FFmpegVideoFrame(FFmpegVideoEncoder *encoder) : encoder_(encoder) {
  frame_ = av_frame_alloc();
  frame_->width = encoder_->avcodec_ctx_->width;
  frame_->height = encoder_->avcodec_ctx_->height;
  frame_->format = encoder_->picture_format_;
  int align = (encoder_->picture_format_ == AV_PIX_FMT_RGB24 || encoder_->picture_format_ == AV_PIX_FMT_BGR24) ? 24 : 8;
  av_image_alloc(frame_->data, frame_->linesize, frame_->width, frame_->height, (AVPixelFormat)frame_->format, align);
}

FFmpegVideoEncoder::FFmpegVideoFrame::~FFmpegVideoFrame() {
  if (frame_) {
    av_freep(&(frame_->data[0]));
    av_frame_unref(frame_);
    av_free(frame_);
  }
}

void FFmpegVideoEncoder::FFmpegVideoFrame::Fill(uint8_t *data, int64_t timestamp) {
  if (frame_ == nullptr) return;

  frame_->pts = timestamp;

  int line_size;
  if (frame_->format == AV_PIX_FMT_RGB24 || frame_->format == AV_PIX_FMT_BGR24) {
    line_size = frame_->width * 3;
    for (int i = 0; i < frame_->height; i++) {
      memcpy(frame_->data[0] + frame_->linesize[0] * i, data + line_size * i, line_size);
    }
  } else if (frame_->format == AV_PIX_FMT_YUV420P) {
    line_size = frame_->width;
    int size = frame_->height * frame_->width;
    for (int i = 0; i < frame_->height; i++) {
      memcpy(frame_->data[0] + frame_->linesize[0] * i, data + line_size * i, line_size);
    }
    for (int i = 0; i < frame_->height / 2; i++) {
      memcpy(frame_->data[1] + frame_->linesize[1] * i, data + size + line_size / 2 * i, line_size / 2);
    }
    for (int i = 0; i < frame_->height / 2; i++) {
      memcpy(frame_->data[2] + frame_->linesize[2] * i, data + size * 5 / 4 + line_size / 2 * i, line_size / 2);
    }
  } else if (frame_->format == AV_PIX_FMT_NV21 || frame_->format == AV_PIX_FMT_NV12) {
    line_size = frame_->width;
    int size = frame_->height * frame_->width;
    for (int i = 0; i < frame_->height; i++) {
      memcpy(frame_->data[0] + frame_->linesize[0] * i, data + line_size * i, line_size);
    }
    for (int i = 0; i < frame_->height / 2; i++) {
      memcpy(frame_->data[1] + frame_->linesize[1] * i, data + size + line_size * i, line_size);
    }
  } else {
    LOG(ERROR) << "unsupport pixel format: " << frame_->format;
  }
}

FFmpegVideoEncoder::FFmpegVideoEncoder(const RtspParam &rtsp_param) : VideoEncoder(OUTPUT_BUFFER_SIZE) {
  switch (rtsp_param.color_format) {
    case YUV420:
      picture_format_ = AV_PIX_FMT_YUV420P;
      break;
    case RGB24:
      picture_format_ = AV_PIX_FMT_RGB24;
      break;
    case BGR24:
      picture_format_ = AV_PIX_FMT_BGR24;
      break;
    case NV21:
      picture_format_ = AV_PIX_FMT_NV21;
      break;
    case NV12:
      picture_format_ = AV_PIX_FMT_NV12;
      break;
    default:
      picture_format_ = AV_PIX_FMT_YUV420P;
      break;
  }
  switch (rtsp_param.codec_type) {
    case H264:
      avcodec_id_ = AV_CODEC_ID_H264;
      break;
    case HEVC:
      avcodec_id_ = AV_CODEC_ID_HEVC;
      break;
    case MPEG4:
      avcodec_id_ = AV_CODEC_ID_MPEG4;
      break;
    default:
      avcodec_id_ = AV_CODEC_ID_H264;
      break;
  }
  if (rtsp_param.frame_rate > 0) {
    frame_rate_ = av_d2q(static_cast<double>(rtsp_param.frame_rate), 60000);
  } else {
    frame_rate_.num = 25;
    frame_rate_.den = 1;
  }

  avcodec_register_all();
  av_register_all();

  avcodec_ = avcodec_find_encoder(avcodec_id_);
  if (!avcodec_) {  // plan to add qsv or other codec
    LOG(ERROR) << "cannot find encoder,use 'libx264'";
    avcodec_ = avcodec_find_encoder_by_name("libx264");
    if (avcodec_ == nullptr) {
      Destroy();
      LOG(ERROR) << "Can't find encoder with libx264";
      return;
    }
  }

  avcodec_ctx_ = avcodec_alloc_context3(avcodec_);
  avcodec_ctx_->codec_id = avcodec_id_;
  avcodec_ctx_->bit_rate = rtsp_param.kbps * 1000;
  avcodec_ctx_->width = rtsp_param.dst_width;
  avcodec_ctx_->height = rtsp_param.dst_height;
  avcodec_ctx_->time_base.num = frame_rate_.den;
  avcodec_ctx_->time_base.den = frame_rate_.num;
  avcodec_ctx_->framerate.num = frame_rate_.num;
  avcodec_ctx_->framerate.den = frame_rate_.den;
  avcodec_ctx_->gop_size = rtsp_param.gop;
  avcodec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
  avcodec_ctx_->max_b_frames = 1;
  // avcodec_ctx_->thread_count = 1;

  av_dict_set(&avcodec_opts_, "preset", "veryfast", 0);
  av_dict_set(&avcodec_opts_, "tune", "zerolatency", 0);
  av_dict_set(&avcodec_opts_, "level", "4.2", 0);
  av_dict_set(&avcodec_opts_, "profile", "high", 0);
  int ret = avcodec_open2(avcodec_ctx_, avcodec_, &avcodec_opts_);
  if (ret < 0) {
    LOG(ERROR) << "avcodec_open2() failed, ret=" << ret;
    Destroy();
    return;
  }

  if (picture_format_ != AV_PIX_FMT_YUV420P) {
    avframe_ = av_frame_alloc();
    avframe_->format = AV_PIX_FMT_YUV420P;
    avframe_->data[0] = nullptr;
    avframe_->linesize[0] = -1;
    avframe_->pts = 0;
    avframe_->width = avcodec_ctx_->width;
    avframe_->height = avcodec_ctx_->height;
    ret = av_image_alloc(avframe_->data, avframe_->linesize, avcodec_ctx_->width, avcodec_ctx_->height,
                         AV_PIX_FMT_YUV420P, 8);
    if (ret < 0) {
      LOG(ERROR) << "av_image_alloc() failed, ret=" << ret;
      Destroy();
      return;
    }

    sws_ctx_ = sws_getContext(avcodec_ctx_->width, avcodec_ctx_->height, picture_format_, avcodec_ctx_->width,
                              avcodec_ctx_->height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx_) {
      LOG(ERROR) << "sws_getContext() failed, ret=" << ret;
      Destroy();
      return;
    }
  }

#if LIBAVCODEC_VERSION_MAJOR < 59
  avpacket_ = reinterpret_cast<AVPacket *>(av_mallocz(sizeof(AVPacket)));
#else
  avpacket_ = av_packet_alloc();
#endif
  av_init_packet(avpacket_);
}

FFmpegVideoEncoder::~FFmpegVideoEncoder() {
  Stop();
  Destroy();
}

void FFmpegVideoEncoder::Destroy() {
  if (avcodec_ctx_) {
    avcodec_close(avcodec_ctx_);
    avcodec_ctx_ = nullptr;
  }
  if (avcodec_opts_) {
    av_dict_free(&avcodec_opts_);
    avcodec_opts_ = nullptr;
  }
  if (avframe_) {
    av_freep(&(avframe_->data[0]));
    av_frame_unref(avframe_);
    av_free(avframe_);
    avframe_ = nullptr;
  }
  if (sws_ctx_) {
    sws_freeContext(sws_ctx_);
    sws_ctx_ = nullptr;
  }
  if (avpacket_) {
    av_packet_unref(avpacket_);
    av_free(avpacket_);
    avpacket_ = nullptr;
  }
}

VideoEncoder::VideoFrame *FFmpegVideoEncoder::NewFrame() { return new FFmpegVideoFrame(this); }

uint32_t FFmpegVideoEncoder::GetOffset(const uint8_t *data) {
  uint32_t offset = 0;
  const uint8_t *p = data;
  if (p[0] == 0x00 && p[1] == 0x00) {
    if (p[2] == 0x01) {
      offset = 3;
    } else if ((p[2] == 0x00) && (p[3] == 0x01)) {
      offset = 4;
    }
  }
  return offset;
}

void FFmpegVideoEncoder::EncodeFrame(VideoFrame *frame) {
  FFmpegVideoFrame *ffpic = dynamic_cast<FFmpegVideoFrame *>(frame);
  AVFrame *picture = ffpic->Get();

  if (sws_ctx_) {
    sws_scale(sws_ctx_, picture->data, picture->linesize, 0, picture->height, avframe_->data, avframe_->linesize);
    avframe_->pts = picture->pts;
    picture = avframe_;
  }

  int ret = 0, got_packet;
  ret = avcodec_encode_video2(avcodec_ctx_, avpacket_, picture, &got_packet);
  if (ret < 0) {
    LOG(ERROR) << "avcodec_encode_video2() failed, ret=" << ret;
    return;
  }

  if (!ret && got_packet && avpacket_->size) {
    // LOG(INFO) << "===got packet: size=" << avpacket_->size << ", pts=" << avpacket_->pts;
    int offset = 0;
    uint8_t *packet_data = nullptr;
    packet_data = reinterpret_cast<uint8_t *>(avpacket_->data);
    offset = GetOffset(packet_data);
    size_t length = avpacket_->size - offset;
    uint8_t *data = avpacket_->data + offset;
    PushOutputBuffer(data, length, frame_count_, avpacket_->pts);
    frame_count_++;
    Callback(NEW_FRAME);
  }
  av_packet_unref(avpacket_);
}

}  // namespace cnstream
