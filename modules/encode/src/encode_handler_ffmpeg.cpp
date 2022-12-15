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

#include "encode_handler_ffmpeg.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include "cnedk_buf_surface_util.hpp"

#include "cnstream_logging.hpp"
#include "cnstream_frame_va.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace cnstream {

static const char *ct_str[] = {"H264", "H265", "MPEG4", "JPEG"};

#define SPECIFIC_CODEC

#define VERSION_LAVC_ALLOC_PACKET AV_VERSION_INT(57, 20, 102)

VEncodeFFmpegHandler::VEncodeFFmpegHandler() {
  // LOGI(VEncodeFFmpegHandler) << "VEncodeFFmpegHandler(" << param.width << "x" << param.height << ", " <<
  //     pf_str[param_.pixel_format] << ", " << ct_str[param_.codec_type] << ")";
  avcodec_register_all();
}

VEncodeFFmpegHandler::~VEncodeFFmpegHandler() {
  if (eos_promise_) {
    eos_promise_->get_future().wait();
    eos_promise_.reset(nullptr);
  }
  VEncodeFFmpegHandler::Stop();
}

int VEncodeFFmpegHandler::SendFrame(std::shared_ptr<CNFrameInfo> data) {
  if (data == nullptr || data->IsEos()) {
    if (!eos_promise_) {
      eos_promise_.reset(new std::promise<void>);
    }
    data_queue_.Push(nullptr);
    return 0;
  }

  CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);

  std::unique_lock<std::mutex> guard(mutex_);
  if (!inited_) {
    Init();
    inited_ = true;
  }
  guard.unlock();

  return SendFrame(frame, 5000);
}

int VEncodeFFmpegHandler::Init() {
  int ret = 0;
  const char *codec_name;

  param_.width = param_.width % 2 ? param_.width - 1 : param_.width;
  param_.height = param_.height % 2 ? param_.height - 1 : param_.height;
  param_.frame_rate = param_.frame_rate > 0 ? param_.frame_rate : 30;
  param_.frame_rate = param_.frame_rate < 120 ? param_.frame_rate : 120;
  // param_.time_base = param_.time_base > 0 ? param_.time_base : 1000;
  param_.bitrate = param_.bitrate < 0x40000 ? 0x40000 : param_.bitrate;
  param_.gop_size = param_.gop_size < 8 ? 8 : param_.gop_size;

  switch (param_.codec_type) {
    case VideoCodecType::AUTO:
    case VideoCodecType::H264:
      av_codec_id_ = AV_CODEC_ID_H264;
      codec_name = "libx264";
      break;
    case VideoCodecType::H265:
      av_codec_id_ = AV_CODEC_ID_HEVC;
      codec_name = "libx265";
      break;
    case VideoCodecType::MPEG4:
      av_codec_id_ = AV_CODEC_ID_MPEG4;
      codec_name = "mpeg4";
      break;
    case VideoCodecType::JPEG:
      av_codec_id_ = AV_CODEC_ID_MJPEG;
      codec_name = "mjpeg";
      break;
    default:
      LOGE(VEncodeFFmpegHandler) << "Start() unsupported codec type: " << ct_str[param_.codec_type];
      return -1;
  }

  av_codec_ = avcodec_find_encoder_by_name(codec_name);

  if (!av_codec_) {
    LOGE(VEncodeFFmpegHandler) << "Start() avcodec_find_encoder \"" << codec_name << "\" failed";
    Destroy();
    return -1;
  }

  av_codec_ctx_ = avcodec_alloc_context3(av_codec_);
  if (!av_codec_ctx_)  {
    LOGE(VEncodeFFmpegHandler) << "Could not allocate video codec context";
    return -1;
  }

  av_codec_ctx_->codec_id = av_codec_id_;
  av_codec_ctx_->width = param_.width;
  av_codec_ctx_->height = param_.height;
  av_codec_ctx_->framerate = av_d2q(param_.frame_rate, 60000);
  av_codec_ctx_->time_base.num = av_codec_ctx_->framerate.den;
  av_codec_ctx_->time_base.den = av_codec_ctx_->framerate.num;
  av_codec_ctx_->bit_rate = param_.bitrate;
  av_codec_ctx_->gop_size = param_.gop_size;
  av_codec_ctx_->pix_fmt = av_codec_id_ == AV_CODEC_ID_MJPEG ? AV_PIX_FMT_YUVJ420P : AV_PIX_FMT_YUV420P;
  av_codec_ctx_->max_b_frames = av_codec_id_ == AV_CODEC_ID_MJPEG ? 0 : 1;

  if (!strcmp(av_codec_->name, "libx264") || !strcmp(av_codec_->name, "libx265")) {
    av_dict_set(&av_opts_, "preset", "superfast", 0);
    av_dict_set(&av_opts_, "tune", "zerolatency", 0);
    if (av_codec_id_ == AV_CODEC_ID_H264) {
      av_dict_set(&av_opts_, "profile", "high", 0);
      av_dict_set(&av_opts_, "level", "5.1", 0);
    } else {
      av_dict_set(&av_opts_, "level-idc", "5.1", 0);
      av_dict_set(&av_opts_, "high-tier", "true", 0);
    }
  }
  ret = avcodec_open2(av_codec_ctx_, av_codec_, &av_opts_);
  if (ret < 0) {
    LOGE(VEncodeFFmpegHandler) << "Start() avcodec_open2 failed, ret=" << ret;
    Destroy();
    return -1;
  }

  av_opt_set(av_codec_ctx_->priv_data, "tune", "zerolatency", 0);
  av_opt_set(av_codec_ctx_->priv_data, "preset", "superfast", 0);

#if LIBAVCODEC_VERSION_INT < VERSION_LAVC_ALLOC_PACKET
  av_packet_ = reinterpret_cast<AVPacket *>(av_mallocz(sizeof(AVPacket)));
#else
  av_packet_ = av_packet_alloc();
#endif

  memset(av_packet_, 0, sizeof(AVPacket));
  av_init_packet(av_packet_);
  state_ = RUNNING;
  thread_ = std::thread(&VEncodeFFmpegHandler::Loop, this);
  return 0;
}

int VEncodeFFmpegHandler::Stop() {
  state_ = STOPPING;

  if (thread_.joinable()) thread_.join();

  AVFrame *frame;
  while (data_queue_.Size()) {
    data_queue_.WaitAndPop(frame);
    if (frame) av_frame_free(&frame);
  }
  Destroy();

  return 0;
}

int VEncodeFFmpegHandler::SendFrame(Scaler::Buffer* data) {
  if (data == nullptr) {
    if (!eos_promise_) eos_promise_.reset(new std::promise<void>);
    data_queue_.Push(nullptr);
    return 0;
  }

  std::unique_lock<std::mutex> guard(mutex_);
  if (!inited_) {
    Init();
    inited_ = true;
  }
  guard.unlock();

  AVFrame *avframe = nullptr;

  avframe = av_frame_alloc();
  if (!avframe) {
    LOGE(VEncodeFFmpegHandler) << "alloc frame failed";
    return -1;
  }

  avframe->width = param_.width;
  avframe->height = param_.height;
  avframe->format = av_pixel_format_;

  int ret = av_frame_get_buffer(avframe, input_alignment_);
  if (ret < 0) {
    LOGE(VEncodeFFmpegHandler) << "RequestFrameBuffer() av_frame_get_buffer failed, ret=" << ret;
    av_frame_free(&avframe);
    return -1;
  }

  // transform src to dst
  {
    Scaler::Buffer dst_buf;
    dst_buf.color = Scaler::ColorFormat::YUV_I420;
    dst_buf.data[0] = avframe->data[0];
    dst_buf.data[1] = avframe->data[1];
    dst_buf.data[2] = avframe->data[2];
    dst_buf.width = avframe->width;
    dst_buf.height = avframe->height;
    dst_buf.stride[0] = avframe->linesize[0];
    dst_buf.stride[1] = avframe->linesize[1];
    dst_buf.stride[2] = avframe->linesize[2];

    if (!Scaler::Process(data, &dst_buf, nullptr, nullptr, Scaler::Carrier::LIBYUV)) {
      LOGE(VEncodeFFmpegHandler) << "Encode() scaler process 1 failed";
      av_frame_free(&avframe);
      return -1;
    }
  }
  data_queue_.Push(avframe);

  return 0;
}

int VEncodeFFmpegHandler::SendFrame(CNDataFramePtr frame, int timeout_ms) {
  // send normal frame
  AVFrame *avframe = nullptr;

  avframe = av_frame_alloc();
  if (!avframe) {
    LOGE(VEncodeFFmpegHandler) << "alloc frame failed";
    return -1;
  }

  avframe->width = param_.width;
  avframe->height = param_.height;
  avframe->format = av_pixel_format_;

  int ret = av_frame_get_buffer(avframe, input_alignment_);
  if (ret < 0) {
    LOGE(VEncodeFFmpegHandler) << "RequestFrameBuffer() av_frame_get_buffer failed, ret=" << ret;
    av_frame_free(&avframe);
    return -1;
  }

  cnedk::BufSurfWrapperPtr surf = frame->buf_surf;

  avframe->pts = (surf->GetPts() == INVALID_TIMESTAMP ? AV_NOPTS_VALUE : surf->GetPts());
  avframe->pkt_pts = avframe->pts;

  // transform src to dst
  {
    Scaler::Buffer src_buf;
    Scaler::MatToBuffer(frame->ImageBGR(), Scaler::ColorFormat::BGR, &src_buf);

    Scaler::Buffer dst_buf;
    dst_buf.color = Scaler::ColorFormat::YUV_I420;
    dst_buf.data[0] = avframe->data[0];
    dst_buf.data[1] = avframe->data[1];
    dst_buf.data[2] = avframe->data[2];
    dst_buf.width = avframe->width;
    dst_buf.height = avframe->height;
    dst_buf.stride[0] = avframe->linesize[0];
    dst_buf.stride[1] = avframe->linesize[1];
    dst_buf.stride[2] = avframe->linesize[2];

    if (!Scaler::Process(&src_buf, &dst_buf, nullptr, nullptr, Scaler::Carrier::LIBYUV)) {
      LOGE(VEncodeFFmpegHandler) << "Encode() scaler process 1 failed";
      av_frame_free(&avframe);
      return -1;
    }
  }

  data_queue_.Push(avframe);

  return 0;
}


void VEncodeFFmpegHandler::Destroy() {
  if (av_codec_ctx_) {
    avcodec_close(av_codec_ctx_);
    av_codec_ctx_ = nullptr;
  }
  if (av_opts_) {
    av_dict_free(&av_opts_);
    av_opts_ = nullptr;
  }
  if (av_frame_) {
    av_frame_free(&av_frame_);
    av_frame_ = nullptr;
  }
  if (av_packet_) {
    av_packet_unref(av_packet_);
    av_free(av_packet_);
    av_packet_ = nullptr;
  }
}

void VEncodeFFmpegHandler::Loop() {
  int ret;
  AVFrame *frame = nullptr;

  while (state_ == State::RUNNING) {
    if (data_queue_.WaitAndTryPop(frame, std::chrono::microseconds(200))) {
      if (!frame) {
        if (eos_promise_) eos_promise_->set_value();
        break;
      }
      do {
        int got_packet = 0;
        av_packet_->data = nullptr;
        av_packet_->size = 0;
        ret = avcodec_encode_video2(av_codec_ctx_, av_packet_, frame, &got_packet);

        if (ret < 0) {
          LOGE(VEncodeFFmpegHandler) << "Loop() avcodec_encode_video2 failed, ret=" << ret;
          break;
        }

        if (got_packet) {
          CnedkVEncFrameBits frame_bits;
          frame_bits.bits = av_packet_->data;
          frame_bits.len = av_packet_->size;
          frame_bits.pts = av_packet_->pts;
          if (av_packet_->flags & AV_PKT_FLAG_KEY) {
            frame_bits.pkt_type = CNEDK_VENC_PACKAGE_TYPE_KEY_FRAME;
          }
          OnFrameBits(&frame_bits);
        }

        if (frame) av_frame_free(&frame);
        frame = nullptr;
      } while (0);

      // ret = avcodec_send_frame(av_codec_ctx_, frame);
      // if (ret < 0) {
      //   LOGE(VEncodeFFmpegHandler) << "avcodec send frame failed, ret=" << ret;
      // }

      // do {
      //   ret = avcodec_receive_packet(av_codec_ctx_, av_packet_);
      //   if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      //     break;
      //   } else if (ret < 0) {
      //     LOGE(VEncodeFFmpegHandler) << "Loop() avcodec_receive_packet failed, ret=" << ret;
      //     break;
      //   }

      //   CnedkVEncFrameBits frame_bits;
      //   frame_bits.bits = av_packet_->data;
      //   frame_bits.len = av_packet_->size;
      //   frame_bits.pts = av_packet_->pts;
      //   // std::cout << "frame_bits.len: " << frame_bits.len << std::endl;
      //   OnFrameBits(&frame_bits);
      // } while (ret >= 0);

      // av_frame_free(&frame);
    }
  }
}

}  // namespace cnstream
