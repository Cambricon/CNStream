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

#ifdef __cplusplus
extern "C" {
#endif
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#ifdef __cplusplus
}
#endif

#include <algorithm>
#include <atomic>
#include <iostream>
#include <string>

#include "cnstream_logging.hpp"

#include "video_sink.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace cnstream {

namespace video {

class VideoSink {
 public:
  using Param = cnstream::VideoSink::Param;

  explicit VideoSink(const cnstream::VideoSink::Param &param);
  ~VideoSink();

  int Start();
  int Stop();
  int Write(const VideoPacket *packet);

 private:
  bool IsKeyFrame(const uint8_t *data, int size, bool h264);
  bool ExtractPS(const uint8_t *data, int size, bool h264);

  Param param_;
  std::atomic<bool> started_{false};
  AVFormatContext *ctx_ = nullptr;
  AVPacket *packet_ = nullptr;
  int64_t frame_count_ = 0;
  bool header_written_ = false;
  uint8_t *ps_ = nullptr;
  uint32_t ps_size_ = 0;
  int64_t init_timestamp_ = 0;
};

#define VERSION_LAVC_ALLOC_PACKET AV_VERSION_INT(57, 20, 102)
#define VERSION_LAVF_AVCPAR AV_VERSION_INT(57, 40, 100)

VideoSink::VideoSink(const Param &param) : param_(param) {
  av_register_all();
}

VideoSink::~VideoSink() { Stop(); }

int VideoSink::Start() {
  if (started_) return cnstream::VideoSink::SUCCESS;

  LOGI(VideoSink) << "Start() file name: " << param_.file_name;

  // Check parameters
  if (param_.pixel_format != VideoPixelFormat::I420) {
    LOGE(VideoSink) << "Start() Only support pixel format: YUV I420";
    return cnstream::VideoSink::ERROR_PARAMETERS;
  }
  if (param_.codec_type != VideoCodecType::H264 && param_.codec_type != VideoCodecType::H265) {
    LOGE(VideoSink) << "Start() Only support codec type: H264 and H265";
    return cnstream::VideoSink::ERROR_PARAMETERS;
  }

  auto dot = param_.file_name.find_last_of(".");
  if (dot == std::string::npos) {
    LOGE(VideoSink) << "Start() unknown file type \"" << param_.file_name << "\"";
    return cnstream::VideoSink::ERROR_PARAMETERS;
  }
  std::string ext_name = param_.file_name.substr(dot + 1);
  std::transform(ext_name.begin(), ext_name.end(), ext_name.begin(), ::tolower);
  if (ext_name != "mp4" && ext_name != "mkv" && ext_name != "flv" && ext_name != "avi") {
    LOGE(VideoSink) << "Start() unsupported file type \"" << param_.file_name << "\"";
    return cnstream::VideoSink::ERROR_PARAMETERS;
  }
  if (ext_name != "mp4" && ext_name != "mkv" && param_.codec_type == VideoCodecType::H265) {
    LOGE(VideoSink) << "Start() only mp4 and mkv support HEVC video";
    return cnstream::VideoSink::ERROR_PARAMETERS;
  }
  std::string format = ext_name;
  if (ext_name == "mkv") format = "matroska";

  avformat_alloc_output_context2(&ctx_, nullptr, format.c_str(), param_.file_name.c_str());
  if (!ctx_) {
    LOGE(VideoSink) << "Start() avformat_alloc_output_context2 for \"" << ext_name << "\" failed";
    return cnstream::VideoSink::ERROR_FAILED;
  }

  AVCodecID codec_id = param_.codec_type == VideoCodecType::H264 ? AV_CODEC_ID_H264 : AV_CODEC_ID_HEVC;
  AVCodec *codec = avcodec_find_decoder(codec_id);
  AVStream *stream = avformat_new_stream(ctx_, codec);
  if (!stream) {
    LOGE(VideoSink) << "Start() avformat_new_stream failed";
    avformat_free_context(ctx_);
    ctx_ = nullptr;
    return cnstream::VideoSink::ERROR_FAILED;
  }
  if (!stream) {
    LOGE(VideoSink) << "Start() avformat_new_stream failed";
    avformat_free_context(ctx_);
    ctx_ = nullptr;
    return cnstream::VideoSink::ERROR_FAILED;
  }
  stream->id = ctx_->nb_streams - 1;
  stream->avg_frame_rate = av_d2q(param_.frame_rate, 60000);
  stream->time_base = (AVRational){1, static_cast<int>(param_.time_base)};
#if LIBAVFORMAT_VERSION_INT < VERSION_LAVF_AVCPAR
  stream->codec->codec_type = codec->type;
  stream->codec->codec_id = codec->id;
  stream->codec->codec_tag = 0;
  stream->codec->pix_fmt = AV_PIX_FMT_YUV420P;
  stream->codec->width = param_.width;
  stream->codec->height = param_.height;
  stream->codec->bit_rate = param_.bit_rate;
  stream->codec->gop_size = param_.gop_size;
  stream->codec->time_base = stream->time_base;
#else
  stream->codecpar->codec_type = codec->type;
  stream->codecpar->codec_id = codec->id;
  stream->codecpar->codec_tag = 0;
  stream->codecpar->format = AV_PIX_FMT_YUV420P;
  stream->codecpar->width = param_.width;
  stream->codecpar->height = param_.height;
  stream->codecpar->bit_rate = param_.bit_rate;
#endif
  if (!(ctx_->oformat->flags & AVFMT_NOFILE)) {
    int ret = avio_open(&ctx_->pb, param_.file_name.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
      LOGE(VideoSink) << "Start() avio_open \"" << param_.file_name << "\" failed, ret=" << ret;
      avformat_free_context(ctx_);
      ctx_ = nullptr;
      return cnstream::VideoSink::ERROR_FAILED;
    }
  }

  av_dump_format(ctx_, 0, param_.file_name.c_str(), 1);

#if LIBAVCODEC_VERSION_INT < VERSION_LAVC_ALLOC_PACKET
  packet_ = reinterpret_cast<AVPacket *>(av_mallocz(sizeof(AVPacket)));
#else
  packet_ = av_packet_alloc();
#endif
  av_init_packet(packet_);

  started_ = true;
  return cnstream::VideoSink::SUCCESS;
}

int VideoSink::Stop() {
  if (!started_) return cnstream::VideoSink::SUCCESS;

  if (ctx_) {
    if (header_written_) {
      av_write_trailer(ctx_);
      LOGI(VideoSink) << "Stop() av_write_trailer ok";
    }
#if LIBAVFORMAT_VERSION_INT < VERSION_LAVF_AVCPAR
    ctx_->streams[0]->codec->extradata = nullptr;
    ctx_->streams[0]->codec->extradata_size = 0;
#else
    ctx_->streams[0]->codecpar->extradata = nullptr;
    ctx_->streams[0]->codecpar->extradata_size = 0;
#endif
    if (ctx_->pb) {
      avio_close(ctx_->pb);
      ctx_->pb = nullptr;
    }
    avformat_free_context(ctx_);
    ctx_ = nullptr;
  }
  if (packet_) {
    av_packet_unref(packet_);
    av_free(packet_);
    packet_ = nullptr;
  }
  if (ps_) delete[] ps_;

  started_ = false;

  return cnstream::VideoSink::SUCCESS;
}

int VideoSink::Write(const VideoPacket *packet) {
  if (!started_) {
    LOGE(VideoSink) << "Write() sink is stopped";
    return cnstream::VideoSink::ERROR_STATE;
  }
  if (!packet || !packet->data || !packet->size) {
    LOGE(VideoSink) << "Write() invalid parameters";
    return cnstream::VideoSink::ERROR_PARAMETERS;
  }

  int ret;
  AVStream *stream = ctx_->streams[0];
  AVRational frame_rate = stream->avg_frame_rate;
  AVRational time_base = stream->time_base;

  if (!ps_ && ExtractPS(packet->data, packet->size, param_.codec_type == VideoCodecType::H264)) {
    LOGI(VideoSink) << "Write() parameter sets found, size=" << ps_size_;
#if LIBAVFORMAT_VERSION_INT < VERSION_LAVF_AVCPAR
    stream->codec->extradata = ps_;
    stream->codec->extradata_size = ps_size_;
#else
    stream->codecpar->extradata = ps_;
    stream->codecpar->extradata_size = ps_size_;
#endif
  }

  // bool key_frame = packet->IsKey();
  bool key_frame = IsKeyFrame(packet->data, packet->size, param_.codec_type == VideoCodecType::H264);
  if (!header_written_) {
    if (!param_.start_from_key_frame || key_frame) {
      ret = avformat_write_header(ctx_, nullptr);
      if (ret < 0) {
        LOGE(VideoSink) << "Write() avformat_write_header failed, ret=" << ret;
        return cnstream::VideoSink::ERROR_FAILED;
      }
      header_written_ = true;
      if (packet->pts != INVALID_TIMESTAMP) {
        init_timestamp_ = av_rescale_q(packet->pts, (AVRational){1, static_cast<int>(param_.time_base)}, time_base);
      }
      LOGI(VideoSink) << "Write() avformat_write_header ok";
    } else {
      LOGI(VideoSink) << "Write() skip non key frame before writing header";
      return cnstream::VideoSink::SUCCESS;
    }
  }

  av_new_packet(packet_, packet->size);
  memcpy(packet_->data, packet->data, packet->size);

  // Rescale timestamps
  if (packet->pts != INVALID_TIMESTAMP) {
    packet_->pts = av_rescale_q(packet->pts, (AVRational){1, static_cast<int>(param_.time_base)}, time_base);
  } else {
    packet_->pts = av_rescale_q(frame_count_, (AVRational){frame_rate.den, frame_rate.num}, time_base);
  }
  if (packet->dts != INVALID_TIMESTAMP) {
    packet_->dts = av_rescale_q(packet->dts, (AVRational){1, static_cast<int>(param_.time_base)}, time_base);
  } else {
    packet_->dts = av_rescale_q(frame_count_ - 1, (AVRational){frame_rate.den, frame_rate.num}, time_base);
    if (packet->pts != INVALID_TIMESTAMP) packet_->dts += init_timestamp_;
  }
  packet_->duration = av_rescale_q(1, (AVRational){frame_rate.den, frame_rate.num}, time_base);
  packet_->pos = -1;
  packet_->stream_index = 0;
  if (key_frame) packet_->flags |= AV_PKT_FLAG_KEY;

  // LOGI(VideoSink) << "Write() size=" << packet_->size << ",pts=" << packet_->pts << ",dts=" << packet_->dts
  //                 << ",duration=" << packet_->duration << ",index=" << frame_count_ << (key_frame ? " [K]" : "");

  ret = av_interleaved_write_frame(ctx_, packet_);
  if (ret < 0) {
    LOGE(VideoSink) << "Write() av_interleaved_write_frame failed, ret=" << ret;
    av_packet_unref(packet_);
    return cnstream::VideoSink::ERROR_FAILED;
  }

  av_packet_unref(packet_);
  frame_count_++;
  return cnstream::VideoSink::SUCCESS;
}

bool VideoSink::IsKeyFrame(const uint8_t *data, int size, bool h264) {
  const uint8_t *p = data;
  const uint8_t *end = p + size;
  const uint8_t *nal_start;

  auto find_startcode = [](const uint8_t *start, const uint8_t *end) -> const uint8_t * {
    if (start[0] == 0 && start[1] == 0) {
      if (start[2] == 1 && (start + 3) <= end) {
        return start + 3;
      }
      if (start[2] == 0 && start[3] == 1 && (start + 4) <= end) {
        return start + 4;
      }
    }
    return nullptr;
  };

  do {
    nal_start = find_startcode(p, end);
    if (nal_start) {
      if (h264) {
        if ((*nal_start & 0x1f) == 5) return true;
      } else {
        uint8_t nal_type = (*nal_start & 0x7e) >> 1;
        if (nal_type >= 16 && nal_type <= 21) return true;
      }
      p = nal_start;
    }
    while (p < end && *(p++) == 0) {
    }
  } while (p < end);

  return false;
}

bool VideoSink::ExtractPS(const uint8_t *data, int size, bool h264) {
  const uint8_t *p = data;
  const uint8_t *end = p + size;
  const uint8_t *nal_start, *ps = nullptr;

  auto find_startcode = [](const uint8_t *start, const uint8_t *end) -> const uint8_t * {
    if (start[0] == 0 && start[1] == 0) {
      if (start[2] == 1 && (start + 3) <= end) {
        return start + 3;
      }
      if (start[2] == 0 && start[3] == 1 && (start + 4) <= end) {
        return start + 4;
      }
    }
    return nullptr;
  };

  bool ret = false;
  do {
    nal_start = find_startcode(p, end);
    if (nal_start || p == end) {
      if (ps) {
        uint32_t ps_len = p - ps;
        uint8_t *last_ps = ps_;
        ps_ = new (std::nothrow) uint8_t[ps_size_ + ps_len];
        if (last_ps) {
          memcpy(ps_, last_ps, ps_size_);
          delete[] last_ps;
        }
        memcpy(ps_ + ps_size_, ps, ps_len);
        ps_size_ += ps_len;
        ps = nullptr;
        ret = true;
      }
      if (nal_start) {
        if (h264) {
          uint8_t nal_type = *nal_start & 0x1f;
          if (nal_type == 7 || nal_type == 8) ps = p;
        } else {
          uint8_t nal_type = (*nal_start & 0x7e) >> 1;
          if (nal_type >= 32 && nal_type <= 34) ps = p;
        }
        p = nal_start;
      }
    }
    while (p < end && *(p++) == 0) {
    }
  } while (p < end);

  return ret;
}

}  // namespace video

VideoSink::VideoSink(const Param &param) { sink_ = new (std::nothrow) cnstream::video::VideoSink(param); }

VideoSink::~VideoSink() {
  if (sink_) {
    delete sink_;
    sink_ = nullptr;
  }
}

VideoSink::VideoSink(VideoSink &&sink) {
  sink_ = sink.sink_;
  sink.sink_ = nullptr;
}

VideoSink &VideoSink::operator=(VideoSink &&sink) {
  if (sink_) {
    delete sink_;
  }
  sink_ = sink.sink_;
  sink.sink_ = nullptr;
  return *this;
}

int VideoSink::Start() {
  if (sink_) return sink_->Start();
  return ERROR_FAILED;
}

int VideoSink::Stop() {
  if (sink_) return sink_->Stop();
  return ERROR_FAILED;
}

int VideoSink::Write(const VideoPacket *packet) {
  if (sink_) return sink_->Write(packet);
  return ERROR_FAILED;
}

}  // namespace cnstream
