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

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
}
#endif

#include <condition_variable>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <memory>

#include "data_handler_mem.hpp"
#include "perf_manager.hpp"

namespace cnstream {

std::shared_ptr<SourceHandler> ESMemHandler::Create(DataSource *module, const std::string &stream_id) {
  if (!module || stream_id.empty()) {
    return nullptr;
  }
  std::shared_ptr<ESMemHandler> handler(new (std::nothrow) ESMemHandler(module, stream_id));
  return handler;
}

ESMemHandler::ESMemHandler(DataSource *module, const std::string &stream_id) : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) ESMemHandlerImpl(module, *this);
}

ESMemHandler::~ESMemHandler() {
  if (impl_) {
    delete impl_, impl_ = nullptr;
  }
}

bool ESMemHandler::Open() {
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

void ESMemHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

void ESMemHandler::SetDataType(ESMemHandler::DataType type) {
  if (impl_) {
    impl_->SetDataType(type);
  }
}

int ESMemHandler::Write(ESPacket *pkt) {
  if (impl_) {
    return impl_->Write(pkt);
  }
  return -1;
}

int ESMemHandler::Write(unsigned char *data, int len) {
  if (impl_) {
    return impl_->Write(data, len);
  }
  return -1;
}

bool ESMemHandlerImpl::Open() {
  // updated with paramSet
  DataSource *source = dynamic_cast<DataSource *>(module_);
  param_ = source->GetSourceParam();
#if 0
  if (param_.decoder_type_ != DECODER_MLU) {
    LOG(ERROR) << "decoder_type not supported:" << param_.decoder_type_;
    return false;
  }
#endif
  this->interval_ = param_.interval_;
  perf_manager_ = source->GetPerfManager(stream_id_);
  size_t MaxSize = 60;  // FIXME
  queue_ = new (std::nothrow) cnstream::BoundedQueue<std::shared_ptr<EsPacket>>(MaxSize);
  if (!queue_) {
    return false;
  }
  parser_.Open();
  // start demuxer
  running_.store(1);
  thread_ = std::move(std::thread(&ESMemHandlerImpl::DecodeLoop, this));
  return true;
}

void ESMemHandlerImpl::Close() {
  if (running_.load()) {
    running_.store(0);
    if (thread_.joinable()) {
      thread_.join();
    }
  }
  parser_.Close();
  if (queue_) {
    delete queue_;
    queue_ = nullptr;
  }
}

int ESMemHandlerImpl::Write(ESPacket *pkt) {
  if (pkt && pkt->data && pkt->size) {
    // send data to parser if needed
    if (!parse_done_.load() && running_.load()) {
      int ret = parser_.Parse(pkt->data, pkt->size);
      if (ret < 0) {
        return -1;
      }
      usleep(40 * 1000);  // FIXME
    }
  }
  if (queue_) {
    queue_->Push(std::make_shared<EsPacket>(pkt));
  } else {
    return -1;
  }
  return 0;
}

int ESMemHandlerImpl::Write(unsigned char *data, int len) {
  if (data && len) {
    // send data to parser if needed
    if (!parse_done_.load() && running_.load()) {
      int ret = parser_.Parse(data, len);
      if (ret < 0) {
        return -1;
      }
      usleep(40 * 1000);  // FIXME
    }
  }

  if (!queue_) {
    return -1;
  }

  if (!es_buffer_) {
    es_buffer_ = new (std::nothrow) unsigned char[max_es_buffer_size];
    if (!es_buffer_) {
      return -1;
    }
    es_len_ = 0;
  }

  if (!len || !data) {
    ESPacket pkt;
    if (es_len_) {
      pkt.data = es_buffer_;
      pkt.size = es_len_;
      es_len_ = 0;
    }
    pkt.flags = ESPacket::FLAG_EOS;
    queue_->Push(std::make_shared<EsPacket>(&pkt));
    return 0;
  }

  if (len + es_len_ > max_es_buffer_size) {
    return -1;
  }

  memcpy(es_buffer_ + es_len_, data, len);
  es_len_ += len;

  if (es_len_ <= 4) {
    return 0;
  }

  int pos = 0;
  int i = 4;
  while (i < es_len_ - 4) {
    if (es_buffer_[i + 0] == 0x00
        && es_buffer_[i + 1] == 0x00
        && es_buffer_[i + 2] == 0x00
        && es_buffer_[i + 3] == 0x01) {
      if (i - pos > 1000) {  // FIXME, parse nal type instead
        ESPacket pkt;
        pkt.data = &es_buffer_[pos];
        pkt.size = i - pos;
        pkt.pts = pts_++;
        queue_->Push(std::make_shared<EsPacket>(&pkt));
        pos = i;
      }
      i += 4;
    } else {
      ++i;
    }
  }
  if (pos != 0) {
    memmove(es_buffer_, &es_buffer_[pos], es_len_ - pos);
    es_len_ -= pos;
  }
  return 0;
}

void ESMemHandlerImpl::DecodeLoop() {
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

bool ESMemHandlerImpl::PrepareResources() {
  VideoStreamInfo info;
  while (1) {
    if (parser_.GetInfo(info) > 0) {
      parse_done_.store(1);
      break;
    }
    if (!running_.load()) {
      break;
    }
    usleep(1000 * 10);
  }
  if (param_.decoder_type_ == DecoderType::DECODER_MLU) {
    decoder_ = std::make_shared<MluDecoder>(this);
  } else if (param_.decoder_type_ == DecoderType::DECODER_CPU) {
    decoder_ = std::make_shared<FFmpegCpuDecoder>(this);
  } else {
    LOG(ERROR) << "unsupported decoder_type";
    return false;
  }
  if (!decoder_) {
    return false;
  }
  bool ret = decoder_->Create(&info, interval_);
  if (!ret) {
      return false;
  }
  if (info.extra_data.size()) {
    ESPacket pkt;
    pkt.data = info.extra_data.data();
    pkt.size = info.extra_data.size();
    if (!decoder_->Process(&pkt)) {
      return false;
    }
  }
  return true;
}

void ESMemHandlerImpl::ClearResources() {
  if (decoder_.get()) {
    decoder_->Destroy();
  }
}

bool ESMemHandlerImpl::Process() {
  using EsPacketPtr = std::shared_ptr<EsPacket>;

  EsPacketPtr in;
  int timeoutMs  = 10000;
  bool ret = this->queue_->Pop(timeoutMs, in);
  if (!ret) {
    ESPacket pkt;
    pkt.flags = ESPacket::FLAG_EOS;
    decoder_->Process(&pkt);
    return false;
  }

  if (perf_manager_ != nullptr) {
    std::string thread_name = "cn-" + module_->GetName() + stream_id_;
    perf_manager_->Record(false, PerfManager::GetDefaultType(), module_->GetName(), in->pkt_.pts);
    perf_manager_->Record(PerfManager::GetDefaultType(), PerfManager::GetPrimaryKey(), std::to_string(in->pkt_.pts),
                          module_->GetName() + "_th", "'" + thread_name + "'");
  }

  if (in->pkt_.flags & ESPacket::FLAG_EOS) {
    LOG(INFO) << "Read EOS from file";
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
