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

#include "cndecode/cndecode.h"
#include "cninfer/mlu_context.h"
#include "fr_controller.hpp"

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

bool DataHandlerFFmpeg::Open() {
  if (!this->module_) {
    return false;
  }

  // default value
  dev_ctx_.dev_type = DevContext::MLU;
  dev_ctx_.dev_id = 0;

  // updated with paramSet
  ModuleParamSet param_set = module_->ParamSet();
  if (param_set.find("decoder_type") != param_set.end()) {
    std::string dec_type = param_set["decoder_type"];
    if (dec_type == "mlu") {
      dev_ctx_.dev_type = DevContext::MLU;
      dev_ctx_.dev_id = 0;
      if (param_set.find("device_id") != param_set.end()) {
        std::stringstream ss;
        int device_id;
        ss << param_set["device_id"];
        ss >> device_id;
        dev_ctx_.dev_id = device_id;
      }
    } else {
      LOG(ERROR) << "decoder_type " << param_set["decoder_type"] << "not supported";
      return false;
    }
  }
  chn_idx_ = this->GetStreamIndex();
  if (chn_idx_ == DataHandler::INVALID_STREAM_ID) {
    return false;
  }
  dev_ctx_.ddr_channel = chn_idx_ % 4;

  // start demuxer
  running_.store(1);
  thread_ = std::move(std::thread(&DataHandlerFFmpeg::ExtractingLoop, this));
  return true;
}

void DataHandlerFFmpeg::Close() {
  if (running_.load()) {
    running_.store(0);
    if (thread_.joinable()) {
      thread_.join();
    }
  }
}

bool DataHandlerFFmpeg::SendPacket(const libstream::CnPacket& packet, bool eos) {
  LOG_IF(INFO, eos) << "[Decoder] stream_id " << stream_id_ << " send eos.";
  try {
    if (instance_->SendData(packet, eos)) {
      return true;
    }
  } catch (libstream::StreamlibsError& e) {
    LOG(ERROR) << "[Decoder] " << e.what();
    return false;
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

static std::mutex decoder_mutex;

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

  // create decoder
  libstream::CnDecode::Attr instance_attr;
  memset(&instance_attr, 0, sizeof(instance_attr));
  // common attrs
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  instance_attr.maximum_geometry.w = p_format_ctx_->streams[video_index_]->codecpar->width;
  instance_attr.maximum_geometry.h = p_format_ctx_->streams[video_index_]->codecpar->height;
#else
  instance_attr.maximum_geometry.w = p_format_ctx_->streams[video_index_]->codec->width;
  instance_attr.maximum_geometry.h = p_format_ctx_->streams[video_index_]->codec->height;
#endif
  switch (codec_id) {
    case AV_CODEC_ID_H264:
      instance_attr.codec_type = libstream::H264;
      break;
    case AV_CODEC_ID_HEVC:
      instance_attr.codec_type = libstream::H265;
      break;
    case AV_CODEC_ID_MJPEG:
      instance_attr.codec_type = libstream::JPEG;
      break;
    default: {
      LOG(ERROR) << "codec type not supported yet, codec_id = " << codec_id;
      return false;
    }
  }
  instance_attr.pixel_format = libstream::YUV420SP_NV21;
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  instance_attr.output_geometry.w = p_format_ctx_->streams[video_index_]->codecpar->width;
  instance_attr.output_geometry.h = p_format_ctx_->streams[video_index_]->codecpar->height;
#else
  instance_attr.output_geometry.w = p_format_ctx_->streams[video_index_]->codec->width;
  instance_attr.output_geometry.h = p_format_ctx_->streams[video_index_]->codec->height;
#endif
  instance_attr.drop_rate = 0;
  instance_attr.frame_buffer_num = 3;
  instance_attr.dev_id = dev_ctx_.dev_id;
  instance_attr.video_mode = libstream::FRAME_MODE;
  instance_attr.silent = false;

  // callbacks
  instance_attr.frame_callback = std::bind(&DataHandlerFFmpeg::FrameCallback, this, std::placeholders::_1);
  instance_attr.perf_callback = std::bind(&DataHandlerFFmpeg::PerfCallback, this, std::placeholders::_1);
  instance_attr.eos_callback = std::bind(&DataHandlerFFmpeg::EOSCallback, this);

  // create CnDecode
  try {
    std::unique_lock<std::mutex> lock(decoder_mutex);
    if (instance_ != nullptr) {
      delete instance_, instance_ = nullptr;
    }
    eos_got_.store(0);
    instance_ = libstream::CnDecode::Create(instance_attr);
  } catch (libstream::StreamlibsError& e) {
    LOG(ERROR) << "[Decoder] " << e.what();
    return false;
  }
  return true;
}

void DataHandlerFFmpeg::ClearResources() {
  if (instance_ != nullptr) {
    while (!eos_got_.load()) {
      usleep(1000 * 10);
    }
    eos_got_.store(0);
    delete instance_, instance_ = nullptr;
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

bool DataHandlerFFmpeg::Extract(libstream::CnPacket* pdata) {
  while (true) {
    last_receive_frame_time_ = GetTickCount();

    if (av_read_frame(p_format_ctx_, &packet_) < 0) {
      pdata->length = 0;
      return false;
    }

    if (packet_.stream_index == video_index_) {
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
        av_bitstream_filter_filter(bitstream_filter_ctx_, vstream->codec, NULL,
                                   reinterpret_cast<uint8_t**>(&pdata->data), reinterpret_cast<int*>(&pdata->length),
                                   packet_.data, packet_.size, 0);
      } else {
        pdata->data = packet_.data;
        pdata->length = packet_.size;
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
      pdata->pts = packet_.pts;
      return true;
    }
  }
}

void DataHandlerFFmpeg::ReleaseData(libstream::CnPacket* pdata) {
  if (bitstream_filter_ctx_) {
    av_free(pdata->data);
  }
  av_packet_unref(&packet_);
}

void DataHandlerFFmpeg::ExtractingLoop() {
  libstream::MluContext mlu_ctx;
  mlu_ctx.set_dev_id(dev_ctx_.dev_id);
  mlu_ctx.set_channel_id(dev_ctx_.ddr_channel);
  mlu_ctx.ConfigureForThisThread();

  libstream::CnPacket pic;
  if (!PrepareResources()) {
    return;
  }
  bool bEOS = false;
  FrController controller(frame_rate_);
  if (frame_rate_ > 0) controller.Start();
  while (running_.load()) {
    bool ret = Extract(&pic);
    if (!ret) {
      LOG(INFO) << "Read EOS from file";
      if (this->loop_) {
        LOG(INFO) << "Clear resources and restart";
        send_flow_eos_.store(0);
        SendPacket(pic, true);
        ClearResources();
        ReleaseData(&pic);
        PrepareResources();
        frame_id_ = 0;
        LOG(INFO) << "Loop...";
        continue;
      } else {
        bEOS = true;
        send_flow_eos_.store(1);
        if (!SendPacket(pic, bEOS)) {
          break;
        }
        break;
      }
    }  // if (!ret)

    if (!SendPacket(pic, bEOS)) {
      ReleaseData(&pic);
      break;
    }

    if (bEOS) break;
    ReleaseData(&pic);
    if (frame_rate_ > 0) controller.Control();
  }
  if (!bEOS) {
    send_flow_eos_.store(1);
    SendPacket(pic, true);
  }
  ClearResources();
}

static CNDataFormat CnPixelFormat2CnDataFormat(libstream::CnPixelFormat pformat) {
  switch (pformat) {
    case libstream::YUV420SP_NV12:
      return CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12;
    case libstream::YUV420SP_NV21:
      return CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
    case libstream::RGB24:
      return CNDataFormat::CN_PIXEL_FORMAT_RGB24;
    case libstream::BGR24:
      return CNDataFormat::CN_PIXEL_FORMAT_BGR24;
    default:
      return CNDataFormat::CN_INVALID;
  }
  return CNDataFormat::CN_INVALID;
}
void DataHandlerFFmpeg::FrameCallback(const libstream::CnFrame& frame) {
  if (DevContext::MLU != dev_ctx_.dev_type) {
    LOG(FATAL) << "Unsupported!!!";
    return;
  }
  auto data = CNFrameInfo::Create(stream_id_);
  if (data == nullptr) {
    LOG(WARNING) << "CNFrameInfo::Create Failed,DISCARD image";
    return;
  }

  void* frame_data[CN_MAX_PLANES];
  if (frame.planes > CN_MAX_PLANES) {
    LOG(FATAL) << "planes invalid!!!";
    return;
  }
  for (uint32_t pi = 0; pi < frame.planes; ++pi) {
    frame_data[pi] = reinterpret_cast<void*>(frame.data.ptrs[pi]);
  }
  data->frame.CopyFrameFromMLU(dev_ctx_.dev_id, dev_ctx_.ddr_channel, CnPixelFormat2CnDataFormat(frame.pformat),
                               frame.width, frame.height, frame_data, frame.strides);

  // frame position
  data->channel_idx = chn_idx_;
  data->frame.frame_id = frame_id_++;
  data->frame.timestamp = frame.pts;
  if (this->module_) {
    this->module_->SendData(data);
  }
  instance_->ReleaseBuffer(frame.buf_id);
}

void DataHandlerFFmpeg::EOSCallback() {
  auto data = CNFrameInfo::Create(stream_id_);
  data->channel_idx = chn_idx_;
  data->frame.flags |= CNFrameFlag::CN_FRAME_FLAG_EOS;
  LOG(INFO) << "[Decoder]  " << stream_id_ << " receive eos.";
  if (this->module_ && send_flow_eos_.load()) {
    this->module_->SendData(data);
  }
  eos_got_.store(1);
}

}  // namespace cnstream
