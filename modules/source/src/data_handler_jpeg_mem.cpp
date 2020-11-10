/*************************************************************************
:a
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
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "perf_manager.hpp"
#include "data_handler_jpeg_mem.hpp"

namespace cnstream {

std::shared_ptr<SourceHandler> ESJpegMemHandler::Create(DataSource *module, const std::string &stream_id, int max_width,
                                                        int max_height) {
  if (!module || stream_id.empty()) {
    LOGE(SOURCE) << "source module or stream id must not be empty";
    return nullptr;
  }
  std::shared_ptr<ESJpegMemHandler> handler(new (std::nothrow)
                                                ESJpegMemHandler(module, stream_id, max_width, max_height));
  return handler;
}

ESJpegMemHandler::ESJpegMemHandler(DataSource *module, const std::string &stream_id, int max_width, int max_height)
    : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) ESJpegMemHandlerImpl(module, this, max_width, max_height);
}

ESJpegMemHandler::~ESJpegMemHandler() {
  if (impl_) {
    delete impl_, impl_ = nullptr;
  }
}

bool ESJpegMemHandler::Open() {
  if (!this->module_) {
    LOGE(SOURCE) << "module_ null";
    return false;
  }
  if (!impl_) {
    LOGE(SOURCE) << "impl_ null";
    return false;
  }

  if (stream_index_ == cnstream::INVALID_STREAM_IDX) {
    LOGE(SOURCE) << "invalid stream_idx";
    return false;
  }

  return impl_->Open();
}

void ESJpegMemHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

int ESJpegMemHandler::Write(ESPacket *pkt) {
  if (impl_) {
    return impl_->Write(pkt);
  }
  return -1;
}

bool ESJpegMemHandlerImpl::Open() {
  DataSource *source = dynamic_cast<DataSource *>(module_);
  if (nullptr != source) {
    param_ = source->GetSourceParam();
  } else {
    LOGE(SOURCE) << "source module is null";
    return false;
  }

  SetPerfManager(source->GetPerfManager(stream_id_));
  SetThreadName(module_->GetName(), handler_.GetStreamUniqueIdx());
  return InitDecoder();
}

void ESJpegMemHandlerImpl::Close() {
  if (decoder_.get()) {
    decoder_->Destroy();
  }
}

int ESJpegMemHandlerImpl::Write(ESPacket *pkt) {
  if (pkt && decoder_) {
    ProcessImage(pkt);
    return 0;
  }

  return -1;
}

bool ESJpegMemHandlerImpl::ProcessImage(ESPacket *in_pkt) {
  if (in_pkt->flags & ESPacket::FLAG_EOS) {
    decoder_->Process(nullptr);
    return false;
  }

  VideoEsPacket pkt;
  pkt.data = in_pkt->data;
  pkt.len = in_pkt->size;
  pkt.pts = in_pkt->pts;
  RecordStartTime(module_->GetName(), in_pkt->pts);
  if (!decoder_->Process(&pkt)) {
    return false;
  }

  return true;
}

bool ESJpegMemHandlerImpl::InitDecoder() {
  if (param_.decoder_type_ == DecoderType::DECODER_MLU) {
    decoder_ = std::make_shared<MluDecoder>(this);
  } else if (param_.decoder_type_ == DecoderType::DECODER_CPU) {
    decoder_ = std::make_shared<FFmpegCpuDecoder>(this);
  } else {
    LOGE(SOURCE) << "unsupported decoder_type";
    return false;
  }
  if (!decoder_) {
    return false;
  }

  // FIXME, fill info, a parser is needed?
  VideoInfo info;
  info.codec_id = AV_CODEC_ID_MJPEG;

  ExtraDecoderInfo extra;
  extra.apply_stride_align_for_scaler = param_.apply_stride_align_for_scaler_;
  extra.device_id = param_.device_id_;
  extra.input_buf_num = param_.input_buf_number_;
  extra.output_buf_num = param_.output_buf_number_;
  extra.max_width = max_width_;
  extra.max_height = max_height_;
  bool ret = decoder_->Create(&info, &extra);
  if (!ret) {
    return false;
  }

  MluDeviceGuard guard(param_.device_id_);
  return true;
}

// IDecodeResult methods
void ESJpegMemHandlerImpl::OnDecodeError(DecodeErrorCode error_code) {
  // FIXME,  handle decode error ...
  interrupt_.store(true);
}

void ESJpegMemHandlerImpl::OnDecodeFrame(DecodeFrame *frame) {
  if (frame_count_++ % param_.interval_ != 0) {
    return;  // discard frames
  }
  if (!frame) return;
  if (!frame->valid) return;

  std::shared_ptr<CNFrameInfo> data = this->CreateFrameInfo();
  if (!data) {
    return;
  }
  data->timestamp = frame->pts;  // FIXME
  int ret = SourceRender::Process(data, frame, frame_id_++, param_);
  if (ret < 0) {
    return;
  }
  this->SendFrameInfo(data);
}
void ESJpegMemHandlerImpl::OnDecodeEos() {
  this->SendFlowEos();
}

}  // namespace cnstream
