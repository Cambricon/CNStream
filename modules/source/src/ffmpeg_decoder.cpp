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
#include <memory>
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
static CNDataFormat PixelFmt2CnDataFormat(edk::PixelFmt pformat) {
  switch (pformat) {
    case edk::PixelFmt::NV12:
      return CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12;
    case edk::PixelFmt::NV21:
      return CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
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
  edk::EasyDecode::Attr instance_attr;
  memset(&instance_attr, 0, sizeof(instance_attr));
  // common attrs
  instance_attr.frame_geometry.w = codec_width;
  instance_attr.frame_geometry.h = codec_height;
  switch (codec_id) {
    case AV_CODEC_ID_H264:
      instance_attr.codec_type = edk::CodecType::H264;
      break;
    case AV_CODEC_ID_HEVC:
      instance_attr.codec_type = edk::CodecType::H265;
      break;
    case AV_CODEC_ID_MJPEG:
      instance_attr.codec_type = edk::CodecType::JPEG;
      break;
    default: {
      LOG(ERROR) << "codec type not supported yet, codec_id = " << codec_id;
      return false;
    }
  }
  instance_attr.pixel_format = edk::PixelFmt::NV21;
  // instance_attr.output_geometry.w = handler_.Output_w() > 0 ? handler_.Output_w() : codec_width;
  // instance_attr.output_geometry.h = handler_.Output_h() > 0 ? handler_.Output_h() : codec_height;
  instance_attr.input_buffer_num = handler_.InputBufNumber();
  instance_attr.output_buffer_num = handler_.OutputBufNumber();
  if (handler_.ReuseCNDecBuf()) {
    instance_attr.output_buffer_num += cnstream::GetParallelism();  // FIXME
  }
  instance_attr.dev_id = dev_ctx_.dev_id;
  instance_attr.silent = false;
  instance_attr.stride_align = 1;
  // callbacks

  instance_attr.frame_callback = std::bind(&FFmpegMluDecoder::FrameCallback, this, std::placeholders::_1);
  instance_attr.eos_callback = std::bind(&FFmpegMluDecoder::EOSCallback, this);

  // create CnDecode
  try {
    std::unique_lock<std::mutex> lock(decoder_mutex);
    instance_.reset();
    eos_got_.store(0);
    instance_.reset(edk::EasyDecode::Create(instance_attr));
    if (nullptr == instance_.get()) {
      LOG(ERROR) << "[Decoder] stream_id " << stream_id_ << " failed to create";
      return false;
    }
  } catch (edk::Exception &e) {
    LOG(ERROR) << "[Decoder] stream_id " << stream_id_ << " error message: " << e.what();
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
    /*make sure all cndec buffers released before destorying cndecoder
     */
    while (cndec_buf_ref_count_.load()) {
      std::this_thread::yield();
    }
    while (!eos_got_.load()) {
      std::this_thread::yield();
    }
    eos_got_.store(2);  // avoid double-wait eos
  }
}

bool FFmpegMluDecoder::Process(AVPacket *pkt, bool eos) {
  LOG_IF(INFO, eos) << "[FFmpegMluDecoder] stream_id " << stream_id_ << " send eos.";
  try {
    edk::CnPacket packet;
    if (pkt && !eos) {
      packet.data = pkt->data;
      packet.length = pkt->size;
      packet.pts = pkt->pts;
    } else {
      packet.length = 0;
    }
    if (instance_->SendData(packet, eos)) {
      return true;
    } else {
    }
  } catch (edk::Exception &e) {
    LOG(ERROR) << "[Decoder] stream_id " << stream_id_ << " error message: " << e.what();
    return false;
  }

  return false;
}

int FFmpegMluDecoder::ProcessFrame(const edk::CnFrame &frame, bool *reused) {
  *reused = false;

  // FIXME, remove infinite-loop
  std::shared_ptr<CNFrameInfo> data;
  while (1) {
    data = CNFrameInfo::Create(stream_id_);
    if (data.get() != nullptr) break;
    if (stream_id_.empty()) return -1;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  data->channel_idx = stream_idx_;
  data->frame.frame_id = frame_id_++;
  data->frame.timestamp = frame.pts;
  /*fill source data info*/
  data->frame.ctx = dev_ctx_;
  data->frame.width = frame.width;
  data->frame.height = frame.height;
  data->frame.fmt = PixelFmt2CnDataFormat(frame.pformat);
  for (int i = 0; i < data->frame.GetPlanes(); i++) {
    data->frame.stride[i] = frame.strides[i];
    data->frame.ptr_mlu[i] = reinterpret_cast<void *>(frame.ptrs[i]);
  }
  if (handler_.ReuseCNDecBuf()) {
    data->frame.deAllocator_ = std::make_shared<CNDeallocator>(this, frame.buf_id);
    if (data->frame.deAllocator_) {
      *reused = true;
    }
  }
  data->frame.CopyToSyncMem();
  handler_.SendData(data);
  return 0;
}

void FFmpegMluDecoder::FrameCallback(const edk::CnFrame &frame) {
  if (frame.width == 0 || frame.height == 0) {
    LOG(WARNING) << "Skip frame! stream id:" << stream_id_ << " width x height:" << frame.width << " x " << frame.height
                 << " timestamp:" << frame.pts << std::endl;
    instance_->ReleaseBuffer(frame.buf_id);
    return;
  }
  bool reused = false;
  if (frame_count_++ % interval_ == 0) {
    ProcessFrame(frame, &reused);
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
#if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 14, 0)) \
  || ((LIBAVCODEC_VERSION_MICRO >= 100) && (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 33, 100)))
  instance_ = st->codec;
#else
  instance_ = avcodec_alloc_context3(dec);
  if (!instance_) {
    LOG(ERROR) << "Failed to do avcodec_alloc_context3";
    return false;
  }
  if (avcodec_parameters_to_context(instance_, st->codecpar) < 0) {
    LOG(ERROR) << "Failed to copy codec parameters to decoder context";
    return false;
  }
  av_codec_set_pkt_timebase(instance_, st->time_base);
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
      while (this->Process(nullptr, true)) {
      }
    }
    while (!eos_got_.load()) {
      std::this_thread::yield();
    }
    eos_got_.store(0);
#if !((LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 14, 0)) \
        || ((LIBAVCODEC_VERSION_MICRO >= 100) && (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 33, 100))))
    avcodec_free_context(&instance_);
#endif
    instance_ = nullptr;
  }
  if (av_frame_) {
    av_frame_free(&av_frame_);
    av_frame_ = nullptr;
  }
  if (nullptr != nv21_data_) {
    delete[] nv21_data_, nv21_data_ = nullptr;
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
  if (frame_count_++ % interval_ != 0) {
    return true;  // discard frames
  }

  // FIXME, remove infinite-loop
  std::shared_ptr<CNFrameInfo> data;
  while (1) {
    data = CNFrameInfo::Create(stream_id_);
    if (data != nullptr) {
      break;
    }
    if (stream_id_.empty()) return false;
    std::this_thread::sleep_for(std::chrono::microseconds(5));
  }
  data->channel_idx = stream_idx_;

  if (instance_->pix_fmt != AV_PIX_FMT_YUV420P && instance_->pix_fmt != AV_PIX_FMT_YUVJ420P) {
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
      nv21_data_ = new(std::nothrow) uint8_t[y_size_ * 3 / 2];
      if (nullptr == nv21_data_) {
        LOG(ERROR) << "FFmpegCpuDecoder::ProcessFrame() Failed to alloc memory, size:" << y_size_ * 3 / 2;
        return false;
      }
    }

    /*yuv420 to NV21*/
    memcpy(nv21_data_, frame->data[0], frame->linesize[0] * frame->height);
    uint8_t *u = frame->data[1];
    uint8_t *v = frame->data[2];
    uint8_t *vu = reinterpret_cast<uint8_t *>(nv21_data_) + frame->linesize[0] * frame->height;
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
      CNSyncedMemory* CNSyncedMemory_ptr =
        new(std::nothrow) CNSyncedMemory(plane_size, dev_ctx_.dev_id, dev_ctx_.ddr_channel);
      LOG_IF(FATAL, nullptr == CNSyncedMemory_ptr) << "FFmpegCpuDecoder::ProcessFrame() new CNSyncedMemory failed";
      data->frame.data[i].reset(CNSyncedMemory_ptr);
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
    uint8_t *vu = reinterpret_cast<uint8_t *>(data->frame.cpu_data) + frame->linesize[0] * frame->height;
    for (int i = 0; i < frame->linesize[1] * frame->height / 2; i++) {
      *vu++ = *v++;
      *vu++ = *u++;
    }
    auto t = reinterpret_cast<uint8_t *>(data->frame.cpu_data);
    for (int i = 0; i < data->frame.GetPlanes(); ++i) {
      size_t plane_size = data->frame.GetPlaneBytes(i);
      CNSyncedMemory* CNSyncedMemory_ptr = new(std::nothrow) CNSyncedMemory(plane_size);
      LOG_IF(FATAL, nullptr == CNSyncedMemory_ptr) << "FFmpegCpuDecoder::ProcessFrame() new CNSyncedMemory failed";
      data->frame.data[i].reset(CNSyncedMemory_ptr);
      data->frame.data[i]->SetCpuData(t);
      t += plane_size;
    }
  } else {
    LOG(ERROR) << "DevContex::INVALID";
    return false;
  }

  data->frame.frame_id = frame_id_++;
  data->frame.timestamp = frame->pts;
  handler_.SendData(data);
  return true;
}

}  // namespace cnstream
