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

#include <condition_variable>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <memory>

#include "data_handler_jpeg_mem.hpp"
#include "perf_manager.hpp"

namespace cnstream {

std::shared_ptr<SourceHandler>
  ESJpegMemHandler::Create(DataSource *module, const std::string &stream_id, int max_width, int max_height) {
  if (!module || stream_id.empty()) {
    return nullptr;
  }
  std::shared_ptr<ESJpegMemHandler>
    handler(new (std::nothrow) ESJpegMemHandler(module, stream_id, max_width, max_height));
  return handler;
}

ESJpegMemHandler::ESJpegMemHandler(DataSource *module, const std::string &stream_id, int max_width, int max_height)
  : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) ESJpegMemHandlerImpl(module, *this, max_width, max_height);
}

ESJpegMemHandler::~ESJpegMemHandler() {
  if (impl_) {
    delete impl_, impl_ = nullptr;
  }
}

bool ESJpegMemHandler::Open() {
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
  // updated with paramSet
  DataSource *source = dynamic_cast<DataSource *>(module_);
  param_ = source->GetSourceParam();

  if (param_.decoder_type_ != DECODER_MLU) {
    LOG(ERROR) << "decoder_type not supported:" << param_.decoder_type_;
    return false;
  }

  this->interval_ = param_.interval_;
  perf_manager_ = source->GetPerfManager(stream_id_);
  size_t MaxSize = 60;  // FIXME
  queue_ = new (std::nothrow) cnstream::BoundedQueue<std::shared_ptr<EsPacket>>(MaxSize);
  if (!queue_) {
    return false;
  }

  // start demuxer
  running_.store(1);
  thread_ = std::thread(&ESJpegMemHandlerImpl::DecodeLoop, this);
  return true;
}

void ESJpegMemHandlerImpl::Close() {
  if (running_.load()) {
    running_.store(0);
    if (thread_.joinable()) {
      thread_.join();
    }
  }
  if (queue_) {
    delete queue_;
    queue_ = nullptr;
  }
  // parser_.Free();
}

int ESJpegMemHandlerImpl::Write(ESPacket *pkt) {
  if (queue_) {
    queue_->Push(std::make_shared<EsPacket>(pkt));
  } else {
    return -1;
  }
  return 0;
}

void ESJpegMemHandlerImpl::DecodeLoop() {
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
  LOG(INFO) << "DecodeLoop Exit";
}

bool ESJpegMemHandlerImpl::PrepareResources() {
  VideoStreamInfo info;
  info.codec_id = AV_CODEC_ID_MJPEG;
  info.codec_width = max_width_;
  info.codec_height = max_height_;;

  if (!running_.load()) {
    return false;
  }

  decoder_ = std::make_shared<MluDecoder>(this);

  if (!decoder_) {
    return false;
  }
  bool ret = decoder_->Create(&info, interval_);
  if (!ret) {
      return false;
  }

  return true;
}

void ESJpegMemHandlerImpl::ClearResources() {
  if (decoder_.get()) {
    decoder_->Destroy();
  }
}

bool ESJpegMemHandlerImpl::Process() {
  using EsPacketPtr = std::shared_ptr<EsPacket>;

  EsPacketPtr in;
  int timeoutMs  = 1000;
  bool ret = this->queue_->Pop(timeoutMs, in);

  if (!ret) {
    // continue.. not exit
    return true;
  }

  if (perf_manager_ != nullptr) {
    std::string thread_name = "cn-" + module_->GetName() + stream_id_;
    perf_manager_->Record(false, PerfManager::GetDefaultType(), module_->GetName(), in->pkt_.pts);
    perf_manager_->Record(PerfManager::GetDefaultType(), PerfManager::GetPrimaryKey(), std::to_string(in->pkt_.pts),
                          module_->GetName() + "_th", "'" + thread_name + "'");
  }

  if (in->pkt_.flags & ESPacket::FLAG_EOS) {
    LOG(INFO) << "Eos reached";
    ESPacket pkt;
    pkt.data = in->pkt_.data;
    pkt.size = in->pkt_.size;
    pkt.pts = in->pkt_.pts;
    pkt.flags = ESPacket::FLAG_EOS;
    decoder_->Process(&pkt);
    return false;
  }  // if (!ret)

  ESPacket pkt;
  pkt.data = in->pkt_.data;
  pkt.size = in->pkt_.size;
  pkt.pts = in->pkt_.pts;
  pkt.flags &= ~ESPacket::FLAG_EOS;
  if (!decoder_->Process(&pkt)) {
    return false;
  }
  return true;
}

}  // namespace cnstream
