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
#include "data_handler_mem.hpp"
#include "data_handler_util.hpp"
#include "data_source.hpp"
#include "platform_utils.hpp"
#include "profiler/module_profiler.hpp"
#include "profiler/pipeline_profiler.hpp"
#include "video_decoder.hpp"

namespace cnstream {

class ESMemHandlerImpl : public IParserResult, public IDecodeResult, public SourceRender, public IUserPool {
 public:
  explicit ESMemHandlerImpl(DataSource *module, const ESMemSourceParam &param, ESMemHandler *handler)
      : SourceRender(handler),
        module_(module),
        handle_param_(param),
        handler_(*handler),
        stream_id_(handler->GetStreamId()) {}
  ~ESMemHandlerImpl() {}

  bool Open();
  void Close();
  void Stop();
  int Write(ESPacket *pkt);

  // IParserResult methods
  void OnParserInfo(VideoInfo *info) override;
  void OnParserFrame(VideoEsFrame *frame) override;

 private:
  DataSource *module_ = nullptr;
  DataSourceParam param_;
  ESMemSourceParam handle_param_;
  ESMemHandler &handler_;
  std::string stream_id_;
  CnedkPlatformInfo platform_info_;
  CnedkBufSurfaceCreateParams create_params_;

 private:
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
  bool PrepareResources();
  void ClearResources();
  bool Process();
  void DecodeLoop();

 private:
  std::mutex info_mutex_;
  VideoInfo info_{};
  std::atomic<bool> info_set_{false};

  std::shared_ptr<Decoder> decoder_ = nullptr;
  cnedk::BufPool pool_;
  bool pool_created_ = false;
  std::mutex mutex_;

  EsParser parser_;
  std::mutex queue_mutex_;
  BoundedQueue<std::shared_ptr<EsPacket>> *queue_ = nullptr;

  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> eos_reached_{false};

  std::atomic<bool> generate_pts_{false};
  uint64_t fake_pts_ = 0;
  uint64_t pts_gap_ = 3003;  // FIXME

  ModuleProfiler *module_profiler_ = nullptr;
  PipelineProfiler *pipeline_profiler_ = nullptr;
};  // class ESMemHandlerImpl

std::shared_ptr<SourceHandler> CreateSource(DataSource *module, const std::string &stream_id,
                                            const ESMemSourceParam &param) {
  if (!module || stream_id.empty()) {
    LOGE(SOURCE) << "CreateSource(): Create ESMemHandler failed. source module and stream id must not be empty";
    return nullptr;
  }
  return std::make_shared<ESMemHandler>(module, stream_id, param);
}

int Write(std::shared_ptr<SourceHandler>handler, ESPacket* pkt) {
  auto handle = std::dynamic_pointer_cast<ESMemHandler>(handler);
  return handle->Write(pkt);
}

ESMemHandler::ESMemHandler(DataSource *module, const std::string &stream_id, const ESMemSourceParam &param)
    : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) ESMemHandlerImpl(module, param, this);
}

ESMemHandler::~ESMemHandler() {
  if (impl_) {
    delete impl_, impl_ = nullptr;
  }
}

bool ESMemHandler::Open() {
  if (!this->module_) {
    LOGE(SOURCE) << "[ESMemHandler] Open(): [" << stream_id_ << "]: module_ is null";
    return false;
  }
  if (!impl_) {
    LOGE(SOURCE) << "[ESMemHandler] Open(): [" << stream_id_ << "]: no memory left";
    return false;
  }

  if (stream_index_ == kInvalidStreamIdx) {
    LOGE(SOURCE) << "[ESMemHandler] Open(): [" << stream_id_ << "]: invalid stream_idx";
    return false;
  }

  return impl_->Open();
}

void ESMemHandler::Stop() {
  if (impl_) {
    impl_->Stop();
  }
}

void ESMemHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

int ESMemHandler::Write(ESPacket *pkt) {
  if (impl_) {
    return impl_->Write(pkt);
  }
  return -1;
}

bool ESMemHandlerImpl::Open() {
  DataSource *source = dynamic_cast<DataSource *>(module_);
  if (nullptr == source) {
    LOGE(SOURCE) << "[ESMemHandlerImpl] Open(): [" << stream_id_ << "]: source module is null";
    return false;
  }
  param_ = source->GetSourceParam();
  cnrtSetDevice(param_.device_id);
  if (CnedkPlatformGetInfo(param_.device_id, &platform_info_) < 0) {
    LOGE(SOURCE) << "[ESMemHandlerImpl] Open(): Get platform information failed";
    return false;
  }
  std::string platform(platform_info_.name);

  if (handle_param_.out_res.width > 0 && handle_param_.out_res.height > 0) {
    LOGI(SOURCE) << "[ESMemHandlerImpl] Open(): Create pool";
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
      LOGE(SOURCE) << "[ESMemHandlerImpl] Open(): Create pool failed";
      return false;
    }
  }

  if (!module_profiler_) {
    if (module_) module_profiler_ = module_->GetProfiler();
    if (!pipeline_profiler_) {
      if (module_->GetContainer()) pipeline_profiler_ = module_->GetContainer()->GetProfiler();
    }
  }

  size_t max_size = 60;  // FIXME
  queue_ = new (std::nothrow) cnstream::BoundedQueue<std::shared_ptr<EsPacket>>(max_size);
  if (!queue_) {
    LOGE(SOURCE) << "[ESMemHandlerImpl] Open(): failed to create BoundedQueue.";
    return false;
  }

  // start decode Loop
  running_.store(true);
  thread_ = std::thread(&ESMemHandlerImpl::DecodeLoop, this);

  int ret = -1;
  if (handle_param_.data_type == ESMemSourceParam::DataType::H264) {
    ret = parser_.Open(AV_CODEC_ID_H264, this, nullptr, 0, handle_param_.only_key_frame);
  } else if (handle_param_.data_type == ESMemSourceParam::DataType::H265) {
    ret = parser_.Open(AV_CODEC_ID_HEVC, this, nullptr, 0, handle_param_.only_key_frame);
  } else {
    LOGF(SOURCE) << "[ESMemHandlerImpl] Open(): Unsupported data type " << static_cast<int>(handle_param_.data_type);
    ret = -1;
  }
  if (ret < 0) return false;
  return true;
}

void ESMemHandlerImpl::Stop() {
  if (running_.load()) running_.store(false);
}

void ESMemHandlerImpl::Close() {
  Stop();
  if (thread_.joinable()) {
    thread_.join();
  }

  {
    std::lock_guard<std::mutex> lk(queue_mutex_);
    if (queue_) {
      delete queue_;
      queue_ = nullptr;
    }
  }
  parser_.Close();
  LOGI(SOURCE) << "[ESMemHandlerImpl] Close(): this(" << this << ") Destroy pool";
  DestroyPool();
}

int ESMemHandlerImpl::Write(ESPacket *pkt) {
  if (!pkt || eos_reached_ || !running_.load()) {
    return -1;
  }
  if (!pkt->has_pts) {
    generate_pts_ = true;
  }
  // There are 4 situations:
  //   1. normal packet:           Parse data
  //   2. normal end packet:       Parse data, which is nullptr, to notify parser its the end
  //   3. eos packet without data: Parse Eos
  //   4. eos packet with data:    Parse data and Parse Eos
  if ((pkt->data && pkt->size) || !(pkt->flags & static_cast<uint32_t>(ESPacket::FLAG::FLAG_EOS))) {
    VideoEsPacket packet;
    packet.data = pkt->data;
    packet.len = pkt->size;
    packet.pts = pkt->pts;
    if (parser_.Parse(packet) < 0) {
      eos_reached_ = true;
      return -1;
    }
  }

  if (pkt->flags & static_cast<uint32_t>(ESPacket::FLAG::FLAG_EOS)) {
    if (parser_.ParseEos() < 0) {
      return -1;
    }
    eos_reached_ = true;
    return 0;
  }
  return 0;
}

void ESMemHandlerImpl::OnParserInfo(VideoInfo *video_info) {
  // FIXME
  if (!video_info) {
    LOGE(SOURCE) << "[ESMemHandlerImpl] OnParserInfo(): [" << stream_id_ << "]: null info ";
    return;
  }
  std::unique_lock<std::mutex> lk(info_mutex_);
  info_ = *video_info;
  info_set_.store(true);
  LOGI(SOURCE) << "[ESMemHandlerImpl] OnParserInfo(): [" << stream_id_ << "]: Got video info.";
}

void ESMemHandlerImpl::OnParserFrame(VideoEsFrame *frame) {
  ESPacket pkt;
  if (frame) {
    pkt.data = frame->data;
    pkt.size = frame->len;
    pkt.pts = generate_pts_ ? (fake_pts_ += pts_gap_) : frame->pts;
    if (frame->IsEos()) {
      pkt.flags = static_cast<size_t>(ESPacket::FLAG::FLAG_EOS);
      eos_reached_ = true;
      LOGI(SOURCE) << "[ESMemHandlerImpl] OnParserFrame(): [" << stream_id_ << "]: " << "EOS reached";
    } else {
      pkt.flags = frame->flags ? static_cast<size_t>(ESPacket::FLAG::FLAG_KEY_FRAME) : 0;
    }
  } else {
    pkt.flags = static_cast<size_t>(ESPacket::FLAG::FLAG_EOS);
    eos_reached_ = true;
    LOGI(SOURCE) << "[ESMemHandlerImpl] OnParserFrame(): [" << stream_id_ << "]: " << "EOS reached";
  }
  while (running_.load()) {
    int timeoutMs = 1000;
    std::lock_guard<std::mutex> lk(queue_mutex_);
    if (queue_ && queue_->Push(timeoutMs, std::make_shared<EsPacket>(&pkt))) {
      break;
    }
    if (!queue_) {
      LOGW(SOURCE) << "[ESMemHandlerImpl] OnParserFrame(): Frame queue doesn't exist";
      return;
    }
  }
}

void ESMemHandlerImpl::DecodeLoop() {
  /*meet cnrt requirement,
   *  for cpu case(device_id < 0), MluDeviceGuard will do nothing
   */
  MluDeviceGuard guard(param_.device_id);

  if (!PrepareResources()) {
    ClearResources();
    if (eos_reached_ && !info_set_.load()) {
      LOGW(SOURCE) << "[ESMemHandlerImpl] DecodeLoop(): PrepareResources failed, can not get video info.";
    } else {
      if (nullptr != module_) {
        Event e;
        e.type = EventType::EVENT_STREAM_ERROR;
        e.module_name = module_->GetName();
        e.message = "Prepare codec resources failed.";
        e.stream_id = stream_id_;
        e.thread_id = std::this_thread::get_id();
        module_->PostEvent(e);
      }
      LOGE(SOURCE) << "[ESMemHandlerImpl] DecodeLoop(): PrepareResources failed.";
    }
    return;
  }

  VLOG1(SOURCE) << "[ESMemHandlerImpl] DecodeLoop(): [" << stream_id_ << "] Loop.";
  while (running_.load()) {
    if (!Process()) {
      break;
    }
  }

  VLOG1(SOURCE) << "[ESMemHandlerImpl] DecodeLoop(): [" << stream_id_ << "]: Loop Exit.";
  ClearResources();
}


bool ESMemHandlerImpl::PrepareResources() {
  VLOG1(SOURCE) << "[ESMemHandlerImpl] PrepareResources(): [" << stream_id_ << "]: Begin to preprare";
  VideoInfo info;
  while (running_.load()) {
    if (eos_reached_ && !info_set_.load()) {
      break;
    }
    if (info_set_.load()) {
      std::unique_lock<std::mutex> lk(info_mutex_);
      info = info_;
      break;
    }
    usleep(1000);
  }

  if (!running_.load()) {
    LOGE(SOURCE) << "[ESMemHandlerImpl] PrepareResources(): not running";
    return false;
  }

  if (eos_reached_ && !info_set_.load()) {
    OnDecodeEos();
    return false;
  }

  decoder_ = std::make_shared<MluDecoder>(stream_id_, this, this);
  if (!decoder_) {
    LOGE(SOURCE) << "[ESMemHandlerImpl] PrepareResources(): Create decoder failed. Decoder is nullptr";
    return false;
  }
  decoder_->SetPlatformName(platform_info_.name);

  ExtraDecoderInfo extra;
  extra.device_id = param_.device_id;
  extra.max_width = handle_param_.max_res.width;
  extra.max_height = handle_param_.max_res.height;
  bool ret = decoder_->Create(&info, &extra);
  if (!ret) {
    LOGE(SOURCE) << "[ESMemHandlerImpl] PrepareResources(): Create decoder failed, ret = " << ret;
    return false;
  }
  VLOG1(SOURCE) << "[ESMemHandlerImpl] PrepareResources(): [" << stream_id_ << "]: Finish prepraring resources";
  return true;
}

void ESMemHandlerImpl::ClearResources() {
  VLOG1(SOURCE) << "[ESMemHandlerImpl] ClearResources(): [" << stream_id_ << "]: Begin to clear resources";
  if (decoder_.get()) {
    decoder_->Destroy();
  }
  VLOG1(SOURCE) << "[ESMemHandlerImpl] ClearResources():[" << stream_id_ << "]: Finish clearing resources";
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

  if (in->pkt_.flags & static_cast<size_t>(ESPacket::FLAG::FLAG_EOS)) {
    LOGI(SOURCE) << "[ESMemHandlerImpl] Process(): [" << stream_id_ << "]: " << " Process EOS frame";
    decoder_->Process(nullptr);
    return false;
  }  // if (!ret)

  VideoEsPacket pkt;
  pkt.data = in->pkt_.data;
  pkt.len = in->pkt_.size;
  pkt.pts = in->pkt_.pts;

  if (module_profiler_) {
    auto record_key = std::make_pair(stream_id_, pkt.pts);
    module_profiler_->RecordProcessStart(kPROCESS_PROFILER_NAME, record_key);
    if (pipeline_profiler_) {
      pipeline_profiler_->RecordInput(record_key);
    }
  }

  if (!decoder_->Process(&pkt)) {
    LOGI(SOURCE) << "[ESMemHandlerImpl] Process(): [" << stream_id_ << "]: decode failed";
    return false;
  }
  return true;
}

// IDecodeResult methods
void ESMemHandlerImpl::OnDecodeError(DecodeErrorCode error_code) {
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

void ESMemHandlerImpl::OnDecodeFrame(cnedk::BufSurfWrapperPtr wrapper) {
  if (frame_count_++ % param_.interval != 0) {
    return;  // discard frames
  }
  std::shared_ptr<CNFrameInfo> data = this->CreateFrameInfo();
  if (!data) {
    LOGW(SOURCE) << "[ESMemHandlerImpl] OnDecodeFrame(): failed to create FrameInfo.";
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
    LOGE(SOURCE) << "[ESMemHandlerImpl] OnDecodeFrame(): [" << stream_id_ << "]: Render frame failed";
    return;
  }
  this->SendFrameInfo(data);
}
void ESMemHandlerImpl::OnDecodeEos() {
  this->SendFlowEos();
  LOGI(SOURCE) << "[ESMemHandlerImpl] OnDecodeEos(): called";
}

// IUserPool
int ESMemHandlerImpl::CreatePool(CnedkBufSurfaceCreateParams *params, uint32_t block_count) {
  std::unique_lock<std::mutex> lk(mutex_);
  if (!pool_.CreatePool(params, block_count)) {
    pool_created_ = true;
    return 0;
  }
  LOGE(SOURCE) << "[ESMemHandlerImpl] CreatePool(): Create pool failed.";
  return -1;
}

void ESMemHandlerImpl::DestroyPool() {
  std::unique_lock<std::mutex> lk(mutex_);
  if (pool_created_) {
    pool_.DestroyPool(5000);
  }
}

void ESMemHandlerImpl::OnBufInfo(int width, int height, CnedkBufSurfaceColorFormat fmt) {
  // TODO(liujian): we create according to the first time (ignore Buffer Info changing).
  std::string platform(platform_info_.name);
  if (IsEdgePlatform(platform)) {
    std::unique_lock<std::mutex> lk(mutex_);
    if (pool_created_) return;
    LOGI(SOURCE) << "[ESMemHandlerImpl] OnBufInfo() Create pool";
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
      LOGE(SOURCE) << "[ESMemHandlerImpl] OnBufInfo() Create pool failed";
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

cnedk::BufSurfWrapperPtr ESMemHandlerImpl::GetBufSurface(int timeout_ms) {
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
        LOGE(SOURCE) << "[ESMemHandlerImpl] GetBufSurface() Create BufSurface failed.";
      return nullptr;
    }
    return std::make_shared<cnedk::BufSurfaceWrapper>(surf);
  }
  return nullptr;
}
}  // namespace cnstream
