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
#include "data_handler_ffmpeg.hpp"
#include <future>
#include <sstream>
#include <thread>
#include <utility>
#include "cninfer/mlu_context.h"
namespace cnstream {

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

// FFMPEG use AVCodecParameters instead of AVCodecContext since from version 3.1(libavformat/version:57.40.100)
#define FFMPEG_VERSION_3_1 AV_VERSION_INT(57, 40, 100)

static uint64_t GetTickCount() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static int InterruptCallBack(void* ctx) {
  DataHandlerFFmpeg* pvideounpack = reinterpret_cast<DataHandlerFFmpeg*>(ctx);
  if (pvideounpack->CheckTimeOut(GetTickCount())) {
    return 1;
  }
  return 0;
}

bool DataHandlerFFmpeg::CheckTimeOut(uint64_t ul_current_time) {
  if ((ul_current_time - last_receive_frame_time_) / 1000 > max_receive_time_out_) {
    return true;
  }
  return false;
}

struct local_ffmpeg_init {
  local_ffmpeg_init() {
    avcodec_register_all();
    av_register_all();
    avformat_network_init();
  }
} init_ffmpeg;

bool DataHandlerFFmpeg::PrepareResources() {
  const char* p_rtmp_start_str = "rtmp://";
  // format context
  p_format_ctx_ = avformat_alloc_context();

  if (0 == strncasecmp(filename_.c_str(), p_rtmp_start_str, strlen(p_rtmp_start_str))) {
    AVIOInterruptCB intrpt_callback = {InterruptCallBack, this};
    p_format_ctx_->interrupt_callback = intrpt_callback;
    last_receive_frame_time_ = GetTickCount();
  }
  // options
  av_dict_set(&options_, "buffer_size", "1024000", 0);
  av_dict_set(&options_, "stimeout", "200000", 0);
  // open input
  int ret_code = avformat_open_input(&p_format_ctx_, filename_.c_str(), NULL, &options_);
  if (0 != ret_code) {
    LOG(ERROR) << "Couldn't open input stream.";
    return false;
  }
  // find video stream information
  ret_code = avformat_find_stream_info(p_format_ctx_, NULL);
  if (ret_code < 0) {
    LOG(ERROR) << "Couldn't find stream information.";
    return false;
  }
  video_index_ = -1;
  AVStream* vstream = nullptr;
  for (uint32_t loop_i = 0; loop_i < p_format_ctx_->nb_streams; loop_i++) {
    vstream = p_format_ctx_->streams[loop_i];
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
    if (vstream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
#else
    if (vstream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
#endif
      video_index_ = loop_i;
      break;
    }
  }
  if (video_index_ == -1) {
    LOG(ERROR) << "Didn't find a video stream.";
    return false;
  }
  // p_codec_ctx_ = vstream->codec;
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  AVCodecID codec_id = vstream->codecpar->codec_id;
#else
  AVCodecID codec_id = vstream->codec->codec_id;
#endif
  // bitstream filter
  bitstream_filter_ctx_ = nullptr;
  if (strstr(p_format_ctx_->iformat->name, "mp4") || strstr(p_format_ctx_->iformat->name, "flv") ||
      strstr(p_format_ctx_->iformat->name, "matroska") || strstr(p_format_ctx_->iformat->name, "rtsp")) {
    if (AV_CODEC_ID_H264 == codec_id) {
      bitstream_filter_ctx_ = av_bitstream_filter_init("h264_mp4toannexb");
    } else if (AV_CODEC_ID_HEVC == codec_id) {
      bitstream_filter_ctx_ = av_bitstream_filter_init("hevc_mp4toannexb");
    } else {
    }
  }

  av_init_packet(&packet_);
  packet_.data = NULL;
  packet_.size = 0;

  if (dev_ctx_.dev_id != DevContext::INVALID) {
    libstream::MluContext mlu_ctx;
    mlu_ctx.set_dev_id(dev_ctx_.dev_id);
    mlu_ctx.set_channel_id(dev_ctx_.ddr_channel);
    mlu_ctx.ConfigureForThisThread();
  }

  if (param_.decoder_type_ == DecoderType::DECODER_MLU) {
    decoder_ = std::make_shared<FFmpegMluDecoder>(*this);
  } else if (param_.decoder_type_ == DecoderType::DECODER_CPU) {
    decoder_ = std::make_shared<FFmpegCpuDecoder>(*this);
  } else {
    LOG(ERROR) << "unsupported decoder_type";
    return false;
  }
  if (decoder_.get()) {
    bool ret = decoder_->Create(vstream);
    if (ret) {
      decoder_->ResetCount(this->interval_);
      return true;
    }
    return false;
  }
  return false;
}

void DataHandlerFFmpeg::ClearResources() {
  if (decoder_.get()) {
    EnableFlowEos(true);
    decoder_->Destroy();
  }
  if (p_format_ctx_) {
    avformat_close_input(&p_format_ctx_);
    avformat_free_context(p_format_ctx_);
    av_dict_free(&options_);
    options_ = nullptr;
    p_format_ctx_ = nullptr;
  }
  if (bitstream_filter_ctx_) {
    av_bitstream_filter_close(bitstream_filter_ctx_);
    bitstream_filter_ctx_ = nullptr;
  }
  video_index_ = -1;
  first_frame_ = true;
}

bool DataHandlerFFmpeg::Extract() {
  while (true) {
    last_receive_frame_time_ = GetTickCount();

    if (av_read_frame(p_format_ctx_, &packet_) < 0) {
      return false;
    }

    if (packet_.stream_index != video_index_) {
      av_packet_unref(&packet_);
      continue;
    }

    AVStream* vstream = p_format_ctx_->streams[video_index_];

    if (first_frame_) {
      if (packet_.flags & AV_PKT_FLAG_KEY) {
        first_frame_ = false;
      } else {
        av_packet_unref(&packet_);
        continue;
      }
    }

    if (bitstream_filter_ctx_) {
      av_bitstream_filter_filter(bitstream_filter_ctx_, vstream->codec, NULL, &packet_.data, &packet_.size,
                                 packet_.data, packet_.size, 0);
    }

    // find pts information
    if (AV_NOPTS_VALUE == packet_.pts && find_pts_) {
      find_pts_ = false;
      LOG(WARNING) << "Didn't find pts informations, "
                   << "use ordered numbers instead. "
                   << "stream url: " << filename_.c_str();
    } else if (AV_NOPTS_VALUE != packet_.pts) {
      find_pts_ = true;
    }
    return true;
  }
}

bool DataHandlerFFmpeg::Process() {
  bool ret = Extract();
  if (!ret) {
    LOG(INFO) << "Read EOS from file";
    demux_eos_.store(1);
    if (this->loop_) {
      LOG(INFO) << "Clear resources and restart";
      EnableFlowEos(false);
      decoder_->Process(nullptr, true);
      ClearResources();
      PrepareResources();
      demux_eos_.store(0);
      LOG(INFO) << "Loop...";
      return true;
    } else {
      EnableFlowEos(true);
      decoder_->Process(nullptr, true);
      return false;
    }
  }  // if (!ret)
  if (!decoder_->Process(&packet_, false)) {
    av_packet_unref(&packet_);
    return false;
  }

  av_packet_unref(&packet_);
  return true;
}

}  // namespace cnstream
