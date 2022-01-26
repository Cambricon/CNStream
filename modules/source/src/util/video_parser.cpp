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

#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "cnstream_logging.hpp"
#include "video_parser.hpp"

CNS_IGNORE_DEPRECATED_PUSH

namespace cnstream {

/**
 * FFMPEG use FF_AV_INPUT_BUFFER_PADDING_SIZE instead of
 * FF_INPUT_BUFFER_PADDING_SIZE since from version 2.8
 * (avcodec.h/version:56.56.100)
 * */

#define FFMPEG_VERSION_2_8 AV_VERSION_INT(56, 56, 100)

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
#ifdef HAVE_FFMPEG_AVDEVICE
    avdevice_register_all();
#endif
  }
};
static local_ffmpeg_init init_ffmpeg;

static uint64_t GetTickCount() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

class FFParserImpl {
 public:
  explicit FFParserImpl(const std::string& stream_id) : stream_id_(stream_id) {}
  ~FFParserImpl() = default;

  static int InterruptCallBack(void* ctx) {
    FFParserImpl* demux = reinterpret_cast<FFParserImpl*>(ctx);
    if (demux->CheckTimeOut(GetTickCount())) {
      return 1;
    }
    return 0;
  }

  bool CheckTimeOut(uint64_t ul_current_time) {
    if ((ul_current_time - last_receive_frame_time_) / 1000 > max_receive_time_out_) {
      return true;
    }
    return false;
  }

  int Open(const std::string &url, IParserResult *result, bool only_key_frame = false) {
    std::unique_lock<std::mutex> guard(mutex_);
    if (!result) return -1;
    result_ = result;
    // format context
    fmt_ctx_ = avformat_alloc_context();
    if (!fmt_ctx_) {
      return -1;
    }
    url_name_ = url;

    AVInputFormat* ifmt = NULL;
    // for usb camera
#ifdef HAVE_FFMPEG_AVDEVICE
    const char* usb_prefix = "/dev/video";
    if (0 == strncasecmp(url_name_.c_str(), usb_prefix, strlen(usb_prefix))) {
    // open v4l2 input
#if defined(__linux) || defined(__unix)
      ifmt = av_find_input_format("video4linux2");
      if (!ifmt) {
        LOGE(SOURCE) << "[" << stream_id_ << "]: Could not find v4l2 format.";
        return false;
      }
#elif defined(_WIN32) || defined(_WIN64)
      ifmt = av_find_input_format("dshow");
      if (!ifmt) {
        LOGE(SOURCE) << "[" << stream_id_ << "]: Could not find dshow.";
        return false;
      }
#else
      LOGE(SOURCE) << "[" << stream_id_ << "]: Unsupported Platform";
      return false;
#endif
    }
#endif
    int ret_code;
    const char* p_rtsp_start_str = "rtsp://";
    if (0 == strncasecmp(url_name_.c_str(), p_rtsp_start_str, strlen(p_rtsp_start_str))) {
      AVIOInterruptCB intrpt_callback = {InterruptCallBack, this};
      fmt_ctx_->interrupt_callback = intrpt_callback;
      last_receive_frame_time_ = GetTickCount();
      // options
      av_dict_set(&options_, "buffer_size", "1024000", 0);
      av_dict_set(&options_, "max_delay", "500000", 0);
      av_dict_set(&options_, "stimeout", "20000000", 0);
      av_dict_set(&options_, "rtsp_flags", "prefer_tcp", 0);
    } else {
      // options
      av_dict_set(&options_, "buffer_size", "1024000", 0);
      av_dict_set(&options_, "max_delay", "500000", 0);
    }

    // open input
    ret_code = avformat_open_input(&fmt_ctx_, url_name_.c_str(), ifmt, &options_);
    if (0 != ret_code) {
      LOGI(SOURCE) << "[" << stream_id_ << "]: Couldn't open input stream -- " << url_name_;
      return -1;
    }
    // find video stream information
    ret_code = avformat_find_stream_info(fmt_ctx_, NULL);
    if (ret_code < 0) {
      LOGI(SOURCE) << "[" << stream_id_ << "]: Couldn't find stream information -- " << url_name_;
      return -1;
    }

    // fill info
    VideoInfo video_info;
    VideoInfo *info = &video_info;
    int video_index = -1;
    AVStream* st = nullptr;
    for (uint32_t loop_i = 0; loop_i < fmt_ctx_->nb_streams; loop_i++) {
      st = fmt_ctx_->streams[loop_i];
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
      LOGI(SOURCE) << "[" << stream_id_ << "]: Couldn't find a video stream -- " << url_name_;
      return -1;
    }
    video_index_ = video_index;

#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
    info->codec_id = st->codecpar->codec_id;
#ifdef HAVE_FFMPEG_AVDEVICE  // for usb camera
    info->format = st->codecpar->format;
    info->width = st->codecpar->width;
    info->height = st->codecpar->height;
#endif
    int field_order = st->codecpar->field_order;
#else
    info->codec_id = st->codec->codec_id;
#ifdef HAVE_FFMPEG_AVDEVICE  // for usb camera
    info->format = st->codec->format;
    info->width = st->codec->width;
    info->height = st->codec->height;
#endif
    int field_order = st->codec->field_order;
#endif

    /*At this moment, if the demuxer does not set this value (avctx->field_order == UNKNOWN),
    *   the input stream will be assumed as progressive one.
    */
    switch (field_order) {
    case AV_FIELD_TT:
    case AV_FIELD_BB:
    case AV_FIELD_TB:
    case AV_FIELD_BT:
      info->progressive = 0;
      break;
    case AV_FIELD_PROGRESSIVE:  // fall through
    default:
      info->progressive = 1;
      break;
    }

#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
    uint8_t* extradata = st->codecpar->extradata;
    int extradata_size = st->codecpar->extradata_size;
#else
    unsigned char* extradata = st->codec->extradata;
    int extradata_size = st->codec->extradata_size;
#endif
    if (extradata && extradata_size) {
      info->extra_data.resize(extradata_size);
      memcpy(info->extra_data.data(), extradata, extradata_size);
    }
    // bitstream filter
    bsf_ctx_ = nullptr;
    if (strstr(fmt_ctx_->iformat->name, "mp4") || strstr(fmt_ctx_->iformat->name, "flv") ||
        strstr(fmt_ctx_->iformat->name, "matroska")) {
      if (AV_CODEC_ID_H264 == info->codec_id) {
        bsf_ctx_ = av_bitstream_filter_init("h264_mp4toannexb");
      } else if (AV_CODEC_ID_HEVC == info->codec_id) {
        bsf_ctx_ = av_bitstream_filter_init("hevc_mp4toannexb");
      } else {
        bsf_ctx_ = nullptr;
      }
    }
    if (result_) {
      result_->OnParserInfo(info);
    }
    av_init_packet(&packet_);
    first_frame_ = true;
    eos_reached_ = false;
    open_success_ = true;
    only_key_frame_ = only_key_frame;
    return 0;
  }

  void Close() {
    std::unique_lock<std::mutex> guard(mutex_);
    if (fmt_ctx_) {
      avformat_close_input(&fmt_ctx_);
      avformat_free_context(fmt_ctx_);
      av_dict_free(&options_);
      if (bsf_ctx_) {
        av_bitstream_filter_close(bsf_ctx_);
        bsf_ctx_ = nullptr;
      }
      options_ = nullptr;
      fmt_ctx_ = nullptr;
    }
    first_frame_ = true;
    eos_reached_ = false;
    open_success_ = false;
  }

  int Parse() {
    std::unique_lock<std::mutex> guard(mutex_);
    if (eos_reached_ || !open_success_) {
      return -1;
    }
    while (true) {
      last_receive_frame_time_ = GetTickCount();
      if (av_read_frame(fmt_ctx_, &packet_) < 0) {
        if (result_) {
          result_->OnParserFrame(nullptr);
        }
        eos_reached_ = true;
        return -1;
      }

      if (packet_.stream_index != video_index_) {
        av_packet_unref(&packet_);
        continue;
      }

      AVStream *vstream = fmt_ctx_->streams[video_index_];
      if (first_frame_) {
        if (packet_.flags & AV_PKT_FLAG_KEY) {
          first_frame_ = false;
        } else {
          av_packet_unref(&packet_);
          continue;
        }
      }

      if (bsf_ctx_) {
        av_bitstream_filter_filter(bsf_ctx_, vstream->codec, NULL, &packet_.data, &packet_.size,
                                 packet_.data, packet_.size, 0);
      }
      // find pts information
      if (AV_NOPTS_VALUE == packet_.pts && find_pts_) {
        find_pts_ = false;
        // LOGW(SOURCE) << "Didn't find pts informations, "
        //              << "use ordered numbers instead. "
        //              << "stream url: " << url_name_.c_str();
      } else if (AV_NOPTS_VALUE != packet_.pts) {
        find_pts_ = true;
        packet_.pts = av_rescale_q(packet_.pts, vstream->time_base, {1, 90000});
      }
      if (!find_pts_) {
        packet_.pts = pts_++;  // FIXME
      }

      if (result_) {
        VideoEsFrame frame;
        frame.flags = packet_.flags;
        frame.data = packet_.data;
        frame.len = packet_.size;
        frame.pts = packet_.pts;
        if (!only_key_frame_ || (only_key_frame_ && (frame.flags & AV_PKT_FLAG_KEY))) {
          result_->OnParserFrame(&frame);
        }
      }
      if (bsf_ctx_) {
        av_freep(&packet_.data);
      }
      av_packet_unref(&packet_);
      return 0;
    }
  }

  std::string GetStreamID() { return stream_id_; }

 private:
  AVFormatContext* fmt_ctx_ = nullptr;
  AVBitStreamFilterContext *bsf_ctx_ = nullptr;
  AVDictionary* options_ = NULL;
  bool first_frame_ = true;
  int video_index_ = -1;
  uint64_t last_receive_frame_time_ = 0;
  uint8_t max_receive_time_out_ = 3;
  bool find_pts_ = false;
  uint64_t pts_ = 0;
  std::string stream_id_ = "";
  std::string url_name_;
  IParserResult *result_ = nullptr;
  AVPacket packet_;
  bool eos_reached_ = false;
  bool open_success_ = false;
  std::mutex mutex_;
  bool only_key_frame_ = false;
};  // class FFmpegDemuxerImpl  // NOLINT

FFParser::FFParser(const std::string& stream_id) {
  impl_ = new FFParserImpl(stream_id);
}

FFParser::~FFParser() {
  if (impl_) delete impl_, impl_ = nullptr;
}

int FFParser::Open(const std::string &url, IParserResult *result, bool only_key_frame) {
  if (impl_) {
    return impl_->Open(url, result, only_key_frame);
  }
  return -1;
}

void FFParser::Close() {
  if(impl_) {
    impl_->Close();
  }
}

int FFParser::Parse() {
  if(impl_) {
    impl_->Parse();
  }
  return -1;
}

std::string FFParser::GetStreamID() { return impl_->GetStreamID(); }

// H264/H265 ES Parser implementation
class EsParserImpl {
 public:
  EsParserImpl() = default;
  ~EsParserImpl() = default;
  int Open(AVCodecID codec_id, IParserResult *result, uint8_t *paramset = nullptr, uint32_t paramset_size = 0,
           bool only_key_frame = false) {
    std::unique_lock<std::mutex> guard(mutex_);
    codec_id_ = codec_id;
    result_ = result;
    if (codec_id != AV_CODEC_ID_H264 && codec_id !=  AV_CODEC_ID_HEVC) {
      return -1;
    }
    if (!result) {
      return -1;
    }

    // create parser...
    codec_ = avcodec_find_decoder(codec_id_);
    if (!codec_) {
      return -1;
    }
    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_){
      return -1;
    }
    codec_ctx_->time_base.den = 90000;
    codec_ctx_->time_base.num = 1;

    if (paramset && paramset_size) {
      paramset_ = std::vector<uint8_t>(paramset, paramset + paramset_size);
      codec_ctx_->extradata = paramset_.data();
      codec_ctx_->extradata_size = paramset_.size();
    }

    if (avcodec_open2(codec_ctx_, codec_, NULL) < 0) {
      return -1;
    }

    parser_ctx_ = av_parser_init(codec_id_);
    if (!parser_ctx_) {
      return -1;
    }
    frame_ = av_frame_alloc();
    if (!frame_) {
      return -1;
    }
    av_init_packet(&packet_);
    open_success_ = true;
    only_key_frame_ = only_key_frame;
    return 0;
  }
  void Close() {
    std::unique_lock<std::mutex> guard(mutex_);
    if (parser_ctx_) {
      av_parser_close(parser_ctx_);
      parser_ctx_ = nullptr;
    }
    if (frame_) {
      av_frame_free(&frame_);
      frame_ = nullptr;
    }
    if (codec_ctx_) {
      codec_ctx_->extradata = nullptr;
      codec_ctx_->extradata_size = 0;
      avcodec_close(codec_ctx_);
      av_free(codec_ctx_);
      codec_ctx_ = nullptr;
    }
    open_success_ = false;
  }
  void ParseEos();
  int Parse(const VideoEsPacket &pkt);

 private:
  AVCodecID codec_id_;
  IParserResult *result_;
  AVCodec *codec_ = nullptr;
  AVCodecContext *codec_ctx_ = nullptr;
  AVCodecParserContext *parser_ctx_ = nullptr;
  AVFrame *frame_ = nullptr;
  AVPacket packet_;
  std::vector<uint8_t> paramset_;
  bool first_time_ = true;
  bool open_success_ = false;
  std::mutex mutex_;
  bool only_key_frame_ = false;
};  // class StreamParserImpl

EsParser::EsParser() {
  impl_ = new EsParserImpl();
}

EsParser::~EsParser() {
  if (impl_) delete impl_;
}

int EsParser::Open(AVCodecID codec_id, IParserResult *result, uint8_t *paramset, uint32_t paramset_size,
                   bool only_key_frame) {
  if (impl_) {
    return impl_->Open(codec_id, result, paramset, paramset_size, only_key_frame);
  }
  return -1;
}

void EsParser::Close() {
  if (impl_) {
    impl_->Close();
  }
}

int EsParser::Parse(const VideoEsPacket &pkt) {
  if (impl_) {
    return impl_->Parse(pkt);
  }
  return -1;
}
int EsParser::ParseEos() {
  if (impl_) {
    impl_->ParseEos();
    return 0;
  }
  return -1;
}

inline void EsParserImpl::ParseEos() {
  if (result_) {
    VideoEsFrame frame;
    frame.data = nullptr;
    frame.len = 0;
    frame.pts = 0;
    result_->OnParserFrame(&frame);
  }
}

int EsParserImpl::Parse(const VideoEsPacket &pkt) {
  std::unique_lock<std::mutex> guard(mutex_);
  if (!open_success_) {
    ParseEos();
    return 0;
  }

  uint8_t *cur_ptr = pkt.data;
  int cur_size = pkt.len;

  do {
    int len = av_parser_parse2(parser_ctx_, codec_ctx_, &packet_.data, &packet_.size,
                               cur_ptr, cur_size, pkt.pts, AV_NOPTS_VALUE, AV_NOPTS_VALUE);
    cur_ptr += len;
    cur_size -= len;
    if (packet_.size == 0)
      continue;

    if (parser_ctx_->pict_type == AV_PICTURE_TYPE_I) {
      packet_.flags |= AV_PKT_FLAG_KEY;
    }
    packet_.pts = parser_ctx_->pts != AV_NOPTS_VALUE ? parser_ctx_->pts : parser_ctx_->last_pts;

    if (first_time_) {
      if (!(packet_.flags & AV_PKT_FLAG_KEY)) {
        av_packet_unref(&packet_);
        continue;
      }
      int got_picture = 0;
      int ret = avcodec_decode_video2(codec_ctx_, frame_, &got_picture, &packet_);
      if (ret < 0) {
        if (result_) {
          result_->OnParserInfo(nullptr);
        }
        av_packet_unref(&packet_);
        return ret;
      }
      cnstream::VideoInfo info;
      info.codec_id = codec_id_;
      switch (codec_ctx_->field_order) {
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

      // info.width = codec_ctx_->width;
      // info.height = codec_ctx_->height;

      info.extra_data = paramset_;

      if (result_) {
        result_->OnParserInfo(&info);
      }
      first_time_ = false;
    }

    if (result_) {
      VideoEsFrame frame;
      frame.data = packet_.data;
      frame.len = packet_.size;
      frame.pts = packet_.pts;
      frame.flags = packet_.flags;

      if (!only_key_frame_ || (only_key_frame_ && (frame.flags & AV_PKT_FLAG_KEY))) {
        result_->OnParserFrame(&frame);
      }
    }
    av_packet_unref(&packet_);
  } while (cur_size > 0);

  if (!pkt.data || !pkt.len) {
    ParseEos();
  }

  return 0;
}

}  // namespace cnstream

CNS_IGNORE_DEPRECATED_POP

