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

#include <cnrt.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <map>
#include <utility>

#include "cnstream_common.hpp"
#include "cnstream_logging.hpp"
#include "video_decoder.hpp"

CNS_IGNORE_DEPRECATED_PUSH

namespace cnstream {

// FFMPEG use AVCodecParameters instead of AVCodecContext
// since from version 3.1(libavformat/version:57.40.100)
#define FFMPEG_VERSION_3_1 AV_VERSION_INT(57, 40, 100)

extern
Decoder* CreateMlu2xxDecoder(const std::string& stream_id, IDecodeResult *cb);

extern
Decoder* CreateMlu3xxDecoder(const std::string& stream_id, IDecodeResult *cb);

MluDecoder::MluDecoder(const std::string& stream_id, IDecodeResult *cb) : Decoder(stream_id, cb) {
}

MluDecoder::~MluDecoder() {
  if (impl_) {
    delete impl_;
    impl_ = nullptr;
  }
}

bool MluDecoder::Create(VideoInfo *info, ExtraDecoderInfo *extra) {
  if (impl_) {
    LOGW(SOURCE) << "[" << stream_id_ << "]: Decoder create duplicated.";
    return false;
  }
  cnrtDeviceInfo_t dev_info;
  cnrtRet_t cnrt_ret = cnrtGetDeviceInfo(&dev_info, extra->device_id);
  if (CNRT_RET_SUCCESS != cnrt_ret) {
    LOGE(SOURCE) << "Call cnrtGetDeviceInfo failed. ret = " << cnrt_ret;
    return false;
  }
  const std::string device_name(dev_info.device_name);
  if (std::string::npos != device_name.find("MLU3")) {
    impl_ = CreateMlu3xxDecoder(stream_id_, result_);
  } else if (std::string::npos != device_name.find("MLU270") || std::string::npos != device_name.find("MLU220")) {
    impl_ = CreateMlu2xxDecoder(stream_id_, result_);
  } else {
    LOGE(SOURCE) << "Device not supported yet, device name: " << device_name;
    return false;
  }
  if (nullptr == impl_) return false;
  LOGI(SOURCE) << "[" << stream_id_ << "]: Begin create decoder";
  bool ret = impl_->Create(info, extra);
  if (ret)
    LOGI(SOURCE) << "[" << stream_id_ << "]: Finish create decoder";
  else
    LOGE(SOURCE) << "[" << stream_id_ << "]: Create decoder failed";
  return ret;
}

void MluDecoder::Destroy() {
  if (impl_) {
    impl_->Destroy();
    delete impl_;
    impl_ = nullptr;
  }
}

bool MluDecoder::Process(VideoEsPacket *pkt) {
  if (impl_) {
    return impl_->Process(pkt);
  }
  return false;
}

//----------------------------------------------------------------------------
// CPU decoder
bool FFmpegCpuDecoder::Create(VideoInfo *info, ExtraDecoderInfo *extra) {
  AVCodec *dec = avcodec_find_decoder(info->codec_id);
  if (!dec) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "avcodec_find_decoder failed";
    return false;
  }
  instance_ = avcodec_alloc_context3(dec);
  if (!instance_) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "Failed to do avcodec_alloc_context3";
    return false;
  }
  // av_codec_set_pkt_timebase(instance_, st->time_base);

  if (!extra->extra_info.empty()) {
    instance_->extradata = extra->extra_info.data();
    instance_->extradata_size = extra->extra_info.size();
  }
// for usb camera
#ifdef HAVE_FFMPEG_AVDEVICE
  instance_->pix_fmt = AVPixelFormat(info->format);
  instance_->height = info->height;
  instance_->width = info->width;
#endif
  if (avcodec_open2(instance_, dec, NULL) < 0) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "Failed to open codec";
    return false;
  }
  av_frame_ = av_frame_alloc();
  if (!av_frame_) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "Could not alloc frame";
    return false;
  }
  eos_got_.store(0);
  eos_sent_.store(0);
  return true;
}

void FFmpegCpuDecoder::Destroy() {
  LOGI(SOURCE) << "[" << stream_id_ << "]: Begin destroy decoder";
  if (instance_ != nullptr) {
    if (!eos_sent_.load()) {
      while (this->Process(nullptr, true)) {
      }
    }
    while (!eos_got_.load()) {
      std::this_thread::yield();
    }
    avcodec_close(instance_), av_free(instance_);
    instance_ = nullptr;
  }
  if (av_frame_) {
    av_frame_free(&av_frame_);
    av_frame_ = nullptr;
  }
  LOGI(SOURCE) << "[" << stream_id_ << "]: Finish destroy decoder";
}

bool FFmpegCpuDecoder::Process(VideoEsPacket *pkt) {
  if (pkt && pkt->data && pkt->len) {
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = pkt->data;
    packet.size = pkt->len;
    packet.pts = pkt->pts;
    return Process(&packet, false);
  }
  return Process(nullptr, true);
}

bool FFmpegCpuDecoder::Process(AVPacket *pkt, bool eos) {
  if (eos) {
    AVPacket packet;
    av_init_packet(&packet);
    packet.size = 0;
    packet.data = NULL;

    LOGI(SOURCE) << "[" << stream_id_ << "]: Sent EOS packet to decoder";
    eos_sent_.store(1);
    // flush all frames ...
    int got_frame = 0;
    do {
      avcodec_decode_video2(instance_, av_frame_, &got_frame, &packet);
      if (got_frame) ProcessFrame(av_frame_);
    } while (got_frame);

    if (result_) {
      result_->OnDecodeEos();
    }
    eos_got_.store(1);
    return false;
  }
  int got_frame = 0;
  int ret = avcodec_decode_video2(instance_, av_frame_, &got_frame, pkt);
  if (ret < 0) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "avcodec_decode_video2 failed, data ptr, size:" << pkt->data << ", " << pkt->size;
    return true;
  }
  if (got_frame) {
    ProcessFrame(av_frame_);
  }
  return true;
}


bool FFmpegCpuDecoder::ProcessFrame(AVFrame *frame) {
  if (instance_->pix_fmt != AV_PIX_FMT_YUV420P && instance_->pix_fmt != AV_PIX_FMT_YUVJ420P &&
      instance_->pix_fmt != AV_PIX_FMT_YUYV422) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "FFmpegCpuDecoder only supports AV_PIX_FMT_YUV420P , AV_PIX_FMT_YUVJ420P and AV_PIX_FMT_YUYV422";
    return false;
  }
  DecodeFrame cn_frame;
  cn_frame.valid = true;
  cn_frame.width =  frame->width;
  cn_frame.height =  frame->height;
#if LIBAVFORMAT_VERSION_INT <= FFMPEG_VERSION_3_1
  cn_frame.pts = frame->pkt_pts;
#else
  cn_frame.pts = frame->pts;
#endif
  switch (instance_->pix_fmt) {
    case AV_PIX_FMT_YUV420P:
      cn_frame.fmt = DecodeFrame::PixFmt::FMT_I420;
      cn_frame.planeNum = 3;
      break;
    case AV_PIX_FMT_YUVJ420P:
      cn_frame.fmt = DecodeFrame::PixFmt::FMT_J420;
      cn_frame.planeNum = 3;
      break;
    case AV_PIX_FMT_YUYV422:
      cn_frame.fmt = DecodeFrame::PixFmt::FMT_YUYV;
      cn_frame.planeNum = 1;
      break;
    default:
      cn_frame.fmt = DecodeFrame::PixFmt::FMT_INVALID;
      cn_frame.planeNum = 0;
      break;
  }
  cn_frame.mlu_addr = false;
  for (int i = 0; i < cn_frame.planeNum; i++) {
    cn_frame.stride[i] = frame->linesize[i];
    cn_frame.plane[i] = frame->data[i];
  }
  cn_frame.buf_ref = nullptr;
  if (result_) {
    result_->OnDecodeFrame(&cn_frame);
  }
  return true;
}

}  // namespace cnstream

CNS_IGNORE_DEPRECATED_POP

