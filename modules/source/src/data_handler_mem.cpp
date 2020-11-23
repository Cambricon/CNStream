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
#include "data_handler_mem.hpp"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <memory>
#include <vector>

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
  impl_ = new (std::nothrow) ESMemHandlerImpl(module, this);
}

ESMemHandler::~ESMemHandler() {
  if (impl_) {
    delete impl_, impl_ = nullptr;
  }
}

bool ESMemHandler::Open() {
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

void ESMemHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

int ESMemHandler::SetDataType(ESMemHandler::DataType type) {
  if (impl_) {
    return impl_->SetDataType(type);
  }
  return -1;
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
  DataSource *source = dynamic_cast<DataSource *>(module_);
  param_ = source->GetSourceParam();
  /*
  if (param_.decoder_type_ != DECODER_MLU) {
    LOGE(SOURCE) << "decoder_type not supported:" << param_.decoder_type_;
    return false;
  }
  */

  SetPerfManager(source->GetPerfManager(stream_id_));
  SetThreadName(module_->GetName(), handler_.GetStreamUniqueIdx());

  size_t MaxSize = 60;  // FIXME
  queue_ = new (std::nothrow) cnstream::BoundedQueue<std::shared_ptr<EsPacket>>(MaxSize);
  if (!queue_) {
    return false;
  }

  // start decode Loop
  running_.store(true);
  thread_ = std::thread(&ESMemHandlerImpl::DecodeLoop, this);
  return true;
}

void ESMemHandlerImpl::Close() {
  if (running_.load()) {
    running_.store(false);
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  {
    std::lock_guard<std::mutex> lk(queue_mutex_);
    if (queue_) {
      delete queue_;
      queue_ = nullptr;
    }
  }
  parser_.Close();
}

int ESMemHandlerImpl::Write(ESPacket *pkt) {
  if (!pkt || eos_reached_ || !running_.load()) {
    return -1;
  }
  VideoEsPacket packet;
  packet.data = pkt->data;
  packet.len = pkt->size;
  packet.pts = pkt->pts;
  if (parser_.Parse(packet) < 0) {
    eos_reached_ = true;
    return -1;
  }
  return 0;
}

struct NalDesc {
  unsigned char *nal = nullptr;
  int len = 0;
};

static int FindStartCode(unsigned char *buf) {
  if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1) {
    return 4;
  }
  if (buf[0] == 0 && buf[1] == 0 && buf[2] == 1) {
    return 3;
  }
  return 0;
}

static int GetNaluH2645(unsigned char *buf, int len, std::vector<NalDesc> &vec_desc) {  // NOLINT
  std::vector<int> vec_pos;
  for (int i = 0; i < len - 4; i++) {
    int size = FindStartCode(buf + i);
    if (!size) {
      continue;
    }
    vec_pos.push_back(i);
    i += size - 1;
  }
  if (vec_pos.empty()) {
    return 0;
  }

  int num = vec_pos.size();
  for (int i = 0; i < num - 1; i++) {
    NalDesc desc;
    desc.nal = buf + vec_pos[i];
    desc.len = vec_pos[i + 1] - vec_pos[i];
    int type_idx = (desc.nal[2] == 1) ? 3 : 4;
    if (desc.len < type_idx) {
      // "INVALID nal size"
      return -1;
    }
    vec_desc.push_back(desc);
  }

  // handle the last nal
  if (vec_pos[num - 1]) {
    NalDesc desc;
    desc.nal = buf + vec_pos[num - 1];
    desc.len = len - vec_pos[num - 1];
    vec_desc.push_back(desc);
  }
  return 0;
}

static const size_t max_frame_bits_size = 2 * 1024 * 1024;
int ESMemHandlerImpl::Write(unsigned char *data, int len) {
  if (eos_reached_ || !running_.load()) {
    return -1;
  }

  if (!frame_bits_buf_) {
    std::unique_ptr<unsigned char[]> ptr(new unsigned char[max_frame_bits_size]);
    frame_bits_buf_ = std::move(ptr);
    if (!frame_bits_buf_) {
      return -1;
    }
    frame_bits_size_ = 0;
  }
  generate_pts_ = true;

  if (!data || !len) {
    // eos reached
    VideoEsPacket packet;
    parser_.Parse(packet);
    eos_reached_ = true;
    return 0;
  }

  if (frame_bits_size_ + len <= max_frame_bits_size) {
    memcpy(frame_bits_buf_.get() + frame_bits_size_, data, len);
    frame_bits_size_ += len;

    std::vector<NalDesc> vec_desc;
    GetNaluH2645(frame_bits_buf_.get(), frame_bits_size_, vec_desc);

    size_t parsed_len = 0;
    if (vec_desc.size() > 1) {
      for (size_t i = 0; i < vec_desc.size() - 1; i++) {
        VideoEsPacket packet;
        packet.data = vec_desc[i].nal;
        packet.len = vec_desc[i].len;
        packet.pts = 0;
        if (parser_.Parse(packet) < 0) {
          eos_reached_ = true;
          return -1;
        }
        parsed_len += packet.len;
      }
    }
    if (parsed_len) {
      if (frame_bits_size_ - parsed_len) {
        memmove(frame_bits_buf_.get(), frame_bits_buf_.get() + parsed_len, frame_bits_size_ - parsed_len);
      }
    }
    frame_bits_size_ -= parsed_len;
  } else {
    // FIXME
    LOGW(SOURCE) << " parse es failed, discard data";
    frame_bits_size_ = 0;
  }
  return 0;
}

void ESMemHandlerImpl::OnParserInfo(VideoInfo *video_info) {
  // FIXME
  if (!video_info) {
    LOGE(SOURCE) << "ESMemHandlerImpl::OnParserInfo null info ";
    return;
  }
  std::unique_lock<std::mutex> lk(info_mutex_);
  info_ = *video_info;
  info_set_.store(true);
}

void ESMemHandlerImpl::OnParserFrame(VideoEsFrame *frame) {
  ESPacket pkt;
  if (frame) {
    pkt.data = frame->data;
    pkt.size = frame->len;
    pkt.pts = generate_pts_ ? (fake_pts_ ++) : frame->pts;
    pkt.flags = frame->flags ? ESPacket::FLAG_KEY_FRAME : 0;
  } else {
    pkt.flags = ESPacket::FLAG_EOS;
  }
  while (running_) {
    int timeoutMs = 1000;
    std::lock_guard<std::mutex> lk(queue_mutex_);
    if (queue_ && queue_->Push(timeoutMs, std::make_shared<EsPacket>(&pkt))) {
      break;
    }
    if (!queue_) {
      return;
    }
  }
}

void ESMemHandlerImpl::DecodeLoop() {
  /*meet cnrt requirement,
   *  for cpu case(device_id < 0), MluDeviceGuard will do nothing
   */
  MluDeviceGuard guard(param_.device_id_);

  if (!PrepareResources()) {
    ClearResources();
    if (nullptr != module_) {
      Event e;
      e.type = EventType::EVENT_STREAM_ERROR;
      e.module_name = module_->GetName();
      e.message = "Prepare codec resources failed.";
      e.stream_id = stream_id_;
      e.thread_id = std::this_thread::get_id();
      module_->PostEvent(e);
    }
    LOGE(SOURCE) << "PrepareResources failed.";
    return;
  }

  LOGD(SOURCE) << "Mem handler DecodeLoop.";
  while (running_.load()) {
    if (!Process()) {
      break;
    }
  }

  ClearResources();
  LOGD(SOURCE) << "Mem handler DecodeLoop Exit.";
}

bool ESMemHandlerImpl::PrepareResources() {
  VideoInfo info;
  while (running_.load()) {
    if (info_set_.load()) {
      std::unique_lock<std::mutex> lk(info_mutex_);
      info = info_;
      break;
    }
    usleep(1000);
  }

  if (!running_.load()) {
    return false;
  }

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

  ExtraDecoderInfo extra;
  extra.device_id = param_.device_id_;
  extra.input_buf_num = param_.input_buf_number_;
  extra.output_buf_num = param_.output_buf_number_;
  extra.apply_stride_align_for_scaler = param_.apply_stride_align_for_scaler_;
  bool ret = decoder_->Create(&info, &extra);
  if (!ret) {
    return false;
  }
  if (info.extra_data.size()) {
    VideoEsPacket pkt;
    pkt.data = info.extra_data.data();
    pkt.len = info.extra_data.size();
    pkt.pts = 0;
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
  int timeoutMs  = 1000;
  bool ret = this->queue_->Pop(timeoutMs, in);

  if (!ret) {
    // continue.. not exit
    return true;
  }

  if (in->pkt_.flags & ESPacket::FLAG_EOS) {
    LOGI(SOURCE) << "Mem handler stream id: " << stream_id_ << " EOS reached";
    decoder_->Process(nullptr);
    return false;
  }  // if (!ret)

  VideoEsPacket pkt;
  pkt.data = in->pkt_.data;
  pkt.len = in->pkt_.size;
  pkt.pts = in->pkt_.pts;

  RecordStartTime(module_->GetName(), pkt.pts);

  if (!decoder_->Process(&pkt)) {
    return false;
  }
  return true;
}

// IDecodeResult methods
void ESMemHandlerImpl::OnDecodeError(DecodeErrorCode error_code) {
  // FIXME,  handle decode error ...
  interrupt_.store(true);
}

void ESMemHandlerImpl::OnDecodeFrame(DecodeFrame *frame) {
  if (frame_count_++ % param_.interval_ != 0) {
    return;  // discard frames
  }
  if (!frame) return;

  std::shared_ptr<CNFrameInfo> data = this->CreateFrameInfo();
  if (!data) {
    return;
  }

  data->timestamp = frame->pts;  // FIXME
  if (!frame->valid) {
    data->flags = CN_FRAME_FLAG_INVALID;
    this->SendFrameInfo(data);
    return;
  }

  int ret = SourceRender::Process(data, frame, frame_id_++, param_);
  if (ret < 0) {
    return;
  }
  this->SendFrameInfo(data);
}

void ESMemHandlerImpl::OnDecodeEos() {
  this->SendFlowEos();
}

}  // namespace cnstream
