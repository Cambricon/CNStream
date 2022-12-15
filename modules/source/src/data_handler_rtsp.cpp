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

#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "cnrt.h"

#include "cnedk_platform.h"
#include "cnedk_buf_surface_util.hpp"
#include "cnstream_logging.hpp"
#include "data_handler_rtsp.hpp"
#include "data_handler_util.hpp"
#include "platform_utils.hpp"
#include "profiler/module_profiler.hpp"
#include "profiler/pipeline_profiler.hpp"
#include "rtsp_client.hpp"
#include "util/cnstream_queue.hpp"
#include "video_decoder.hpp"

namespace cnstream {

class RtspHandlerImpl : public IDecodeResult, public SourceRender, public IUserPool {
 public:
  explicit RtspHandlerImpl(DataSource *module, const RtspSourceParam &param, RtspHandler *handler)
      : SourceRender(handler),
        module_(module),
        handle_param_(param),
        handler_(*handler),
        stream_id_(handler_.GetStreamId()) {}
  ~RtspHandlerImpl() { Close(); }
  bool Open();
  void Stop();
  void Close();

 private:
  DataSource *module_ = nullptr;
  RtspSourceParam handle_param_;
  RtspHandler &handler_;
  std::string stream_id_;
  DataSourceParam param_;
  CnedkPlatformInfo platform_info_;
  CnedkBufSurfaceCreateParams create_params_;

 private:
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
  void DemuxLoop();
  void DecodeLoop();

  std::shared_ptr<Decoder> decoder_ = nullptr;
  cnedk::BufPool pool_;
  bool pool_created_ = false;
  std::mutex mutex_;
  std::atomic<int> demux_exit_flag_ {0};
  std::thread demux_thread_;
  std::atomic<int> decode_exit_flag_{0};
  std::thread decode_thread_;
  std::atomic<bool> stream_info_set_{false};
  std::mutex stream_info_mutex_;
  VideoInfo stream_info_{};
  BoundedQueue<std::shared_ptr<EsPacket>> *queue_ = nullptr;
  std::mutex stop_mutex_;

  uint32_t interval_ = 1;
  ModuleProfiler *module_profiler_ = nullptr;
  PipelineProfiler *pipeline_profiler_ = nullptr;
};  // class RtspHandlerImpl


std::shared_ptr<SourceHandler> CreateSource(DataSource *module, const std::string &stream_id,
                                            const RtspSourceParam &param) {
  if (!module || stream_id.empty() || param.url_name.empty()) {
    LOGE(SOURCE) << "CreateSource(): Create RtspHandler failed."
                 << " source module, stream id and url_name must not be empty.";
    return nullptr;
  }
  return std::make_shared<RtspHandler>(module, stream_id, param);
}

RtspHandler::RtspHandler(DataSource *module, const std::string &stream_id, const RtspSourceParam &param)
    : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) RtspHandlerImpl(module, param, this);
}

RtspHandler::~RtspHandler() {
  if (impl_) delete impl_, impl_ = nullptr;
}

namespace rtsp_detail {
class IDemuxer {
 public:
  virtual ~IDemuxer() {}
  virtual bool PrepareResources(std::atomic<int> &exit_flag) = 0;  // NOLINT
  virtual void ClearResources(std::atomic<int> &exit_flag) = 0;   // NOLINT
  virtual bool Process() = 0;  // process one frame
  bool GetInfo(VideoInfo &info) {  // NOLINT
    std::unique_lock<std::mutex> lk(mutex_);
    if (info_set_) {
      info = info_;
      return true;
    }
    return false;
  }
 protected:
  void SetInfo(VideoInfo &info) {  // NOLINT
    std::unique_lock<std::mutex> lk(mutex_);
    info_ = info;
    info_set_ = true;
  }
  std::function<void(cnstream::ESPacket, std::string)> save_packet_cb_ = nullptr;
  std::mutex mutex_;
  VideoInfo info_;
  bool info_set_ = false;
};
}  // namespace rtsp_detail

class FFmpegDemuxer : public rtsp_detail::IDemuxer, public IParserResult {
 public:
  FFmpegDemuxer(const std::string &stream_id, FrameQueue *queue, const std::string &url,
                bool only_key_frame, std::function<void(ESPacket, std::string)> cb = nullptr)
      : rtsp_detail::IDemuxer(),
        queue_(queue),
        url_name_(url),
        parser_(stream_id),
        only_key_frame_(only_key_frame) {
    save_packet_cb_ = cb;
  }

  ~FFmpegDemuxer() { }

  bool PrepareResources(std::atomic<int> &exit_flag) override {
    if (parser_.Open(url_name_, this, only_key_frame_) == 0) {
      eos_reached_ = false;
      return true;
    }
    return false;
  }

  void ClearResources(std::atomic<int> &exit_flag) override {
    parser_.Close();
  }

  bool Process() override {
    parser_.Parse();
    if (eos_reached_) {
      return false;
    }
    return true;
  }

  // IParserResult methods
  void OnParserInfo(VideoInfo *info) override {
    this->SetInfo(*info);
  }
  void OnParserFrame(VideoEsFrame *frame) override {
    ESPacket pkt;
    if (frame) {
      pkt.data = frame->data;
      pkt.size = frame->len;
      pkt.pts = frame->pts;
      pkt.flags = 0;
      if (frame->flags & AV_PKT_FLAG_KEY) {
        pkt.flags |= static_cast<size_t>(ESPacket::FLAG::FLAG_KEY_FRAME);
      }
    } else {
      pkt.flags = static_cast<size_t>(ESPacket::FLAG::FLAG_EOS);
      eos_reached_ = true;
    }
    if (queue_) {
      queue_->Push(std::make_shared<EsPacket>(&pkt));
    }
    // sometimes users want to save the es packet data by themselves.
    if (save_packet_cb_) {
      save_packet_cb_(pkt, parser_.GetStreamID());
    }
  }

 private:
  FrameQueue *queue_ = nullptr;
  std::string url_name_;
  FFParser parser_;
  bool eos_reached_ = false;
  bool only_key_frame_ = false;
};  // class FFmpegDemuxer

class Live555Demuxer : public rtsp_detail::IDemuxer, public IRtspCB {
 public:
  Live555Demuxer(const std::string &stream_id, FrameQueue *queue, const std::string &url, int reconnect,
                 bool only_key_frame, std::function<void(ESPacket, std::string)> cb = nullptr)
      : rtsp_detail::IDemuxer(),
        stream_id_(stream_id),
        queue_(queue),
        url_(url),
        reconnect_(reconnect),
        only_key_frame_(only_key_frame) {
    save_packet_cb_ = cb;
  }

  virtual ~Live555Demuxer() {}

  bool PrepareResources(std::atomic<int> &exit_flag) override {
    VLOG1(SOURCE) << "[Live555Demuxer] PrepareResources(): [" << stream_id_ << "]: Begin";
    // start rtsp_client
    cnstream::OpenParam param;
    param.url = url_;
    param.reconnect = reconnect_;
    param.only_key_frame = only_key_frame_;
    param.cb = dynamic_cast<IRtspCB*>(this);
    rtsp_session_.Open(param);

    while (1) {
      if (rtsp_info_set_) {
        break;
      }
      if (exit_flag) {
        break;
      }
      if (connect_failed_) {
        return false;
      }
    }
    if (exit_flag) {
      return false;
    }

    VLOG1(SOURCE) << "[Live555Demuxer] PrepareResources(): [" << stream_id_ << "]: Finish";
    return true;
  }

  void ClearResources(std::atomic<int> &exit_flag) override {
    VLOG1(SOURCE) << "[Live555Demuxer] ClearResources(): [" << stream_id_ << "]: Begin";
    rtsp_session_.Close();
    VLOG1(SOURCE) << "[Live555Demuxer] ClearResources(): [" << stream_id_ << "]: Finish";
  }

  bool Process() override {
    usleep(100 * 1000);
    return true;
  }

 private:
  // IRtspCB methods
  void OnRtspInfo(VideoInfo *info) override {
    this->SetInfo(*info);
    rtsp_info_set_.store(true);
  }
  void OnRtspFrame(VideoEsFrame *frame) override {
    ESPacket pkt;
    if (frame) {
      pkt.data = frame->data;
      pkt.size = frame->len;
      pkt.pts = frame->pts;
      pkt.flags = 0;
      if (frame->flags & AV_PKT_FLAG_KEY) {
        pkt.flags |= static_cast<size_t>(ESPacket::FLAG::FLAG_KEY_FRAME);
      }
      if (!connect_done_) {
        connect_done_.store(true);
        LOGI(SOURCE) << "[Live555Demuxer] OnRtspFrame(): [" << stream_id_ << "]: Rtsp connect success";
      }
    } else {
      pkt.flags = static_cast<size_t>(ESPacket::FLAG::FLAG_EOS);
      if (!connect_done_) {
        // Failed to connect server...
        LOGI(SOURCE) << "[Live555Demuxer] OnRtspFrame(): [" << stream_id_ << "]: Rtsp connect failed";
        connect_failed_.store(true);
      }
    }
    if (queue_) {
      queue_->Push(std::make_shared<EsPacket>(&pkt));
    }

    // sometimes users want to save the es packet data by themselves.
    if (save_packet_cb_) {
      save_packet_cb_(pkt, stream_id_);
    }
  }

  void OnRtspEvent(int type) override {}

 private:
  std::string stream_id_;
  FrameQueue *queue_ = nullptr;
  std::string url_;
  int reconnect_ = 0;
  bool only_key_frame_ = false;
  RtspSession rtsp_session_;
  std::atomic<bool> connect_done_{false};
  std::atomic<bool> connect_failed_{false};
  std::atomic<bool> rtsp_info_set_{false};
};  // class Live555Demuxer


bool RtspHandler::Open() {
  if (!this->module_) {
    LOGE(SOURCE) << "[RtspHandler] Open(): [" << stream_id_ << "]: module_ null";
    return false;
  }
  if (!impl_) {
    LOGE(SOURCE) << "[RtspHandler] Open(): [" << stream_id_ << "]: no memory left";
    return false;
  }

  if (stream_index_ == cnstream::kInvalidStreamIdx) {
    LOGE(SOURCE) << "[RtspHandler] Open(): [" << stream_id_ << "]: Invalid stream_idx";
    return false;
  }

  return impl_->Open();
}

void RtspHandler::Stop() {
  if (impl_) {
    impl_->Stop();
  }
}

void RtspHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

bool RtspHandlerImpl::Open() {
  DataSource *source = dynamic_cast<DataSource *>(module_);
  param_ = source->GetSourceParam();

  if (CnedkPlatformGetInfo(param_.device_id, &platform_info_) < 0) {
    LOGE(SOURCE) << "[RtspHandlerImpl] Open(): Get platform information failed";
    return false;
  }
  std::string platform(platform_info_.name);

  if (handle_param_.out_res.width > 0 && handle_param_.out_res.height > 0) {
    LOGI(SOURCE) << "[RtspHandlerImpl] Open(): Create pool";
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
      LOGE(SOURCE) << "[RtspHandlerImpl] Open(): Create pool failed";
      return false;
    }
  }

  if (!module_profiler_) {
    if (module_) module_profiler_ = module_->GetProfiler();
    if (!pipeline_profiler_) {
      if (module_->GetContainer()) pipeline_profiler_ = module_->GetContainer()->GetProfiler();
    }
  }

  interval_ = handle_param_.interval ? handle_param_.interval : param_.interval;

  size_t maxSize = 60;  // FIXME
  queue_ = new FrameQueue(maxSize);
  if (!queue_) {
    return false;
  }

  decode_exit_flag_ = 0;
  decode_thread_ = std::thread(&RtspHandlerImpl::DecodeLoop, this);
  demux_exit_flag_ = 0;
  demux_thread_ = std::thread(&RtspHandlerImpl::DemuxLoop, this);
  return true;
}

void RtspHandlerImpl::Stop() {
  std::lock_guard<std::mutex> lk(stop_mutex_);  // Close called by multi threads
  if (!demux_exit_flag_) {
    demux_exit_flag_ = 1;
    if (demux_thread_.joinable()) {
      demux_thread_.join();
    }
  }
  if (!decode_exit_flag_) {
    decode_exit_flag_ = 1;
    if (decode_thread_.joinable()) {
      decode_thread_.join();
    }
  }

  if (queue_) {
    delete queue_;
    queue_ = nullptr;
  }
}

void RtspHandlerImpl::Close() {
  Stop();
  LOGI(SOURCE) << "[RtspHandlerImpl] Close(): this(" << this << ") Destroy pool";
  DestroyPool();
}

void RtspHandlerImpl::DemuxLoop() {
  VLOG1(SOURCE) << "[RtspHandlerImpl] DemuxLoop(): [" << stream_id_ << "]: Create demuxer...";
  std::unique_ptr<rtsp_detail::IDemuxer> demuxer;
  if (handle_param_.use_ffmpeg) {
    demuxer.reset(new FFmpegDemuxer(stream_id_, queue_, handle_param_.url_name,
                                    handle_param_.only_key_frame, handle_param_.callback));
  } else {
    demuxer.reset(new Live555Demuxer(stream_id_, queue_, handle_param_.url_name, handle_param_.reconnect,
                                     handle_param_.only_key_frame, handle_param_.callback));
  }
  if (!demuxer) {
    LOGE(SOURCE) << "[RtspHandlerImpl] DemuxLoop(): [" << stream_id_ << "]: Failed to create demuxer";
    return;
  }
  if (!demuxer->PrepareResources(demux_exit_flag_)) {
    if (nullptr != module_) {
      Event e;
      e.type = EventType::EVENT_STREAM_ERROR;
      e.module_name = module_->GetName();
      e.message = "Prepare codec resources failed.";
      e.stream_id = stream_id_;
      e.thread_id = std::this_thread::get_id();
      module_->PostEvent(e);
    }
    LOGE(SOURCE) << "[RtspHandlerImpl] DemuxLoop(): [" << stream_id_ << "]: PrepareResources failed";
    return;
  }

  LOGI(SOURCE) << "[RtspHandlerImpl] DemuxLoop(): [" << stream_id_ << "]: Wait stream info...";

  do {
    {
      std::lock_guard<std::mutex> lk(stream_info_mutex_);
      if (demuxer->GetInfo(stream_info_) == true) {
        break;
      }
      if (demux_exit_flag_) { return; }
    }
    usleep(1000);
  } while (1);
  stream_info_set_.store(true);

  LOGI(SOURCE) << "[RtspHandlerImpl] DemuxLoop(): [" << stream_id_ << "]: Got stream info";

  while (!demux_exit_flag_) {
    if (demuxer->Process() != true) {
      break;
    }
  }

  VLOG1(SOURCE) << "[RtspHandlerImpl] DemuxLoop(): [" << stream_id_ << "]: DemuxLoop Exit";
  demuxer->ClearResources(demux_exit_flag_);
}

void RtspHandlerImpl::DecodeLoop() {
  cnrtSetDevice(param_.device_id);

  // wait stream_info
  while (!decode_exit_flag_) {
    if (stream_info_set_) {
      break;
    }
    usleep(1000);
  }
  if (decode_exit_flag_) {
    return;
  }

  std::unique_ptr<Decoder> decoder_ = nullptr;
  decoder_.reset(new MluDecoder(stream_id_, this, this));
  if (!decoder_) {
    LOGE(SOURCE) << "[RtspHandlerImpl] DecodeLoop(): New decoder failed.";
    return;
  }

  decoder_->SetPlatformName(platform_info_.name);
  ExtraDecoderInfo extra;
  extra.device_id = param_.device_id;
  extra.max_width = handle_param_.max_res.width;
  extra.max_height = handle_param_.max_res.height;
  std::unique_lock<std::mutex> lk(stream_info_mutex_);
  bool ret = decoder_->Create(&stream_info_, &extra);
  if (!ret) {
    LOGE(SOURCE) << "[RtspHandlerImpl] DecodeLoop(): Create decoder failed.";
    decoder_->Destroy();
    return;
  }


  // feed extradata first
  if (stream_info_.extra_data.size()) {
    VideoEsPacket pkt;
    pkt.data = stream_info_.extra_data.data();
    pkt.len = stream_info_.extra_data.size();
    pkt.pts = 0;
    if (!decoder_->Process(&pkt)) {
      return;
    }
  }
  lk.unlock();

  using EsPacketPtr = std::shared_ptr<EsPacket>;
  while (!decode_exit_flag_) {
    EsPacketPtr in;
    int timeoutMs = 1000;
    bool ret = this->queue_->Pop(timeoutMs, in);
    if (!ret) {
      VLOG1(SOURCE) << "[RtspHandlerImpl] DecodeLoop(): [" << stream_id_ << "]: Read packet Timeout";
      continue;
    }

    if (in->pkt_.flags & static_cast<size_t>(ESPacket::FLAG::FLAG_EOS)) {
      LOGI(SOURCE) << "[RtspHandlerImpl] DecodeLoop(): [" << stream_id_ << "]: EOS reached";
      decoder_->Process(nullptr);
      break;
    }  // if (eos)

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
      break;
    }
    std::this_thread::yield();
  }

  VLOG1(SOURCE) << "[RtspHandlerImpl] DecodeLoop(): [" << stream_id_ << "]: Exit";
  if (decoder_.get()) {
    decoder_->Destroy();
  }
}

// IDecodeResult methods
void RtspHandlerImpl::OnDecodeError(DecodeErrorCode error_code) {
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

void RtspHandlerImpl::OnDecodeFrame(cnedk::BufSurfWrapperPtr wrapper) {
  if (frame_count_++ % interval_ != 0) {
    return;  // discard frames
  }

  std::shared_ptr<CNFrameInfo> data = this->CreateFrameInfo();
  if (!data) {
    LOGW(SOURCE) << "[RtspHandlerImpl] OnDecodeFrame, failed to create FrameInfo.";
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
    LOGE(SOURCE) << "[RtspHandlerImpl] OnDecodeFrame(): [" << stream_id_ << "]: Render frame failed";
    return;
  }
  this->SendFrameInfo(data);
}

void RtspHandlerImpl::OnDecodeEos() {
  this->SendFlowEos();
  LOGI(SOURCE) << "[RtspHandlerImpl] OnDecodeEos(): called";
}


int RtspHandlerImpl::CreatePool(CnedkBufSurfaceCreateParams *params, uint32_t block_count) {
  std::unique_lock<std::mutex> lk(mutex_);
  if (!pool_.CreatePool(params, block_count)) {
    pool_created_ = true;
    return 0;
  }
  LOGE(SOURCE) << "[RtspHandlerImpl] CreatePool(): Create pool failed.";
  return -1;
}

void RtspHandlerImpl::DestroyPool() {
  std::unique_lock<std::mutex> lk(mutex_);
  pool_.DestroyPool(5000);
}

void RtspHandlerImpl::OnBufInfo(int width, int height, CnedkBufSurfaceColorFormat fmt) {
  // TODO(liujian): we create according to the first time (ignore Buffer Info changing).
  std::string platform(platform_info_.name);
  if (IsEdgePlatform(platform)) {
    std::unique_lock<std::mutex> lk(mutex_);
    if (pool_created_) return;
    LOGI(SOURCE) << "[RtspHandlerImpl] OnBufInfo() Create pool";
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
      LOGE(SOURCE) << "[RtspHandlerImpl] OnBufInfo() Create pool failed";
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

cnedk::BufSurfWrapperPtr RtspHandlerImpl::GetBufSurface(int timeout_ms) {
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
        LOGE(SOURCE) << "[RtspHandlerImpl] GetBufSurface() Create BufSurface failed.";
      return nullptr;
    }
    return std::make_shared<cnedk::BufSurfaceWrapper>(surf);
  }
  return nullptr;
}

}  // namespace cnstream
