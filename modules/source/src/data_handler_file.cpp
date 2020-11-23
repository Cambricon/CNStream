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

#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "data_handler_file.hpp"

namespace cnstream {

std::shared_ptr<SourceHandler> FileHandler::Create(DataSource *module, const std::string &stream_id,
                                                   const std::string &filename, int framerate, bool loop) {
  if (!module || stream_id.empty() || filename.empty()) {
    return nullptr;
  }
  std::shared_ptr<FileHandler> handler(new (std::nothrow) FileHandler(module, stream_id, filename, framerate, loop));
  return handler;
}

FileHandler::FileHandler(DataSource *module, const std::string &stream_id, const std::string &filename, int framerate,
                         bool loop)
    : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) FileHandlerImpl(module, filename, framerate, loop, this);
}

FileHandler::~FileHandler() {
  if (impl_) {
    delete impl_;
  }
}

bool FileHandler::Open() {
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

void FileHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

bool FileHandlerImpl::Open() {
  DataSource *source = dynamic_cast<DataSource *>(module_);
  param_ = source->GetSourceParam();

  SetPerfManager(source->GetPerfManager(stream_id_));
  SetThreadName(module_->GetName(), handler_.GetStreamUniqueIdx());

  // start separate thread
  running_.store(1);
  thread_ = std::thread(&FileHandlerImpl::Loop, this);
  return true;
}

void FileHandlerImpl::Close() {
  if (running_.load()) {
    running_.store(0);
    if (thread_.joinable()) {
      thread_.join();
    }
  }
}

void FileHandlerImpl::Loop() {
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

  FrController controller(framerate_);
  if (framerate_ > 0) controller.Start();

  LOGD(SOURCE) << "File handler DecodeLoop";
  while (running_.load()) {
    if (!Process()) {
      break;
    }
    if (framerate_ > 0) controller.Control();
  }

  LOGD(SOURCE) << "File handler DecoderLoop Exit.";
  ClearResources();
}

bool FileHandlerImpl::PrepareResources(bool demux_only) {
  int ret = parser_.Open(filename_, this);
  if (ret < 0 || dec_create_failed_) {
    return false;
  }
  return true;
}

void FileHandlerImpl::ClearResources(bool demux_only) {
  if (!demux_only && decoder_) {
    decoder_->Destroy();
    decoder_.reset();
  }
  parser_.Close();
}

bool FileHandlerImpl::Process() {
  parser_.Parse();
  if (eos_reached_) {
    LOGI(SOURCE) << "Read EOS from file stream_id: " << stream_id_;
    if (this->loop_) {
      LOGI(SOURCE) << "Clear resources and restart";
      ClearResources(true);
      if (!PrepareResources(true)) {
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
        return false;
      }
      LOGI(SOURCE) << "Loop...";
      eos_reached_ = false;
      return true;
    } else {
      if (decoder_) decoder_->Process(nullptr);
      return false;
    }
    return false;
  }
  if (decode_failed_ || dec_create_failed_) {
    LOGE(SOURCE) << "Decode failed.";
    return false;
  }
  return true;
}

// IParserResult methods
void FileHandlerImpl::OnParserInfo(VideoInfo *info) {
  if (decoder_) {
    return;  // for the case:  loop and reset demux only
  }
  dec_create_failed_ = false;
  if (param_.decoder_type_ == DecoderType::DECODER_MLU) {
    decoder_ = std::make_shared<MluDecoder>(this);
  } else if (param_.decoder_type_ == DecoderType::DECODER_CPU) {
    decoder_ = std::make_shared<FFmpegCpuDecoder>(this);
  } else {
    LOGE(SOURCE) << "unsupported decoder_type";
    dec_create_failed_ = true;
  }
  if (decoder_) {
    ExtraDecoderInfo extra;
    extra.apply_stride_align_for_scaler = param_.input_buf_number_;
    extra.device_id = param_.device_id_;
    extra.input_buf_num = param_.input_buf_number_;
    extra.output_buf_num = param_.output_buf_number_;
    extra.max_width = 7680;  // FIXME
    extra.max_height = 4320;  // FIXME
    bool ret = decoder_->Create(info, &extra);
    if (ret != true) {
      LOGE(SOURCE) << "dec_create_failed_";
      dec_create_failed_ = true;
      return;
    }
    if (info->extra_data.size()) {
      VideoEsPacket pkt;
      pkt.data = info->extra_data.data();
      pkt.len = info->extra_data.size();
      pkt.pts = 0;
      if (!decoder_->Process(&pkt)) {
        decode_failed_ = true;
        LOGE(SOURCE) << "decode_failed_";
      }
    }
  }
}

void FileHandlerImpl::OnParserFrame(VideoEsFrame *frame) {
  if (!frame) {
    eos_reached_ = true;
    return;  // EOS will be handled in Process()
  }
  VideoEsPacket pkt;
  pkt.data = frame->data;
  pkt.len = frame->len;
  pkt.pts = frame->pts;

  RecordStartTime(module_->GetName(), pkt.pts);

  decode_failed_ = true;
  if (decoder_  && decoder_->Process(&pkt) == true) {
    decode_failed_ = false;
  }
}

// IDecodeResult methods
void FileHandlerImpl::OnDecodeError(DecodeErrorCode error_code) {
  // FIXME,  handle decode error ...
  LOGI("FileHandlerImpl::OnDecodeError() called");
  interrupt_.store(true);
}

void FileHandlerImpl::OnDecodeFrame(DecodeFrame *frame) {
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
void FileHandlerImpl::OnDecodeEos() {
  this->SendFlowEos();
}

}  // namespace cnstream
