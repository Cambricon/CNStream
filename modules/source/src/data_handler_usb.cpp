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

#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "data_handler_usb.hpp"

namespace cnstream {

std::shared_ptr<SourceHandler> UsbHandler::Create(DataSource *module, const std::string &stream_id,
                                                   const std::string &filename, int framerate, bool loop) {
  if (!module || stream_id.empty() || filename.empty()) {
    return nullptr;
  }
  std::shared_ptr<UsbHandler> handler(new (std::nothrow) UsbHandler(module, stream_id, filename, framerate, loop));
  return handler;
}

UsbHandler::UsbHandler(DataSource *module, const std::string &stream_id, const std::string &filename, int framerate,
                         bool loop)
    : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) UsbHandlerImpl(module, filename, framerate, loop, *this);
}

UsbHandler::~UsbHandler() {
  if (impl_) {
    delete impl_;
  }
}

bool UsbHandler::Open() {
  if (!this->module_) {
    LOG(ERROR) << "module_ null";
    return false;
  }
  if (!impl_) {
    LOG(ERROR) << "impl_ null";
    return false;
  }

  if (stream_index_ == cnstream::INVALID_STREAM_IDX) {
    LOG(ERROR) << "invalid stream_idx";
    return false;
  }

  return impl_->Open();
}

void UsbHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

bool UsbHandlerImpl::Open() {
  // updated with paramSet
  DataSource *source = dynamic_cast<DataSource *>(module_);
  param_ = source->GetSourceParam();
  this->interval_ = param_.interval_;
  perf_manager_ = source->GetPerfManager(stream_id_);
  // start demuxer
  running_.store(1);
  thread_ = std::move(std::thread(&UsbHandlerImpl::Loop, this));
  return true;
}

void UsbHandlerImpl::Close() {
  if (running_.load()) {
    running_.store(0);
    if (thread_.joinable()) {
      thread_.join();
    }
  }
}

void UsbHandlerImpl::Loop() {
  /*meet cnrt requirement*/
  if (param_.device_id_ >= 0) {
    try {
      edk::MluContext mlu_ctx;
      mlu_ctx.SetDeviceId(param_.device_id_);
      // mlu_ctx.SetChannelId(dev_ctx_.ddr_channel);
      mlu_ctx.ConfigureForThisThread();
    } catch (edk::Exception &e) {
      if (nullptr != module_)
        module_->PostEvent(EVENT_ERROR, "stream_id " + stream_id_ + " failed to setup dev/channel.");
      return;
    }
  }

  if (!PrepareResources()) {
    if (nullptr != module_)
      module_->PostEvent(EVENT_ERROR, "stream_id " + stream_id_ + "Prepare codec resources failed.");
    return;
  }

  while (running_.load()) {
    if (!Process()) {
      break;
    }
  }

  ClearResources();
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
//
// FFMPEG use AVCodecParameters instead of AVCodecContext
// since from version 3.1(libavformat/version:57.40.100)
//
#define FFMPEG_VERSION_3_1 AV_VERSION_INT(57, 40, 100)

bool UsbHandlerImpl::PrepareResources(bool demux_only) {
  // format context
  p_format_ctx_ = avformat_alloc_context();
  if (!p_format_ctx_) {
      return false;
  }
  // check usb input name
  const char* usb_prefix = "/dev/video";
  if (0 != strncasecmp(filename_.c_str(), usb_prefix, strlen(usb_prefix))) {
    LOG(ERROR) << "Couldn't open input stream.";
    return false;
  }

  // open v4l2 input
  AVInputFormat* ifmt = NULL;
#if defined(__linux) || defined(__unix)
  ifmt = av_find_input_format("video4linux2");
  if (!ifmt) {
    LOG(ERROR) << "Could not find v4l2 format.";
    return false;
  }
#elif defined(_WIN32) || defined(_WIN64)
  ifmt = av_find_input_format("dshow");
  if (!ifmt) {
    LOG(ERROR) << "Could not find dshow.";
    return false;
  }
#endif
  int ret_code = avformat_open_input(&p_format_ctx_, filename_.c_str(), ifmt, &options_);
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
  AVStream *vstream = nullptr;
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
      strstr(p_format_ctx_->iformat->name, "matroska")) {
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

  if (demux_only) return true;

  if (param_.decoder_type_ == DecoderType::DECODER_MLU) {
    decoder_ = std::make_shared<MluDecoder>(this);
  } else if (param_.decoder_type_ == DecoderType::DECODER_CPU) {
    decoder_ = std::make_shared<FFmpegCpuDecoder>(this);
  } else {
    LOG(ERROR) << "unsupported decoder_type";
    return false;
  }
  if (decoder_.get()) {
    bool ret = decoder_->Create(vstream, interval_);
    if (ret) {
      return true;
    }
    return false;
  }
  return false;
}

void UsbHandlerImpl::ClearResources(bool demux_only) {
  if (!demux_only && decoder_.get()) {
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

bool UsbHandlerImpl::Extract() {
  while (true) {
    if (av_read_frame(p_format_ctx_, &packet_) < 0) {
      return false;
    }

    if (packet_.stream_index != video_index_) {
      av_packet_unref(&packet_);
      continue;
    }

    AVStream *vstream = p_format_ctx_->streams[video_index_];
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
      packet_.pts = av_rescale_q(packet_.pts, vstream->time_base, {1, 90000});
    }

    if (!find_pts_) {
      packet_.pts = pts_++;
    }
    return true;
  }
}

bool UsbHandlerImpl::Process() {
  bool ret = Extract();

  if (perf_manager_ != nullptr) {
    std::string thread_name = "cn-" + module_->GetName() + stream_id_;
    perf_manager_->Record(false, PerfManager::GetDefaultType(), module_->GetName(), packet_.pts);
    perf_manager_->Record(PerfManager::GetDefaultType(), PerfManager::GetPrimaryKey(), std::to_string(packet_.pts),
                          module_->GetName() + "_th", "'" + thread_name + "'");
  }

  if (!ret) {
    LOG(INFO) << "Read EOS from file";
    if (this->loop_) {
      LOG(INFO) << "Clear resources and restart";
      ClearResources(true);
      if (!PrepareResources(true)) {
        if (nullptr != module_) module_->PostEvent(EVENT_ERROR, "Prepare codec resources failed");
        return false;
      }
      LOG(INFO) << "Loop...";
      return true;
    } else {
      decoder_->Process(nullptr, true);
      return false;
    }
  }  // if (!ret)
  if (!decoder_->Process(&packet_, false)) {
    if (bitstream_filter_ctx_) {
      av_freep(&packet_.data);
    }
    av_packet_unref(&packet_);
    return false;
  }

  if (bitstream_filter_ctx_) {
    av_freep(&packet_.data);
  }
  av_packet_unref(&packet_);
  return true;
}

}  // namespace cnstream
