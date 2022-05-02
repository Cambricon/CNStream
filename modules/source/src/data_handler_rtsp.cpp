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

#include "data_handler_rtsp.hpp"

#include "profiler/module_profiler.hpp"
#include "profiler/pipeline_profiler.hpp"

namespace cnstream {

std::shared_ptr<SourceHandler> RtspHandler::Create(DataSource *module, const std::string &stream_id,
                                                   const std::string &url_name, bool use_ffmpeg, int reconnect,
                                                   const MaximumVideoResolution &maximum_resolution,
                                                   std::function<void(ESPacket, std::string)> callback) {
  if (!module || stream_id.empty() || url_name.empty()) {
    LOGE(SOURCE) << "[RtspHandler] Create function, invalid paramters.";
    return nullptr;
  }
  std::shared_ptr<RtspHandler> handler(
      new (std::nothrow) RtspHandler(module, stream_id, url_name, use_ffmpeg, reconnect, maximum_resolution, callback));
  return handler;
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
  FFmpegDemuxer(const std::string &stream_id, FrameQueue *queue, const std::string &url, bool only_I,
                std::function<void(ESPacket, std::string)> cb = nullptr)
      : rtsp_detail::IDemuxer(), queue_(queue), url_name_(url), parser_(stream_id), only_key_frame_(only_I) {
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
  Live555Demuxer(const std::string &stream_id, FrameQueue *queue, const std::string &url, int reconnect, bool only_I,
                 std::function<void(ESPacket, std::string)> cb = nullptr)
      : rtsp_detail::IDemuxer(),
        stream_id_(stream_id),
        queue_(queue),
        url_(url),
        reconnect_(reconnect),
        only_key_frame_(only_I) {
    save_packet_cb_ = cb;
  }

  virtual ~Live555Demuxer() {}

  bool PrepareResources(std::atomic<int> &exit_flag) override {
    LOGD(SOURCE) << "[" << stream_id_ << "]: "
                 << "Begin prepare resources";
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

    LOGD(SOURCE) << "[" << stream_id_ << "]: "
                 << "Finish prepare resources";
    return true;
  }

  void ClearResources(std::atomic<int> &exit_flag) override {
    LOGD(SOURCE) << "[" << stream_id_ << "]: "
                 << "Begin clear resources";
    rtsp_session_.Close();
    LOGD(SOURCE) << "[" << stream_id_ << "]: "
                 << "Finish clear resources";
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
        LOGI(SOURCE) << "[" << stream_id_ << "]: "
                     << "Rtsp connect success";
      }
    } else {
      pkt.flags = static_cast<size_t>(ESPacket::FLAG::FLAG_EOS);
      if (!connect_done_) {
        // Failed to connect server...
        LOGW(SOURCE) << "[" << stream_id_ << "]: "
                     << "Rtsp connect failed";
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

RtspHandler::RtspHandler(DataSource *module, const std::string &stream_id, const std::string &url_name, bool use_ffmpeg,
                         int reconnect, const MaximumVideoResolution &maximum_resolution,
                         std::function<void(ESPacket, std::string)> callback)
    : SourceHandler(module, stream_id) {
  impl_ =
      new (std::nothrow) RtspHandlerImpl(module, url_name, this, use_ffmpeg, reconnect, maximum_resolution, callback);
}

RtspHandler::~RtspHandler() {
  if (impl_) {
    delete impl_;
  }
}

bool RtspHandler::Open() {
  if (!this->module_) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "module_ null";
    return false;
  }
  if (!impl_) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "RtspHandler open failed, no memory left";
    return false;
  }

  if (stream_index_ == cnstream::INVALID_STREAM_IDX) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "invalid stream_idx";
    return false;
  }

  return impl_->Open();
}

void RtspHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

bool RtspHandlerImpl::Open() {
  DataSource *source = dynamic_cast<DataSource *>(module_);
  param_ = source->GetSourceParam();

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

void RtspHandlerImpl::Close() {
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

  std::lock_guard<std::mutex> lk(mutex_);  // Close called by multi threads
  if (queue_) {
    delete queue_;
    queue_ = nullptr;
  }
}

void RtspHandlerImpl::DemuxLoop() {
  LOGD(SOURCE) << "[" << stream_id_ << "]: "
               << "Create demuxer...";
  std::unique_ptr<rtsp_detail::IDemuxer> demuxer;
  if (use_ffmpeg_) {
    demuxer.reset(new FFmpegDemuxer(stream_id_, queue_, url_name_, param_.only_key_frame_, save_es_packet_));
  } else {
    auto cb = handler_->GetStreamId();
    demuxer.reset(
        new Live555Demuxer(stream_id_, queue_, url_name_, reconnect_, param_.only_key_frame_, save_es_packet_));
  }
  if (!demuxer) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "Failed to create demuxer";
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
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "PrepareResources failed";
    return;
  }

  LOGI(SOURCE) << "[" << stream_id_ << "]: "
               << "Wait stream info...";

  do {
    {
      std::lock_guard<std::mutex> lk(mutex_);
      if (demuxer->GetInfo(stream_info_) == true) {
        break;
      }
    }
    usleep(1000);
  }while(1);
  stream_info_.maximum_resolution = maximum_resolution_;
  stream_info_set_.store(true);

  LOGI(SOURCE) << "[" << stream_id_ << "]: "
               << "Got stream info";

  while (!demux_exit_flag_) {
    if (demuxer->Process() != true) {
      break;
    }
  }

  LOGD(SOURCE) << "[" << stream_id_ << "]: "
               << "RTSP handler DemuxLoop Exit";
  demuxer->ClearResources(demux_exit_flag_);
}

void RtspHandlerImpl::DecodeLoop() {
  /*meet cnrt requirement,
   *  for cpu case(device_id < 0), MluDeviceGuard will do nothing
   */
  MluDeviceGuard guard(param_.device_id_);

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
  if (param_.decoder_type_ == DecoderType::DECODER_MLU) {
    decoder_.reset(new MluDecoder(stream_id_, this));
  } else if (param_.decoder_type_ == DecoderType::DECODER_CPU) {
    decoder_.reset(new FFmpegCpuDecoder(stream_id_, this));
  } else {
    LOGE(SOURCE) << "unsupported decoder_type";
    return;
  }
  if (decoder_) {
    ExtraDecoderInfo extra;
    extra.device_id = param_.device_id_;
    extra.input_buf_num = param_.input_buf_number_;
    extra.output_buf_num = param_.output_buf_number_;
    extra.apply_stride_align_for_scaler = param_.apply_stride_align_for_scaler_;
    extra.extra_info = stream_info_.extra_data;
    std::lock_guard<std::mutex> lk(mutex_);
    bool ret = decoder_->Create(&stream_info_, &extra);
    if (!ret) {
      LOGE(SOURCE) << "[" << stream_id_ << "]: "
                   << "Failed to create decoder";
      decoder_->Destroy();
      return;
    }
  } else {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "Failed to create decoder";
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

  using EsPacketPtr = std::shared_ptr<EsPacket>;
  while (!decode_exit_flag_) {
    EsPacketPtr in;
    int timeoutMs = 1000;
    bool ret = this->queue_->Pop(timeoutMs, in);
    if (!ret) {
      LOGD(SOURCE) << "[" << stream_id_ << "]: "
                   << "Read packet Timeout";
      continue;
    }

    if (in->pkt_.flags & static_cast<size_t>(ESPacket::FLAG::FLAG_EOS)) {
      LOGI(SOURCE) << "[" << stream_id_ << "]: "
                   << "EOS reached in RtspHandler";
      decoder_->Process(nullptr);
      break;
    }  // if (eos)

    VideoEsPacket pkt;
    pkt.data = in->pkt_.data;
    pkt.len = in->pkt_.size;
    pkt.pts = in->pkt_.pts;

    if (module_ && module_->GetProfiler()) {
      auto record_key = std::make_pair(stream_id_, pkt.pts);
      module_->GetProfiler()->RecordProcessStart(kPROCESS_PROFILER_NAME, record_key);
      if (module_->GetContainer() && module_->GetContainer()->GetProfiler()) {
        module_->GetContainer()->GetProfiler()->RecordInput(record_key);
      }
    }

    if (!decoder_->Process(&pkt)) {
      break;
    }
    std::this_thread::yield();
  }

  LOGD(SOURCE) << "RTSP handler DecodeLoop Exit";
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

void RtspHandlerImpl::OnDecodeFrame(DecodeFrame *frame) {
  if (frame_count_++ % param_.interval_ != 0) {
    return;  // discard frames
  }
  if (!frame) {
    LOGW(SOURCE) << "[RtspHandlerImpl] OnDecodeFrame, frame is nullptr.";
    return;
  }

  std::shared_ptr<CNFrameInfo> data = this->CreateFrameInfo();
  if (!data) {
    LOGW(SOURCE) << "[RtspHandlerImpl] OnDecodeFrame, failed to create FrameInfo.";
    return;
  }

  data->timestamp = frame->pts;  // FIXME
  if (!frame->valid) {
    data->flags = static_cast<size_t>(CNFrameFlag::CN_FRAME_FLAG_INVALID);
    this->SendFrameInfo(data);
    return;
  }

  int ret = SourceRender::Process(data, frame, frame_id_++, param_);
  if (ret < 0) {
    return;
  }
  this->SendFrameInfo(data);
}

void RtspHandlerImpl::OnDecodeEos() {
  this->SendFlowEos();
}

}  // namespace cnstream
