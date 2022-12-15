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
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "cnrt.h"

#include "cnedk_platform.h"
#include "cnedk_buf_surface_util.hpp"
#include "cnstream_logging.hpp"
#include "data_handler_file.hpp"
#include "data_handler_util.hpp"
#include "data_source.hpp"
#include "platform_utils.hpp"
#include "profiler/module_profiler.hpp"
#include "profiler/pipeline_profiler.hpp"
#include "video_decoder.hpp"
#include "video_parser.hpp"

namespace cnstream {

class FileHandlerImpl : public IParserResult, public IDecodeResult, public SourceRender, public IUserPool {
 public:
  explicit FileHandlerImpl(DataSource *module, const FileSourceParam &param, FileHandler *handler)
      : SourceRender(handler),
        module_(module),
        handle_param_(param),
        handler_(*handler),
        stream_id_(handler_.GetStreamId()),
        parser_(stream_id_) {}
  ~FileHandlerImpl() { Close(); }
  bool Open();
  void Stop();
  void Close();

 private:
  DataSource *module_ = nullptr;
  FileSourceParam handle_param_;
  FileHandler &handler_;
  std::string stream_id_;
  DataSourceParam param_;
  CnedkPlatformInfo platform_info_;
  CnedkBufSurfaceCreateParams create_params_;

 private:
  bool PrepareResources(bool demux_only = false);
  void ClearResources(bool demux_only = false);
  bool Process();
  void Loop();

  // IParserResult methods
  void OnParserInfo(VideoInfo *info) override;
  void OnParserFrame(VideoEsFrame *frame) override;

  // IDecodeResult methods
  void OnDecodeError(DecodeErrorCode error_code) override;
  void OnDecodeFrame(cnedk::BufSurfWrapperPtr buf_surf) override;
  void OnDecodeEos() override;

  // IUserPool
  int CreatePool(CnedkBufSurfaceCreateParams *params, uint32_t block_count);
  void DestroyPool() override;
  void OnBufInfo(int width, int height, CnedkBufSurfaceColorFormat fmt);
  cnedk::BufSurfWrapperPtr GetBufSurface(int timeout_ms) override;

 private:
  /**/
  std::atomic<int> running_{0};
  std::thread thread_;
  bool eos_sent_ = false;

 private:
  FFParser parser_;
  std::shared_ptr<Decoder> decoder_ = nullptr;
  cnedk::BufPool pool_;
  bool pool_created_ = false;
  std::mutex mutex_;
  bool dec_create_failed_ = false;
  bool decode_failed_ = false;
  bool eos_reached_ = false;

  uint64_t timestamp_ = 0;
  uint64_t timestamp_base_ = 0;
  bool first_pts_set_ = false;
  uint64_t first_pts_ = 0;
  uint64_t pts_gap_ = 3003;  // FIXME
  ModuleProfiler *module_profiler_ = nullptr;
  PipelineProfiler *pipeline_profiler_ = nullptr;
};  // class FileHandlerImpl

std::shared_ptr<SourceHandler> CreateSource(DataSource *module, const std::string &stream_id,
                                            const FileSourceParam &param) {
  if (!module || stream_id.empty() || param.filename.empty()) {
    LOGE(SOURCE) << "CreateSource(): Create FileHandler failed."
                 << " source module, stream id and filename must not be empty.";
    return nullptr;
  }
  return std::make_shared<FileHandler>(module, stream_id, param);
}

FileHandler::FileHandler(DataSource *module, const std::string &stream_id, const FileSourceParam &param)
    : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) FileHandlerImpl(module, param, this);
}

FileHandler::~FileHandler() {
  if (impl_) delete impl_, impl_ = nullptr;
}

bool FileHandler::Open() {
  if (!this->module_) {
    LOGE(SOURCE) << "[FileHandler] Open(): [" << stream_id_ << "]: module_ null";
    return false;
  }
  if (!impl_) {
    LOGE(SOURCE) << "[FileHandler] Open(): [" << stream_id_ << "]: no memory left";
    return false;
  }

  if (stream_index_ == cnstream::kInvalidStreamIdx) {
    LOGE(SOURCE) << "[FileHandler] Open(): [" << stream_id_ << "]: Invalid stream_idx";
    return false;
  }

  return impl_->Open();
}

void FileHandler::Stop() {
  if (impl_) {
    impl_->Stop();
  }
}

void FileHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

bool FileHandlerImpl::Open() {
  DataSource *source = dynamic_cast<DataSource *>(module_);
  param_ = source->GetSourceParam();

  if (CnedkPlatformGetInfo(param_.device_id, &platform_info_) < 0) {
    LOGE(SOURCE) << "[FileHandlerImpl] Open(): Get platform information failed";
    return false;
  }
  std::string platform(platform_info_.name);

  if (handle_param_.out_res.width > 0 && handle_param_.out_res.height > 0) {
    LOGI(SOURCE) << "[FileHandlerImpl] Open(): Create pool";
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
      LOGE(SOURCE) << "[FileHandlerImpl] Open(): Create pool failed";
      return false;
    }
  }

  if (!module_profiler_) {
    if (module_) module_profiler_ = module_->GetProfiler();
    if (!pipeline_profiler_) {
      if (module_->GetContainer()) pipeline_profiler_ = module_->GetContainer()->GetProfiler();
    }
  }
  // start seperated thread
  running_.store(1);
  thread_ = std::thread(&FileHandlerImpl::Loop, this);
  return true;
}

void FileHandlerImpl::Stop() {
  if (running_.load()) {
    running_.store(0);
    if (thread_.joinable()) {
      thread_.join();
    }
  }
}

void FileHandlerImpl::Close() {
  Stop();
  LOGI(SOURCE) << "[FileHandlerImpl] Close(): this(" << this << ") Destroy pool";
  DestroyPool();
}

void FileHandlerImpl::Loop() {
  cnrtSetDevice(param_.device_id);
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
    LOGE(SOURCE) << "[FileHandlerImpl] Loop(): [" << stream_id_ << "]: PrepareResources failed.";
    return;
  }

  set_thread_name("demux_decode");

  FrController controller(handle_param_.framerate);
  if (handle_param_.framerate > 0) controller.Start();

  VLOG1(SOURCE) << "[FileHandlerImpl] Loop(): [" << stream_id_ << "]: DecoderLoop";
  while (running_.load()) {
    if (!Process()) {
      break;
    }
    if (handle_param_.framerate > 0) controller.Control();
  }

  VLOG1(SOURCE) << "[FileHandlerImpl] Loop(): [" << stream_id_ << "]: DecoderLoop Exit.";
  ClearResources();
}

bool FileHandlerImpl::PrepareResources(bool demux_only) {
  VLOG1(SOURCE) << "[FileHandlerImpl] PrepareResources(): [" << stream_id_ << "]: Begin preprare resources";
  int ret = parser_.Open(handle_param_.filename, this, handle_param_.only_key_frame);
  VLOG1(SOURCE) << "[FileHandlerImpl] PrepareResources(): [" << stream_id_ << "]: Finish preprare resources";
  if (ret < 0 || dec_create_failed_) {
    return false;
  }
  return true;
}

void FileHandlerImpl::ClearResources(bool demux_only) {
  VLOG1(SOURCE) << "[FileHandlerImpl] ClearResources(): [" << stream_id_ << "]: Begin clear resources";
  if (!demux_only && decoder_) {
    decoder_->Destroy();
    decoder_.reset();
  }
  parser_.Close();
  VLOG1(SOURCE) << "[FileHandlerImpl] ClearResources(): [" << stream_id_ << "]: Finish clear resources";
}

bool FileHandlerImpl::Process() {
  parser_.Parse();
  if (eos_reached_) {
    if (this->handle_param_.loop) {
      VLOG1(SOURCE) << "[FileHandlerImpl] Process(): [" << stream_id_ << "]: Loop: Clear resources and restart";
      ClearResources(true);
      if (!PrepareResources(true)) {
        ClearResources();
        if (nullptr != module_) {
          Event e;
          e.type = EventType::EVENT_STREAM_ERROR;
          e.module_name = module_->GetName();
          e.message = "Prepare codec resources failed";
          e.stream_id = stream_id_;
          e.thread_id = std::this_thread::get_id();
          module_->PostEvent(e);
        }
        LOGE(SOURCE) << "[FileHandlerImpl] Process(): [" << stream_id_ << "]: PrepareResources failed";
        return false;
      }
      eos_reached_ = false;
      timestamp_base_ = timestamp_ + pts_gap_;
      return true;
    } else {
      LOGI(SOURCE) << "[FileHandlerImpl] Process(): loop false, eos_reached";
      if (decoder_) decoder_->Process(nullptr);
      return false;
    }
    return false;
  }
  if (decode_failed_ || dec_create_failed_) {
    LOGE(SOURCE) << "[FileHandlerImpl] Process(): [" << stream_id_ << "]: Decode failed";
    return false;
  }
  return true;
}

// IParserResult methods
void FileHandlerImpl::OnParserInfo(VideoInfo *info) {
  if (decoder_) {
    return;  // for the case:  loop and reset demux only
  }
  LOGI(SOURCE) << "[FileHandlerImpl] OnParserInfo(): [" << stream_id_ << "]: Got video info.";
  dec_create_failed_ = false;
  decoder_ = std::make_shared<MluDecoder>(stream_id_, this, this);

  if (decoder_) {
    decoder_->SetPlatformName(platform_info_.name);
    ExtraDecoderInfo extra;
    extra.device_id = param_.device_id;
    extra.max_width = handle_param_.max_res.width;
    extra.max_height = handle_param_.max_res.height;
    bool ret = decoder_->Create(info, &extra);
    if (ret != true) {
      LOGE(SOURCE) << "[FileHandlerImpl] OnParserInfo(): Create decoder failed, ret = " << ret;
      dec_create_failed_ = true;
      return;
    }
  }
}

void FileHandlerImpl::OnParserFrame(VideoEsFrame *frame) {
  if (!frame) {
    VLOG1(SOURCE) << "[FileHandlerImpl] OnParserFrame(): [" << stream_id_ << "]: eos reached in file handler.";
    eos_reached_ = true;
    return;  // EOS will be handled in Process()
  }
  VideoEsPacket pkt;
  pkt.data = frame->data;
  pkt.len = frame->len;
  pkt.pts = frame->pts;

  if (this->handle_param_.loop) {
    if (!first_pts_set_) {
      first_pts_ = pkt.pts, first_pts_set_ = true;
    }
    // for loop case, correct PTS, TODO(liujian)
    timestamp_ = timestamp_base_ + (pkt.pts - first_pts_);
    pkt.pts = timestamp_;
  }

  if (module_profiler_) {
    auto record_key = std::make_pair(stream_id_, pkt.pts);
    module_profiler_->RecordProcessStart(kPROCESS_PROFILER_NAME, record_key);
    if (pipeline_profiler_) {
      pipeline_profiler_->RecordInput(record_key);
    }
  }

  decode_failed_ = true;
  if (decoder_ && decoder_->Process(&pkt) == true) {
    decode_failed_ = false;
  }
}

// IDecodeResult methods
void FileHandlerImpl::OnDecodeError(DecodeErrorCode error_code) {
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

void FileHandlerImpl::OnDecodeFrame(cnedk::BufSurfWrapperPtr wrapper) {
  if (frame_count_++ % param_.interval != 0) {
    // LOGI(SOURCE) << "frames are discarded" << frame_count_;
    return;  // discard frames
  }

  std::shared_ptr<CNFrameInfo> data = this->CreateFrameInfo();
  if (!data) {
    LOGW(SOURCE) << "[FileHandlerImpl] OnDecodeFrame(): failed to create FrameInfo.";
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
    LOGE(SOURCE) << "[FileHandlerImpl] OnDecodeFrame(): [" << stream_id_ << "]: Render frame failed";
    return;
  }
  this->SendFrameInfo(data);
}

void FileHandlerImpl::OnDecodeEos() {
  this->SendFlowEos();
  LOGI(SOURCE) << "[FileHandlerImpl] OnDecodeEos(): called";
}

int FileHandlerImpl::CreatePool(CnedkBufSurfaceCreateParams *params, uint32_t block_count) {
  std::unique_lock<std::mutex> lk(mutex_);
  if (!pool_.CreatePool(params, block_count)) {
    pool_created_ = true;
    return 0;
  }
  LOGE(SOURCE) << "[FileHandlerImpl] CreatePool(): Create pool failed.";
  return -1;
}

void FileHandlerImpl::DestroyPool() {
  std::unique_lock<std::mutex> lk(mutex_);
  pool_.DestroyPool(5000);
}

void FileHandlerImpl::OnBufInfo(int width, int height, CnedkBufSurfaceColorFormat fmt) {
  // TODO(liujian): we create according to the first time (ignore Buffer Info changing).
  std::string platform(platform_info_.name);
  if (IsEdgePlatform(platform)) {
    std::unique_lock<std::mutex> lk(mutex_);
    if (pool_created_) return;
    LOGI(SOURCE) << "[FileHandlerImpl] OnBufInfo() Create pool";
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
      LOGE(SOURCE) << "[FileHandlerImpl] OnBufInfo() Create pool failed";
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

cnedk::BufSurfWrapperPtr FileHandlerImpl::GetBufSurface(int timeout_ms) {
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
        LOGE(SOURCE) << "[FileHandlerImpl] GetBufSurface() Create BufSurface failed.";
      return nullptr;
    }
    return std::make_shared<cnedk::BufSurfaceWrapper>(surf);
  }
  return nullptr;
}

}  // namespace cnstream
