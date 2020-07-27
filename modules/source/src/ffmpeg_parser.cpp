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
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/log.h>
};

#include <string>
#include <algorithm>
#include <vector>

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
    avdevice_register_all();
  }
};
static local_ffmpeg_init init_ffmpeg;

namespace cnstream {

class StreamParserImpl {
 public:
  StreamParserImpl() {}
  ~StreamParserImpl() {}
  int Open(std::string fmt) {
    queue_ = new (std::nothrow) RingBuffer(256 * 1024);
    if (!queue_) return -1;
    fmt_ = fmt;
    thread_ = std::thread(&StreamParserImpl::FindInfo, this);
    return 0;
  }

  void Close() {
    if (thread_.joinable()) {
      thread_.join();
    }
    if (queue_) {
      delete queue_, queue_ = nullptr;
    }
  }

  int Parse(unsigned char *bitstream, int size);
  bool GetInfo(VideoStreamInfo &info);  // NOLINT

 private:
  void FindInfo();
  static constexpr int io_buffer_size_ = 32768;
  std::string fmt_;
  RingBuffer *queue_ = nullptr;
  std::promise<VideoStreamInfo> promise_;
  std::atomic<int> info_got_{0};
  std::atomic<int> info_ready_{0};
  VideoStreamInfo info_;
  std::thread thread_;
};  // class StreamParserImpl

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
    av_free(io_buffer);
    return;
  }
  ic = avformat_alloc_context();
  if (!ic) {
    av_freep(&avio->buffer);
    av_free(avio);
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
    av_freep(&avio->buffer);
    av_free(avio);
    ic->pb = nullptr;
    avformat_close_input(&ic);
    return;
  }

  if (avformat_find_stream_info(ic, NULL) < 0) {
    av_freep(&avio->buffer);
    av_free(avio);
    ic->pb = nullptr;
    avformat_close_input(&ic);
    return;
  }

  VideoStreamInfo info;
  int video_index;
  if (!GetVideoStreamInfo(ic, video_index, info)) {
    av_freep(&avio->buffer);
    av_free(avio);
    ic->pb = nullptr;
    avformat_close_input(&ic);
    return;
  }

  promise_.set_value(info);
  info_got_.store(1);

  LOG(INFO) << this << " codec_id = " << info.codec_id;
  LOG(INFO) << this << " framerate = " << info.framerate.num << "/" << info.framerate.den;
  // free avio explicitly
  av_freep(&avio->buffer);
  av_free(avio);
  ic->pb = nullptr;
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

static int FindStartCode(unsigned char *buf) {
  if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1) {
    return 4;
  }
  if (buf[0] == 0 && buf[1] == 0 && buf[2] == 1) {
    return 3;
  }
  return 0;
}

static int GetNaluH2645(unsigned char *buf, int len, bool isH264, std::vector<NalDesc> &vec_desc) {  // NOLINT
  std::vector<int> vec_pos;
  for (int i = 0; i < len - 4; i++) {
    int size = FindStartCode(buf + i);
    if (!size) {
      continue;
    }
    vec_pos.push_back(i);
    i += size - 1;
  }
  if (vec_pos.empty()) {
    return 0;
  }

  int num = vec_pos.size();
  for (int i = 0; i < num - 1; i++) {
    NalDesc desc;
    desc.nal = buf + vec_pos[i];
    desc.len = vec_pos[i + 1] - vec_pos[i];
    int type_idx = (desc.nal[2] == 1) ? 3 : 4;
    if (desc.len < type_idx) {
      LOG(ERROR) << "INVALID nal size";
      return -1;
    }
    if (isH264) {
      desc.type = desc.nal[type_idx] & 0x1F;
    } else {
      desc.type = (desc.nal[type_idx] >> 1) & 0x3F;
    }
    vec_desc.push_back(desc);
  }

  // handle the last nal
  if (vec_pos[num - 1]) {
    NalDesc desc;
    desc.nal = buf + vec_pos[num - 1];
    desc.len = len - vec_pos[num - 1];
    int type_idx = (desc.nal[2] == 1) ? 3 : 4;
    if (desc.len >= type_idx) {
      if (isH264) {
        desc.type = desc.nal[type_idx] & 0x1F;
      } else {
        desc.type = (desc.nal[type_idx] >> 1) & 0x3F;
      }
    }
    vec_desc.push_back(desc);
  }
  return 0;
}

H2645NalSplitter::~H2645NalSplitter() {
  if (es_buffer_) {
    delete es_buffer_, es_buffer_ = nullptr;
  }
}

int H2645NalSplitter::SplitterWriteFrame(unsigned char *buf, int len) {
  if (buf && len) {
    std::vector<NalDesc> vec_desc;
    if (GetNaluH2645(buf, len, isH264_, vec_desc) < 0) {
      return -1;
    }
    for (auto &it : vec_desc) {
      this->SplitterOnNal(it, false);
    }
  } else {
    NalDesc desc;
    this->SplitterOnNal(desc, true);
  }
  return 0;
}

int H2645NalSplitter::SplitterWriteChunk(unsigned char *buf, int len) {
  static const int max_es_buffer_size = 1024 * 1024;
  if (buf && len) {
    if (!es_buffer_) {
      es_buffer_ = new(std::nothrow) unsigned char[max_es_buffer_size];
      if (!es_buffer_) {
        LOG(ERROR) << "Failed to alloc es_buffer";
        return -1;
      }
      es_len_ = 0;
    }
    if (es_len_ + len > max_es_buffer_size) {
      LOG(ERROR) << "Buffer overflow...FIXME";
      return -1;
    }
    memcpy(es_buffer_ + es_len_, buf, len);
    es_len_ += len;

    std::vector<NalDesc> vec_desc;
    int ret = GetNaluH2645(es_buffer_, es_len_, isH264_, vec_desc);
    if (ret < 0) {
      return ret;
    }
    // remove the last one
    if (vec_desc.size()) {
      NalDesc desc = vec_desc[vec_desc.size() - 1];
      vec_desc.pop_back();

      for (auto &it : vec_desc) {
       this->SplitterOnNal(it, false);
      }

      if (desc.len != es_len_) {
        memmove(es_buffer_, desc.nal, desc.len);
        es_len_ = desc.len;
      }
    }
    return 0;
  }

  // flush data...
  if (es_buffer_ && es_len_) {
    NalDesc desc;
    desc.nal = es_buffer_;
    desc.len = es_len_;
    if (es_len_ > 4) {
      int type_idx = (desc.nal[2] == 1) ? 3 : 4;
      if (isH264_) {
        desc.type = desc.nal[type_idx] & 0x1F;
      } else {
        desc.type = (desc.nal[type_idx] >> 1)& 0x3F;
      }
    }
    this->SplitterOnNal(desc, true);
  }
  return 0;
}

}  // namespace cnstream
