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
#include "ffmpeg_decoder.hpp"
#include <cnrt.h>
#include <glog/logging.h>
#include <future>
#include <sstream>
#include <thread>
#include <utility>
namespace cnstream {

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

// FFMPEG use AVCodecParameters instead of AVCodecContext since from version 3.1(libavformat/version:57.40.100)
#define FFMPEG_VERSION_3_1 AV_VERSION_INT(57, 40, 100)

static std::mutex decoder_mutex;
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

bool FFmpegMluDecoder::Create(AVStream *st) {
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  AVCodecID codec_id = st->codecpar->codec_id;
  int codec_width = st->codecpar->width;
  int codec_height = st->codecpar->height;
#else
  AVCodecID codec_id = st->codec->codec_id;
  int codec_width = st->codec->width;
  int codec_height = st->codec->height;
#endif
  // create decoder
  libstream::CnDecode::Attr instance_attr;
  memset(&instance_attr, 0, sizeof(instance_attr));
  // common attrs
  instance_attr.maximum_geometry.w = codec_width;
  instance_attr.maximum_geometry.h = codec_height;
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
  instance_attr.output_geometry.w = codec_width;
  instance_attr.output_geometry.h = codec_height;
  instance_attr.drop_rate = 0;
  instance_attr.frame_buffer_num = 3;
  if (handler_.ReuseCNDecBuf()) {
    instance_attr.frame_buffer_num += 6;  // FIXME
  }
  instance_attr.dev_id = dev_ctx_.dev_id;
  instance_attr.video_mode = libstream::FRAME_MODE;
  instance_attr.silent = false;

  // callbacks
  instance_attr.frame_callback = std::bind(&FFmpegMluDecoder::FrameCallback, this, std::placeholders::_1);
  instance_attr.perf_callback = std::bind(&FFmpegMluDecoder::PerfCallback, this, std::placeholders::_1);
  instance_attr.eos_callback = std::bind(&FFmpegMluDecoder::EOSCallback, this);

  // create CnDecode
  try {
    std::unique_lock<std::mutex> lock(decoder_mutex);
    instance_.reset();
    eos_got_.store(0);
    instance_.reset(libstream::CnDecode::Create(instance_attr));
    if (nullptr == instance_.get()) {
      LOG(ERROR) << "[Decoder] failed to create";
      return false;
    }
  } catch (libstream::StreamlibsError &e) {
    LOG(ERROR) << "[Decoder] " << e.what();
    return false;
  }
  return true;
}

void FFmpegMluDecoder::Destroy() {
  if (instance_ != nullptr) {
    if (eos_got_.load() > 1) {
      return;
    }
    if (!handler_.GetDemuxEos()) {
      this->Process(nullptr, true);
    }
    while (!eos_got_.load()) {
      usleep(1000 * 10);
    }
    eos_got_.store(2);  // avoid double-wait eos
  }
}

bool FFmpegMluDecoder::Process(AVPacket *pkt, bool eos) {
  LOG_IF(INFO, eos) << "[FFmpegMluDecoder] stream_id " << stream_id_ << " send eos.";
  try {
    libstream::CnPacket packet;
    if (pkt && !eos) {
      packet.data = pkt->data;
      packet.length = pkt->size;
      packet.pts = pkt->pts;
    } else {
      packet.length = 0;
    }
    if (instance_->SendData(packet, eos)) {
      return true;
    }
  } catch (libstream::StreamlibsError &e) {
    LOG(ERROR) << "[Decoder] " << e.what();
    return false;
  }

  return false;
}

int FFmpegMluDecoder::ProcessFrame(const libstream::CnFrame &frame, bool &reused) {
  reused = false;
  auto data = CNFrameInfo::Create(stream_id_);
  if (data == nullptr) {
    // LOG(WARNING) << "CNFrameInfo::Create Failed,DISCARD image";
    ++discard_frame_num_;
    if (discard_frame_num_ % 20 == 0) {
      LOG(WARNING) << "CNFrameInfo::Create Failed,DISCARD image: " << discard_frame_num_;
    }
    return -1;
  }
  data->channel_idx = stream_idx_;
  data->frame.frame_id = frame_id_++;
  data->frame.timestamp = frame.pts;
  /*fill source data info*/
  data->frame.ctx = dev_ctx_;
  data->frame.width = frame.width;
  data->frame.height = frame.height;
  data->frame.fmt = CnPixelFormat2CnDataFormat(frame.pformat);
  for (int i = 0; i < data->frame.GetPlanes(); i++) {
    data->frame.stride[i] = frame.strides[i];
    data->frame.ptr[i] = (void *)frame.data.ptrs[i];
  }
  if (handler_.ReuseCNDecBuf()) {
    data->frame.deAllocator_ = std::make_shared<CNDeallocator>(instance_, frame.buf_id);
    if (data->frame.deAllocator_) {
      reused = true;
    }
  }
  data->frame.CopyToSyncMem();
  handler_.SendData(data);
  return 0;
}

void FFmpegMluDecoder::FrameCallback(const libstream::CnFrame &frame) {
  bool reused = false;
  if (frame_count_++ % interval_ == 0) {
    ProcessFrame(frame, reused);
  }
  if (!reused) {
    instance_->ReleaseBuffer(frame.buf_id);
  }
}

void FFmpegMluDecoder::EOSCallback() {
  handler_.SendFlowEos();
  eos_got_.store(1);
}

//----------------------------------------------------------------------------
// CPU decoder
bool FFmpegCpuDecoder::Create(AVStream *st) {
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  AVCodecID codec_id = st->codecpar->codec_id;
#else
  AVCodecID codec_id = st->codec->codec_id;
#endif
  // create decoder
  AVCodec *dec = avcodec_find_decoder(codec_id);
  if (!dec) {
    LOG(ERROR) << "avcodec_find_decoder failed";
    return false;
  }
  instance_ = avcodec_alloc_context3(dec);
  if (!instance_) {
    LOG(ERROR) << "Failed to do avcodec_alloc_context3";
    return false;
  }
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 0, 0)
  if (avcodec_parameters_to_context(instance_, st->codecpar) < 0) {
    LOG(ERROR) << "Failed to copy codec parameters to decoder context";
    return false;
  }
#endif
  if (avcodec_open2(instance_, dec, NULL) < 0) {
    LOG(ERROR) << "Failed to open codec";
    return false;
  }
  av_frame_ = av_frame_alloc();
  if (!av_frame_) {
    LOG(ERROR) << "Could not alloc frame";
    return false;
  }
  return true;
}

void FFmpegCpuDecoder::Destroy() {
  if (instance_ != nullptr) {
    if (!handler_.GetDemuxEos()) {
      while (this->Process(nullptr, true))
        ;
    }
    while (!eos_got_.load()) {
      usleep(1000 * 10);
    }
    eos_got_.store(0);
    avcodec_free_context(&instance_);
    instance_ = nullptr;
  }
  if (av_frame_) {
    av_frame_free(&av_frame_);
    av_frame_ = nullptr;
  }
}

bool FFmpegCpuDecoder::Process(AVPacket *pkt, bool eos) {
  LOG_IF(INFO, eos) << "[FFmpegCpuDecoder] stream_id " << stream_id_ << " send eos.";
  if (eos) {
    AVPacket packet;
    av_init_packet(&packet);
    packet.size = 0;
    packet.data = NULL;

    // flush all frames ...
    int got_frame = 0;
    do {
      avcodec_decode_video2(instance_, av_frame_, &got_frame, &packet);
      if (got_frame) ProcessFrame(av_frame_);
    } while (got_frame);

    handler_.SendFlowEos();
    eos_got_.store(1);
    return false;
  }
  int got_frame = 0;
  int ret = avcodec_decode_video2(instance_, av_frame_, &got_frame, pkt);
  if (ret < 0) {
    LOG(ERROR) << "avcodec_decode_video2 failed";
    return false;
  }
  if (got_frame) {
    ProcessFrame(av_frame_);
  }
  return true;
}

bool FFmpegCpuDecoder::ProcessFrame(AVFrame *frame) {
  if (frame_count_ % interval_ != 0) {
    return true;  // discard frames
  }
  auto data = CNFrameInfo::Create(stream_id_);
  if (data == nullptr) {
    ++discard_frame_num_;
    // LOG(WARNING) << "CNFrameInfo::Create Failed,DISCARD image";
    if (discard_frame_num_ % 20 == 0) {
      LOG(WARNING) << "CNFrameInfo::Create Failed,DISCARD image: " << discard_frame_num_;
    }
    return false;
  }
  if (instance_->pix_fmt != AV_PIX_FMT_YUV420P) {
    LOG(ERROR) << "FFmpegCpuDecoder only supports AV_PIX_FMT_YUV420P at this moment";
    return false;
  }

  data->frame.ctx = dev_ctx_;
  data->frame.fmt = CN_PIXEL_FORMAT_YUV420_NV21;
  data->frame.width = frame->width;
  data->frame.height = frame->height;
  data->frame.stride[0] = frame->linesize[0];
  data->frame.stride[1] = frame->linesize[0];

  if (DevContext::MLU == dev_ctx_.dev_type) {
    if (y_size_ != frame->linesize[0] * frame->height) {
      if (nullptr != nv21_data_) delete[] nv21_data_;
      y_size_ = frame->linesize[0] * frame->height;
      nv21_data_ = new uint8_t[y_size_ * 3 / 2];
      if (nullptr == nv21_data_) {
        LOG(ERROR) << "FFmpegCpuDecoder: Failed to alloc memory";
        return false;
      }
    }

    /*yuv420 to NV21*/
    memcpy(nv21_data_, frame->data[0], frame->linesize[0] * frame->height);
    uint8_t *u = frame->data[1];
    uint8_t *v = frame->data[2];
    uint8_t *vu = (uint8_t *)nv21_data_ + frame->linesize[0] * frame->height;
    for (int i = 0; i < frame->linesize[1] * frame->height / 2; i++) {
      *vu++ = *v++;
      *vu++ = *u++;
    }
    CALL_CNRT_BY_CONTEXT(cnrtMalloc(&data->frame.mlu_data, y_size_ * 3 / 2), dev_ctx_.dev_id, dev_ctx_.ddr_channel);
    if (nullptr == data->frame.mlu_data) {
      LOG(ERROR) << "FFmpegCpuDecoder: Failed to alloc mlu memory";
      return false;
    }
    CALL_CNRT_BY_CONTEXT(cnrtMemcpy(data->frame.mlu_data, nv21_data_, y_size_ * 3 / 2, CNRT_MEM_TRANS_DIR_HOST2DEV),
                         dev_ctx_.dev_id, dev_ctx_.ddr_channel);

    auto t = reinterpret_cast<uint8_t *>(data->frame.mlu_data);
    for (int i = 0; i < data->frame.GetPlanes(); ++i) {
      size_t plane_size = data->frame.GetPlaneBytes(i);
      data->frame.data[i].reset(new CNSyncedMemory(plane_size, dev_ctx_.dev_id, dev_ctx_.ddr_channel));
      data->frame.data[i]->SetMluData(t);
      t += plane_size;
    }
  } else if (DevContext::CPU == dev_ctx_.dev_type) {
    CNStreamMallocHost(&data->frame.cpu_data, frame->linesize[0] * frame->height * 3 / 2);
    if (!data->frame.cpu_data) {
      LOG(WARNING) << "CNStreamMallocHost failed";
      return false;
    }
    memcpy(data->frame.cpu_data, frame->data[0], frame->linesize[0] * frame->height);
    uint8_t *u = frame->data[1];
    uint8_t *v = frame->data[2];
    uint8_t *vu = (uint8_t *)data->frame.cpu_data + frame->linesize[0] * frame->height;
    for (int i = 0; i < frame->linesize[1] * frame->height / 2; i++) {
      *vu++ = *v++;
      *vu++ = *u++;
    }
    auto t = reinterpret_cast<uint8_t *>(data->frame.cpu_data);
    for (int i = 0; i < data->frame.GetPlanes(); ++i) {
      size_t plane_size = data->frame.GetPlaneBytes(i);
      data->frame.data[i].reset(new CNSyncedMemory(plane_size));
      data->frame.data[i]->SetCpuData(t);
      t += plane_size;
    }
  } else {
    LOG(ERROR) << "DevContex::INVALID";
    return false;
  }

  data->channel_idx = stream_idx_;
  data->frame.frame_id = frame_id_++;
  data->frame.timestamp = frame->pts;
  handler_.SendData(data);
  return 0;
}

}  // namespace cnstream
