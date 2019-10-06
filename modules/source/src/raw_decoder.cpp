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
#include "raw_decoder.hpp"
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

bool RawMluDecoder::Create(DecoderContext *ctx) {
  // create decoder
  libstream::CnDecode::Attr instance_attr;
  memset(&instance_attr, 0, sizeof(instance_attr));
  // common attrs
  instance_attr.maximum_geometry.w = ctx->width;
  instance_attr.maximum_geometry.h = ctx->height;
  switch (ctx->codec_id) {
    case DecoderContext::CN_CODEC_ID_H264:
      instance_attr.codec_type = libstream::H264;
      break;
    case DecoderContext::CN_CODEC_ID_HEVC:
      instance_attr.codec_type = libstream::H265;
      break;
    case DecoderContext::CN_CODEC_ID_JPEG:
      instance_attr.codec_type = libstream::JPEG;
      break;
    default: {
      LOG(ERROR) << "codec type not supported yet, codec_id = " << ctx->codec_id;
      return false;
    }
  }
  instance_attr.pixel_format = libstream::YUV420SP_NV21;
  instance_attr.output_geometry.w = ctx->width;
  instance_attr.output_geometry.h = ctx->height;
  instance_attr.drop_rate = 0;
  instance_attr.frame_buffer_num = 3;
  if (handler_.ReuseCNDecBuf()) {
    instance_attr.frame_buffer_num += 6;  // FIXME
  }
  instance_attr.dev_id = dev_ctx_.dev_id;
  if (instance_attr.codec_type == libstream::JPEG) {
    instance_attr.video_mode = libstream::FRAME_MODE;
  } else {
    instance_attr.video_mode = libstream::STREAM_MODE;
  }
  instance_attr.silent = false;

  // callbacks
  instance_attr.frame_callback = std::bind(&RawMluDecoder::FrameCallback, this, std::placeholders::_1);
  instance_attr.perf_callback = std::bind(&RawMluDecoder::PerfCallback, this, std::placeholders::_1);
  instance_attr.eos_callback = std::bind(&RawMluDecoder::EOSCallback, this);

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

void RawMluDecoder::Destroy() {
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

bool RawMluDecoder::Process(RawPacket *pkt, bool eos) {
  LOG_IF(INFO, eos) << "[RawMluDecoder] stream_id " << stream_id_ << " send eos.";
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

int RawMluDecoder::ProcessFrame(const libstream::CnFrame &frame, bool &reused) {
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

void RawMluDecoder::FrameCallback(const libstream::CnFrame &frame) {
  bool reused = false;
  if (frame_count_++ % interval_ == 0) {
    ProcessFrame(frame, reused);
  }
  if (!reused) {
    instance_->ReleaseBuffer(frame.buf_id);
  };
}

void RawMluDecoder::EOSCallback() {
  handler_.SendFlowEos();
  eos_got_.store(1);
}

}  // namespace cnstream
