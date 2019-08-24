/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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

#include <future>
#include <thread>
#include <utility>

#include "fr_controller.hpp"
#include "video_src.hpp"

using libstream::CnPacket;

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

static uint64_t GetTickCount() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

bool VideoSrc::CheckTimeOut(uint64_t ul_current_time) {
  if ((ul_current_time - last_receive_frame_time_) / 1000 > max_receive_time_out_) {
    return true;
  }
  return false;
}

static int InterruptCallBack(void* ctx) {
  VideoSrc* pvideounpack = reinterpret_cast<VideoSrc*>(ctx);
  if (pvideounpack->CheckTimeOut(GetTickCount())) {
    return 1;
  }
  return 0;
}

bool VideoSrc::Open() {
  running_ = true;
  resolution_promise_ = std::unique_ptr<std::promise<cv::Size>>(new std::promise<cv::Size>);
  thread_ = std::move(std::thread(&VideoSrc::ExtractingLoop, this));
  return true;
}

void VideoSrc::Close() {
  running_ = false;
  if (thread_.joinable()) {
    thread_.join();
  }
  ClearResources();
}

bool VideoSrc::PrepareResources() {
  // init ffmpeg
  avcodec_register_all();
  av_register_all();
  avformat_network_init();
  const char* p_rtmp_start_str = "rtmp://";
  // format context
  p_format_ctx_ = avformat_alloc_context();

  if (0 == strncasecmp(GetUrl().c_str(), p_rtmp_start_str, strlen(p_rtmp_start_str))) {
    AVIOInterruptCB intrpt_callback = {InterruptCallBack, this};
    p_format_ctx_->interrupt_callback = intrpt_callback;
    last_receive_frame_time_ = GetTickCount();
  }
  // options
  av_dict_set(&options_, "buffer_size", "1024000", 0);
  av_dict_set(&options_, "stimeout", "200000", 0);
  // open input
  int ret_code = avformat_open_input(&p_format_ctx_, GetUrl().c_str(), NULL, &options_);
  if (0 != ret_code) {
    resolution_promise_->set_exception(std::make_exception_ptr(std::runtime_error("couldn't open input stream")));
    LOG(ERROR) << "Couldn't open input stream.";
    return false;
  }
  // find video stream information
  ret_code = avformat_find_stream_info(p_format_ctx_, NULL);
  if (ret_code < 0) {
    resolution_promise_->set_exception(std::make_exception_ptr(std::runtime_error("couldn't find stream information")));
    LOG(ERROR) << "Couldn't find stream information.";
    return false;
  }
  video_index_ = -1;
  AVStream* vstream = nullptr;
  for (uint32_t loop_i = 0; loop_i < p_format_ctx_->nb_streams; loop_i++) {
    vstream = p_format_ctx_->streams[loop_i];
    if (vstream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_index_ = loop_i;
      break;
    }
  }
  if (video_index_ == -1) {
    resolution_promise_->set_exception(std::make_exception_ptr(std::runtime_error("didn't find a video stream")));
    LOG(ERROR) << "Didn't find a video stream.";
    return false;
  }
  // p_codec_ctx_ = vstream->codec;
  AVCodecID codec_id = vstream->codecpar->codec_id;
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
  // get resolution
  int width = p_format_ctx_->streams[video_index_]->codecpar->width;
  int height = p_format_ctx_->streams[video_index_]->codecpar->height;
  resolution_.width = width;
  resolution_.height = height;
  if (!resolution_promise_) return true;
  try {
    resolution_promise_->set_value(resolution_);
  } catch (std::future_error& e) {
    LOG(ERROR) << e.what();
    return false;
  }
  return true;
}

void VideoSrc::ClearResources() {
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
  resolution_.width = 0;
  resolution_.height = 0;
}

bool VideoSrc::Extract(CnPacket* pdata) {
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
                     << "stream url: " << GetUrl().c_str();
      } else if (AV_NOPTS_VALUE != packet_.pts) {
        find_pts_ = true;
      }
      // set frame id
      if (find_pts_) {
        if (packet_.pts && packet_.pts == static_cast<int64_t>(GetFrameIndex())) {
          LOG(WARNING) << "Lose frame caused by lost pts informations.";
        }
        if (packet_.duration) SetFrameIndex(packet_.pts / packet_.duration);
      }
      pdata->pts = GetFrameIndex();
      SetFrameIndex(GetFrameIndex() + 1);
      return true;
    }
  }
}

void VideoSrc::ReleaseData(CnPacket* pdata) {
  if (bitstream_filter_ctx_) {
    av_free(pdata->data);
  }
  av_packet_unref(&packet_);
}

void VideoSrc::ExtractingLoop() {
  CnPacket pic;
  if (!PrepareResources()) {
    if (GetCallback()) {
      GetCallback()(pic, true);
    }
    return;
  }
  bool bEOS = false;
  FrController controller(GetFrameRate());
  controller.Start();
  while (running_) {
    bool ret = Extract(&pic);
    if (!ret) {
      LOG(INFO) << "Read EOS from file";
      if (IsLoop()) {
        LOG(INFO) << "Clear resources and restart";
        ClearResources();
        ReleaseData(&pic);
        PrepareResources();
        SetFrameIndex(0);
        LOG(INFO) << "Loop...";
        continue;
      } else {
        bEOS = true;
        if (GetCallback()) {
          if (!GetCallback()(pic, bEOS)) {
            break;
          }
        }
        break;
      }
    }  // if (!ret)

    if (GetCallback()) {
      if (!GetCallback()(pic, bEOS)) {
        ReleaseData(&pic);
        break;
      }
    }
    if (bEOS) break;
    ReleaseData(&pic);
    controller.Control();
  }
}
