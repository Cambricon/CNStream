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

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/log.h>
};

#include <string>
#include <algorithm>

#include "ffmpeg_parser.hpp"
#include "glog/logging.h"

RingBuffer::RingBuffer(const size_t capacity)
  : front_(0)
  , rear_(0)
  , size_(0)
  , capacity_(capacity) {
  data_ = new(std::nothrow) uint8_t[capacity];
}

size_t RingBuffer::Write(const void *data, const size_t bytes) {
  if (bytes == 0) return 0;

  std::unique_lock<std::mutex> lk(mutex_);
  const auto capacity = capacity_;
  while ((capacity - size_) < bytes) {
    if (cond_w_.wait_for(lk, std::chrono::seconds(2)) == std::cv_status::timeout) {
      return -1;
    }
  }
  const auto bytes_to_write = bytes;
  if (bytes_to_write <= capacity - rear_) {
    memcpy(data_ + rear_, data, bytes_to_write);
    rear_ += bytes_to_write;
    if (rear_ == capacity) rear_ = 0;
  } else {
    const auto size_1 = capacity - rear_;
    memcpy(data_ + rear_, data, size_1);
    const auto size_2 = bytes_to_write - size_1;
    memcpy(data_, static_cast<const uint8_t*>(data) + size_1, size_2);
    rear_ = size_2;
  }

  size_ += bytes_to_write;
  cond_r_.notify_one();
  return bytes_to_write;
}

size_t RingBuffer::Read(void *data, const size_t bytes) {
  if (bytes == 0) return 0;

  std::unique_lock<std::mutex> lk(mutex_);
  while (!size_) {
    if (cond_r_.wait_for(lk, std::chrono::seconds(2)) == std::cv_status::timeout) {
      return -1;
    }
  }
  const auto bytes_to_read = std::min(bytes, size_);

  const auto capacity = capacity_;

  if (bytes_to_read <= capacity - front_) {
    memcpy(data, data_ + front_, bytes_to_read);
    front_ += bytes_to_read;
    if (front_ == capacity) front_ = 0;
  } else {
    const auto size_1 = capacity - front_;
    memcpy(data, data_ + front_, size_1);
    const auto size_2 = bytes_to_read - size_1;
    memcpy(static_cast<uint8_t*>(data) + size_1, data_, size_2);
    front_ = size_2;
  }
  size_ -= bytes_to_read;

  cond_w_.notify_one();
  return bytes_to_read;
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/**
 * FFMPEG use AVCodecParameters instead of AVCodecContext 
 * since from version 3.1(libavformat/version:57.40.100)
 **/

#define FFMPEG_VERSION_3_1 AV_VERSION_INT(57, 40, 100)

struct local_ffmpeg_init {
  local_ffmpeg_init() {
    avcodec_register_all();
    av_register_all();
    avformat_network_init();
  }
};
static local_ffmpeg_init init_ffmpeg;

namespace cnstream {

StreamParser::StreamParser() {
  impl_ = new (std::nothrow) StreamParserImpl();
}

StreamParser::~StreamParser() {
  if (impl_) delete impl_;
}

int StreamParser::Open(std::string fmt) {
  if (impl_) {
    return impl_->Open(fmt);
  }
  return -1;
}

void StreamParser::Close() {
  if (impl_) {
    impl_->Close();
  }
}

int StreamParser::Parse(unsigned char *bitstream, int size) {
  if (impl_) {
    return impl_->Parse(bitstream, size);
  }
  return -1;
}

bool StreamParser::GetInfo(VideoStreamInfo &info) {
  if (impl_) {
    return impl_->GetInfo(info);
  }
  return false;
}

static int read_packet(void *opaque, uint8_t *buf, int buf_size) {
  RingBuffer *queue_ = reinterpret_cast<RingBuffer*>(opaque);
  if (queue_) {
    int size = queue_->Read(buf, buf_size);
    if (size < 0) {
      return AVERROR_EOF;
    }
    return size;
  }
  return AVERROR_EOF;
}

bool GetVideoStreamInfo(const AVFormatContext *ic, int &video_index, VideoStreamInfo &info) {  // NOLINT
  video_index = -1;
  AVStream* st = nullptr;
  for (uint32_t loop_i = 0; loop_i < ic->nb_streams; loop_i++) {
    st = ic->streams[loop_i];
  #if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
    if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
  #else
    if (st->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
  #endif
      video_index = loop_i;
      break;
    }
  }
  if (video_index == -1) {
    LOG(ERROR) << "Didn't find a video stream.";
    return false;
  }

#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  info.codec_id = st->codecpar->codec_id;
  info.codec_width = st->codecpar->width;
  info.codec_height = st->codecpar->height;
  int field_order = st->codecpar->field_order;
  info.color_space = st->codecpar->color_space;
  info.bitrate = st->codecpar->bit_rate / 1000;
#else
  info.codec_id = st->codec->codec_id;
  info.codec_width = st->codec->width;
  info.codec_height = st->codec->height;
  int field_order = st->codec->field_order;
  info.color_space = st->codec->colorspace;
  info.bitrate = st->codec->bit_rate / 1000;
#endif
  /*At this moment, if the demuxer does not set this value (avctx->field_order == UNKNOWN),
  *   the input stream will be assumed as progressive one.
  */
  switch (field_order) {
  case AV_FIELD_TT:
  case AV_FIELD_BB:
  case AV_FIELD_TB:
  case AV_FIELD_BT:
    info.progressive = 0;
    break;
  case AV_FIELD_PROGRESSIVE:  // fall through
  default:
    info.progressive = 1;
    break;
  }
  info.framerate.den = st->avg_frame_rate.den;
  info.framerate.num = st->avg_frame_rate.num;
  info.time_base = st->time_base;

#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  uint8_t* extradata = st->codecpar->extradata;
  int extradata_size = st->codecpar->extradata_size;
#else
  unsigned char* extradata = st->codec->extradata;
  int extradata_size = st->codec->extradata_size;
#endif
  if (extradata && extradata_size) {
    info.extra_data.reserve(extradata_size);
    memcpy(info.extra_data.data(), extradata, extradata_size);
  }
  return true;
}

void StreamParserImpl::FindInfo() {
  AVFormatContext *ic = nullptr;
  AVIOContext *avio = nullptr;
  unsigned char *io_buffer = nullptr;

  io_buffer = (unsigned char*)av_malloc(io_buffer_size_ + FF_INPUT_BUFFER_PADDING_SIZE);
  avio = avio_alloc_context(io_buffer, io_buffer_size_, 0, this->queue_, &read_packet, nullptr, nullptr);
  if (!avio) {
    return;
  }
  ic = avformat_alloc_context();
  if (!ic) {
    av_freep(&avio);
    return;
  }
  ic->pb = avio;

  // open input
  ic->flags |= AVFMT_FLAG_NOBUFFER;
  ic->probesize = 100 * 1024;  // FIXME

  AVInputFormat *ifmt = nullptr;
  if (!fmt_.empty()) {
    ifmt = av_find_input_format(fmt_.c_str());
  }
  int ret_code = avformat_open_input(&ic, "mem", ifmt, NULL);
  if (0 != ret_code) {
    avformat_close_input(&ic);
    return;
  }

  if (avformat_find_stream_info(ic, NULL) < 0) {
    avformat_close_input(&ic);
    return;
  }

  VideoStreamInfo info;
  int video_index;
  if (!GetVideoStreamInfo(ic, video_index, info)) {
    avformat_close_input(&ic);
    return;
  }

  promise_.set_value(info);
  info_got_.store(1);

  LOG(INFO) << this << " codec_id = " << info.codec_id;
  LOG(INFO) << this << " framerate = " << info.framerate.num << "/" << info.framerate.den;
  avformat_close_input(&ic);
  return;
}

int StreamParserImpl::Parse(unsigned char *buf, int size) {
  if (info_got_.load()) {
    return 1;
  }
  if (!buf || !size || !queue_) {
    return 0;
  }
  // feed frame-bitstream
  int offset = 0;
  while (1) {
    int bytes = queue_->Write(buf + offset, size - offset);
    if (bytes < 0) {
      LOG(ERROR) << this << " Write failed";
      return -1;
    }
    offset += bytes;
    if (offset >= size) {
      break;
    }
  }
  return 0;
}

bool StreamParserImpl::GetInfo(VideoStreamInfo &info) {
  if (info_ready_.load()) {
    info = info_;
    return true;
  }
  if (info_got_.load()) {
    std::future<VideoStreamInfo> future_ = promise_.get_future();
    info_ = future_.get();
    info = info_;
    info_ready_.store(1);
    return true;
  }
  return false;
}

}  // namespace cnstream
