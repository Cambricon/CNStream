/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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

#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <utility>

#include "cnedk_platform.h"
#include "cnedk_buf_surface_util.hpp"
#include "cnrt.h"
#include "cnstream_logging.hpp"
#include "data_handler_jpeg_mem.hpp"
#include "data_handler_util.hpp"
#include "data_source.hpp"
#include "platform_utils.hpp"
#include "profiler/module_profiler.hpp"
#include "profiler/pipeline_profiler.hpp"
#include "video_decoder.hpp"

namespace cnstream {

class ESJpegMemHandlerImpl : public IDecodeResult, public SourceRender, public IUserPool {
 public:
  explicit ESJpegMemHandlerImpl(DataSource *module, const ESJpegMemSourceParam &param, ESJpegMemHandler *handler)
      : SourceRender(handler),
        module_(module),
        handle_param_(param),
        handler_(*handler),
        stream_id_(handler->GetStreamId()) {}
  ~ESJpegMemHandlerImpl() {}

  bool Open();
  void Close();
  int Write(ESJpegPacket *pkt);

 private:
  DataSource *module_ = nullptr;
  DataSourceParam param_;
  ESJpegMemSourceParam handle_param_;
  ESJpegMemHandler &handler_;
  std::string stream_id_;
  CnedkPlatformInfo platform_info_;
  CnedkBufSurfaceCreateParams create_params_;

 private:
  bool InitDecoder();
  bool ProcessImage(ESJpegPacket *pkt);
  // IDecodeResult methods
  void OnDecodeError(DecodeErrorCode error_code) override;
  void OnDecodeFrame(cnedk::BufSurfWrapperPtr buf_surf) override;
  void OnDecodeEos() override;

  // IUserPool
  int CreatePool(CnedkBufSurfaceCreateParams *params, uint32_t block_count) override;
  void DestroyPool() override;
  void OnBufInfo(int width, int height, CnedkBufSurfaceColorFormat fmt) override;
  cnedk::BufSurfWrapperPtr GetBufSurface(int timeout_ms) override;

 private:
  std::shared_ptr<Decoder> decoder_ = nullptr;
  cnedk::BufPool pool_;
  bool pool_created_ = false;
  std::mutex mutex_;

  RwLock running_lock_;
  std::atomic<bool> running_{false};
  std::atomic<bool> eos_reached_{false};

  std::atomic<bool> generate_pts_{false};
  uint64_t fake_pts_ = 0;
  uint64_t pts_gap_ = 1;  // FIXME

  ModuleProfiler *module_profiler_ = nullptr;
  PipelineProfiler *pipeline_profiler_ = nullptr;
};  // class ESJpegMemHandlerImpl

std::shared_ptr<SourceHandler> CreateSource(DataSource *module, const std::string &stream_id,
                                            const ESJpegMemSourceParam &param) {
  if (!module || stream_id.empty()) {
    LOGE(SOURCE) << "CreateSource(): Create ESJpegMemHandler failed. source module and stream id must not be empty";
    return nullptr;
  }
  return std::make_shared<ESJpegMemHandler>(module, stream_id, param);
}

int Write(std::shared_ptr<SourceHandler>handler, ESJpegPacket* pkt) {
  auto handle = std::dynamic_pointer_cast<ESJpegMemHandler>(handler);
  return handle->Write(pkt);
}

ESJpegMemHandler::ESJpegMemHandler(DataSource *module, const std::string &stream_id, const ESJpegMemSourceParam &param)
    : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) ESJpegMemHandlerImpl(module, param, this);
}

ESJpegMemHandler::~ESJpegMemHandler() {
  if (impl_) {
    delete impl_, impl_ = nullptr;
  }
}

bool ESJpegMemHandler::Open() {
  if (!this->module_) {
    LOGE(SOURCE) << "[ESJpegMemHandler] Open(): [" << stream_id_ << "]: module_ is null";
    return false;
  }
  if (!impl_) {
    LOGE(SOURCE) << "[ESJpegMemHandler] Open(): [" << stream_id_ << "]: no memory left";
    return false;
  }

  if (stream_index_ == kInvalidStreamIdx) {
    LOGE(SOURCE) << "[ESJpegMemHandler] Open(): [" << stream_id_ << "]: invalid stream_idx";
    return false;
  }

  return impl_->Open();
}

void ESJpegMemHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

int ESJpegMemHandler::Write(ESJpegPacket *pkt) {
  if (impl_) {
    return impl_->Write(pkt);
  }
  return -1;
}

bool ESJpegMemHandlerImpl::Open() {
  RwLockWriteGuard guard(running_lock_);
  DataSource *source = dynamic_cast<DataSource *>(module_);
  if (nullptr == source) {
    LOGE(SOURCE) << "[ESJpegMemHandlerImpl] Open(): [" << stream_id_ << "]: source module is null";
    return false;
  }
  param_ = source->GetSourceParam();
  cnrtSetDevice(param_.device_id);
  if (CnedkPlatformGetInfo(param_.device_id, &platform_info_) < 0) {
    LOGE(SOURCE) << "[ESJpegMemHandlerImpl] Open(): Get platform information failed";
    return false;
  }
  std::string platform(platform_info_.name);

  if (handle_param_.out_res.width > 0 && handle_param_.out_res.height > 0) {
    LOGI(SOURCE) << "[ESJpegMemHandlerImpl] Open(): Create pool";
    CnedkBufSurfaceCreateParams create_params;
    memset(&create_params, 0, sizeof(create_params));
    create_params.device_id = param_.device_id;
    create_params.batch_size = 1;
    create_params.color_format = CNEDK_BUF_COLOR_FORMAT_NV12;
    create_params.width = handle_param_.out_res.width;
    create_params.height = handle_param_.out_res.height;
    if (IsEdgePlatform(platform)) {
      create_params.mem_type = CNEDK_BUF_MEM_VB_CACHED;
    } else {
      create_params.mem_type = CNEDK_BUF_MEM_DEVICE;
    }
    if (CreatePool(&create_params, param_.bufpool_size) < 0) {
      LOGE(SOURCE) << "[ESJpegMemHandlerImpl] Open(): Create pool failed";
      return false;
    }
  }

  if (!module_profiler_) {
    if (module_) module_profiler_ = module_->GetProfiler();
    if (!pipeline_profiler_) {
      if (module_->GetContainer()) pipeline_profiler_ = module_->GetContainer()->GetProfiler();
    }
  }

  bool ret = InitDecoder();
  if (ret) {
    running_ = true;
    eos_reached_ = false;
  }
  return ret;
}

void ESJpegMemHandlerImpl::Close() {
  RwLockWriteGuard guard(running_lock_);
  if (decoder_) {
    decoder_->Destroy();
    decoder_ = nullptr;
  }
  running_ = false;
  LOGI(SOURCE) << "[ESJpegMemHandlerImpl] Close(): this(" << this << ") Destroy pool";
  DestroyPool();
}

int ESJpegMemHandlerImpl::Write(ESJpegPacket *pkt) {
  if (!pkt || eos_reached_ || !running_.load()) {
    return -1;
  }

  if (!pkt->has_pts) {
    generate_pts_ = true;
  }

  if (pkt && decoder_) {
    if (ProcessImage(pkt)) return 0;
  }

  return -1;
}

bool ESJpegMemHandlerImpl::ProcessImage(ESJpegPacket *in_pkt) {
  RwLockReadGuard guard(running_lock_);
  if (eos_reached_ || !running_) {
    return false;
  }
  if (!in_pkt->data || in_pkt->size == 0) {
    LOGI(SOURCE) << "[ESJpegMemHandlerImpl] ProcessImage(): [" << stream_id_ << "]: EOS reached";
    decoder_->Process(nullptr);
    return true;
  }

  VideoEsPacket pkt;
  pkt.data = in_pkt->data;
  pkt.len = in_pkt->size;
  pkt.pts = generate_pts_ ? (fake_pts_ += pts_gap_) : in_pkt->pts;

  if (module_profiler_) {
    auto record_key = std::make_pair(stream_id_, pkt.pts);
    module_profiler_->RecordProcessStart(kPROCESS_PROFILER_NAME, record_key);
    if (pipeline_profiler_) {
      pipeline_profiler_->RecordInput(record_key);
    }
  }

  if (!decoder_->Process(&pkt)) {
    LOGI(SOURCE) << "[ESJpegMemHandlerImpl] ProcessImage(): [" << stream_id_ << "]: decode failed";
    return false;
  }

  return true;
}

bool ESJpegMemHandlerImpl::InitDecoder() {
  MluDeviceGuard guard(param_.device_id);
  decoder_ = std::make_shared<MluDecoder>(stream_id_, this, this);
  if (!decoder_) {
    LOGE(SOURCE) << "[ESJpegMemHandlerImpl] InitDecoder(): Create decoder failed. Decoder is nullptr";
    return false;
  }

  decoder_->SetPlatformName(platform_info_.name);

  // FIXME, fill info, a parser is needed?
  VideoInfo info;
  info.codec_id = AV_CODEC_ID_MJPEG;

  ExtraDecoderInfo extra;
  extra.device_id = param_.device_id;
  extra.max_width = handle_param_.max_res.width;
  extra.max_height = handle_param_.max_res.height;
  bool ret = decoder_->Create(&info, &extra);
  if (!ret) {
    LOGE(SOURCE) << "[ESJpegMemHandlerImpl] InitDecoder(): Create decoder failed, ret = " << ret;
    return false;
  }
  return true;
}

// IDecodeResult methods
void ESJpegMemHandlerImpl::OnDecodeError(DecodeErrorCode error_code) {
  // FIXME,  handle decode error ...
  if (nullptr != module_) {
    Event e;
    e.type = EventType::EVENT_STREAM_ERROR;
    e.module_name = module_->GetName();
    e.message = "Decode failed.";
    e.stream_id = stream_id_;
    e.thread_id = std::this_thread::get_id();
    module_->PostEvent(e);
  }
  interrupt_.store(true);
}

void ESJpegMemHandlerImpl::OnDecodeFrame(cnedk::BufSurfWrapperPtr wrapper) {
  if (frame_count_++ % param_.interval != 0) {
    return;  // discard frames
  }
  std::shared_ptr<CNFrameInfo> data = this->CreateFrameInfo();
  if (!data) {
    LOGW(SOURCE) << "[ESJpegMemHandlerImpl] OnDecodeFrame(): failed to create FrameInfo.";
    return;
  }


  data->timestamp = wrapper->GetPts();
  if (!wrapper->GetBufSurface()) {
    data->flags = static_cast<size_t>(CNFrameFlag::CN_FRAME_FLAG_INVALID);
    this->SendFrameInfo(data);
    return;
  }
  int ret = SourceRender::Process(data, std::move(wrapper), frame_id_++, param_);
  if (ret < 0) {
    LOGE(SOURCE) << "[ESJpegMemHandlerImpl] OnDecodeFrame(): [" << stream_id_ << "]: Render frame failed";
    return;
  }
  this->SendFrameInfo(data);
}
void ESJpegMemHandlerImpl::OnDecodeEos() {
  this->SendFlowEos();
  LOGI(SOURCE) << "[ESJpegMemHandlerImpl] OnDecodeEos(): called";
}

int ESJpegMemHandlerImpl::CreatePool(CnedkBufSurfaceCreateParams *params, uint32_t block_count) {
  std::unique_lock<std::mutex> lk(mutex_);
  if (!pool_.CreatePool(params, block_count)) {
    pool_created_ = true;
    return 0;
  }
  LOGE(SOURCE) << "[ESJpegMemHandlerImpl] CreatePool(): Create pool failed.";
  return -1;
}

void ESJpegMemHandlerImpl::DestroyPool() {
  std::unique_lock<std::mutex> lk(mutex_);
  if (pool_created_) {
    pool_.DestroyPool(5000);
  }
}

void ESJpegMemHandlerImpl::OnBufInfo(int width, int height, CnedkBufSurfaceColorFormat fmt) {
  // TODO(liujian): we create according to the first time (ignore Buffer Info changing).
  std::string platform(platform_info_.name);
  if (IsEdgePlatform(platform)) {
    std::unique_lock<std::mutex> lk(mutex_);
    if (pool_created_) return;
    LOGI(SOURCE) << "[ESJpegMemHandlerImpl] OnBufInfo() Create pool";
    memset(&create_params_, 0, sizeof(CnedkBufSurfaceCreateParams));
    create_params_.device_id = param_.device_id;
    create_params_.batch_size = 1;
    if (fmt != CNEDK_BUF_COLOR_FORMAT_NV12 && fmt == CNEDK_BUF_COLOR_FORMAT_NV21) {
      create_params_.color_format = CNEDK_BUF_COLOR_FORMAT_NV12;
    }
    create_params_.width = width;
    create_params_.height = height;
    create_params_.mem_type = CNEDK_BUF_MEM_VB_CACHED;

    if (!pool_.CreatePool(&create_params_, param_.bufpool_size)) {
      pool_created_ = true;
    } else {
      LOGE(SOURCE) << "[ESJpegMemHandlerImpl] OnBufInfo() Create pool failed";
    }
  } else if (IsCloudPlatform(platform)) {
    memset(&create_params_, 0, sizeof(CnedkBufSurfaceCreateParams));
    create_params_.width = width;
    create_params_.height = height;
    create_params_.device_id = param_.device_id;
    create_params_.batch_size = 1;
    create_params_.color_format = fmt;
    create_params_.mem_type = CNEDK_BUF_MEM_DEVICE;
    return;
  }
}

cnedk::BufSurfWrapperPtr ESJpegMemHandlerImpl::GetBufSurface(int timeout_ms) {
  std::string platform(platform_info_.name);
  if (IsEdgePlatform(platform)) {
    std::unique_lock<std::mutex> lk(mutex_);
    return pool_.GetBufSurfaceWrapper(timeout_ms);
  } else if (IsCloudPlatform(platform)) {
    if (pool_created_) {
      std::unique_lock<std::mutex> lk(mutex_);
      return pool_.GetBufSurfaceWrapper(timeout_ms);
    }
    CnedkBufSurface *surf = nullptr;
    if (CnedkBufSurfaceCreate(&surf, &create_params_) < 0) {
        LOGE(SOURCE) << "[ESJpegMemHandlerImpl] GetBufSurface() Create BufSurface failed.";
      return nullptr;
    }
    return std::make_shared<cnedk::BufSurfaceWrapper>(surf);
  }
  return nullptr;
}

}  // namespace cnstream
