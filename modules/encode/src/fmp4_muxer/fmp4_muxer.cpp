/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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
#include "fmp4_muxer.hpp"

#include <iostream>
#include <string>

#include "cnstream_logging.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace cnstream {

#define FFMPEG_VERSION_3_1 AV_VERSION_INT(57, 40, 100)

struct local_ffmpeg_init {
  local_ffmpeg_init() {
    avcodec_register_all();
    av_register_all();
  }
};
static local_ffmpeg_init init_ffmpeg;

int Mp4Muxer::Open(const std::string &filename, int width, int height, VideoCodecType codec_type) {
  avformat_alloc_output_context2(&ctx_, nullptr, "mp4", filename.c_str());
  if (!ctx_) {
    LOGE(VENC) << "Mp4Muxer::Open() avformat_alloc_output_context2 for mp4 failed";
    return -1;
  }

  AVCodecID codec_id = codec_type == VideoCodecType::H265 ? AV_CODEC_ID_H265 : AV_CODEC_ID_H264;
  AVCodec *codec = avcodec_find_decoder(codec_id);
  AVStream *stream = avformat_new_stream(ctx_, codec);
  if (!stream) {
    LOGE(VENC) << "Mp4Muxer::Open() avformat_new_stream failed";
    avformat_free_context(ctx_);
    ctx_ = nullptr;
    return -1;
  }
  if (!stream) {
    LOGE(VENC) << "Mp4Muxer::Open() avformat_new_stream failed";
    avformat_free_context(ctx_);
    ctx_ = nullptr;
    return -1;
  }
  stream->id = ctx_->nb_streams - 1;
  stream->avg_frame_rate = av_d2q(30, 90000);
  stream->time_base = (AVRational){1, 90000};
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  stream->codecpar->codec_type = codec->type;
  stream->codecpar->codec_id = codec->id;
  stream->codecpar->codec_tag = 0;
  stream->codecpar->format = AV_PIX_FMT_NV12;
  stream->codecpar->width = width;
  stream->codecpar->height = height;
  stream->codecpar->bit_rate = 0;
#else
  stream->codec->codec_type = codec->type;
  stream->codec->codec_id = codec->id;
  stream->codec->codec_tag = 0;
  stream->codec->pix_fmt = AV_PIX_FMT_NV12;
  stream->codec->width = width;
  stream->codec->height = height;
  stream->codec->bit_rate = 0;
#endif

  // av_dump_format(ctx_, 0, filename.c_str(), 1);
  if (!(ctx_->oformat->flags & AVFMT_NOFILE)) {
    int ret = avio_open(&ctx_->pb, filename.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
      LOGE(VENC) << "Mp4Muxer::Open() avio_open \"" << filename << "\" failed, ret=" << ret;
      avformat_free_context(ctx_);
      ctx_ = nullptr;
      return -1;
    }
  }
  // fp_ = fopen("test.h264", "wb");
  return 0;
}

int Mp4Muxer::Close() {
  if (ctx_) {
    if (header_written_) {
      av_write_trailer(ctx_);
      LOGI(VENC) << "Mp4Muxer::Close() av_write_trailer ok";
    }
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
    ctx_->streams[0]->codecpar->extradata = nullptr;
    ctx_->streams[0]->codecpar->extradata_size = 0;
#else
    ctx_->streams[0]->codec->extradata = nullptr;
    ctx_->streams[0]->codec->extradata_size = 0;
#endif
    if (ctx_->pb) {
      avio_close(ctx_->pb);
      ctx_->pb = nullptr;
    }
    avformat_free_context(ctx_);
    ctx_ = nullptr;
  }

  // if(fp_) fclose(fp_), fp_ = nullptr;
  return 0;
}

// FIXME, assume that first three frames are sps & pps & idr
//
int Mp4Muxer::Write(CnedkVEncFrameBits *framebits) {
  // if (fp_) fwrite(framebits->bits, 1, framebits->len, fp_);
  AVStream *stream = ctx_->streams[0];
  AVRational frame_rate = stream->avg_frame_rate;
  AVRational time_base = stream->time_base;

  if (!header_written_) {
    if (framebits->pkt_type == CNEDK_VENC_PACKAGE_TYPE_SPS) {
      memcpy(extradata_, framebits->bits, framebits->len);
      extradata_len_ += framebits->len;
      return 0;
    }

    if (framebits->pkt_type == CNEDK_VENC_PACKAGE_TYPE_PPS || framebits->pkt_type == CNEDK_VENC_PACKAGE_TYPE_SPS_PPS) {
      memcpy(extradata_ + extradata_len_, framebits->bits, framebits->len);
      extradata_len_ += framebits->len;
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
      stream->codecpar->extradata = extradata_;
      stream->codecpar->extradata_size = extradata_len_;
#else
      stream->codec->extradata = extradata_;
      stream->codec->extradata_size = extradata_len_;
#endif

      AVDictionary *opts = NULL;
      av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
      int ret = avformat_write_header(ctx_, &opts);
      av_dict_free(&opts);
      if (ret < 0) {
        LOGE(VENC) << "Mp4Muxer::Write() avformat_write_header failed, ret=" << ret;
        return -1;
      }
      header_written_ = true;
      return 0;
    }
  }

  // discards sps & pps
  if (framebits->pkt_type == CNEDK_VENC_PACKAGE_TYPE_SPS || framebits->pkt_type == CNEDK_VENC_PACKAGE_TYPE_PPS ||
      framebits->pkt_type == CNEDK_VENC_PACKAGE_TYPE_SPS_PPS)
    return 0;

  AVPacket packet;
  av_init_packet(&packet);
  packet.data = framebits->bits, packet.size = framebits->len;
  // packet.pts = framebits->pts;
  // rescale timestamps
  if (packet.pts != AV_NOPTS_VALUE) {
    packet.pts = av_rescale_q(packet.pts, (AVRational){1, 90000}, time_base);
  } else {
    packet.pts = av_rescale_q(frame_count_, (AVRational){frame_rate.den, frame_rate.num}, time_base);
  }
  if (packet.dts != AV_NOPTS_VALUE) {
    packet.dts = av_rescale_q(packet.dts, (AVRational){1, 90000}, time_base);
  } else {
    packet.dts = av_rescale_q(frame_count_ - 1, (AVRational){frame_rate.den, frame_rate.num}, time_base);
    if (packet.pts != AV_NOPTS_VALUE) packet.dts += init_timestamp_;
  }
  packet.duration = av_rescale_q(1, (AVRational){frame_rate.den, frame_rate.num}, time_base);
  packet.pos = -1;
  packet.stream_index = 0;

  bool key_frame = (framebits->pkt_type == CNEDK_VENC_PACKAGE_TYPE_KEY_FRAME);
  if (key_frame) packet.flags |= AV_PKT_FLAG_KEY;

  int ret = av_interleaved_write_frame(ctx_, &packet);
  if (ret < 0) {
    LOGE(VENC) << "Mp4Muxer::Write() av_interleaved_write_frame failed, ret=" << ret;
    return -1;
  }

  frame_count_++;
  return 0;
}

}  // namespace cnstream
