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
                                                   const std::string &url_name, bool use_ffmpeg, int reconnect) {
  if (!module || stream_id.empty() || url_name.empty()) {
    return nullptr;
  }
  std::shared_ptr<RtspHandler> handler(new (std::nothrow)
                                           RtspHandler(module, stream_id, url_name, use_ffmpeg, reconnect));
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

  std::mutex mutex_;
  VideoInfo info_;
  bool info_set_ = false;
};
}  // namespace rtsp_detail

class FFmpegDemuxer : public rtsp_detail::IDemuxer, public IParserResult {
 public:
  FFmpegDemuxer(FrameQueue *queue, const std::string &url)
    :rtsp_detail::IDemuxer(), queue_(queue), url_name_(url) {
  }

  ~FFmpegDemuxer() { }

  bool PrepareResources(std::atomic<int> &exit_flag) override {
    if (parser_.Open(url_name_, this) == 0) {
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
        pkt.flags |= ESPacket::FLAG_KEY_FRAME;
      }
    } else {
      pkt.flags = ESPacket::FLAG_EOS;
      eos_reached_ = true;
    }
    if (queue_) {
      queue_->Push(std::make_shared<EsPacket>(&pkt));
    }
  }

 private:
  FrameQueue *queue_ = nullptr;
  std::string url_name_;
  FFParser parser_;
  bool eos_reached_ = false;
};  // class FFmpegDemuxer

class Live555Demuxer : public rtsp_detail::IDemuxer, public IRtspCB {
 public:
  Live555Demuxer(FrameQueue *queue, const std::string &url, int reconnect)
    :rtsp_detail::IDemuxer(), queue_(queue), url_(url), reconnect_(reconnect) {
  }

  virtual ~Live555Demuxer() {}

  bool PrepareResources(std::atomic<int> &exit_flag) override {
    // start rtsp_client
    cnstream::OpenParam param;
    param.url = url_;
    param.reconnect = reconnect_;
    param.cb = dynamic_cast<IRtspCB*>(this);
    rtsp_session_.Open(param);

    // waiting for stream info...
    while (1) {
      if (info_set_) {
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
    return true;
  }

  void ClearResources(std::atomic<int> &exit_flag) override {
    rtsp_session_.Close();
  }

  bool Process() override {
    usleep(100 * 1000);
    return true;
  }

 private:
  // IRtspCB methods
  void OnRtspInfo(VideoInfo *info) override {
    this->SetInfo(*info);
    info_set_.store(true);
  }
  void OnRtspFrame(VideoEsFrame *frame) override {
    ESPacket pkt;
    if (frame) {
      pkt.data = frame->data;
      pkt.size = frame->len;
      pkt.pts = frame->pts;
      pkt.flags = 0;
      if (frame->flags & AV_PKT_FLAG_KEY) {
        pkt.flags |= ESPacket::FLAG_KEY_FRAME;
      }
      if (!connect_done_) connect_done_.store(true);
    } else {
      pkt.flags = ESPacket::FLAG_EOS;
      if (!connect_done_) {
        // Failed to connect server...
        connect_failed_.store(true);
      }
    }
    if (queue_) {
      queue_->Push(std::make_shared<EsPacket>(&pkt));
    }
  }

  void OnRtspEvent(int type) override {}

 private:
  FrameQueue *queue_ = nullptr;
  std::string url_;
  int reconnect_ = 0;
  RtspSession rtsp_session_;
  std::atomic<bool> connect_done_{false};
  std::atomic<bool> connect_failed_{false};
  std::atomic<bool> info_set_{false};
};  // class Live555Demuxer

RtspHandler::RtspHandler(DataSource *module, const std::string &stream_id, const std::string &url_name, bool use_ffmpeg,
                         int reconnect)
    : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) RtspHandlerImpl(module, url_name, this, use_ffmpeg, reconnect);
}

RtspHandler::~RtspHandler() {
  if (impl_) {
    delete impl_;
  }
}

bool RtspHandler::Open() {
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

void RtspHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

bool RtspHandlerImpl::Open() {
  DataSource *source = dynamic_cast<DataSource *>(module_);
  param_ = source->GetSourceParam();

  SetThreadName(module_->GetName(), handler_->GetStreamUniqueIdx());

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
  if (queue_) {
    delete queue_;
  }
}

void RtspHandlerImpl::DemuxLoop() {
  LOGD(SOURCE) << "DemuxLoop Start...";
  std::unique_ptr<rtsp_detail::IDemuxer> demuxer;
  if (use_ffmpeg_) {
    demuxer.reset(new FFmpegDemuxer(queue_, url_name_));
  } else {
    demuxer.reset(new Live555Demuxer(queue_, url_name_, reconnect_));
  }
  if (!demuxer) {
    LOGE(SOURCE) << "Failed to create demuxer";
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
    LOGI(SOURCE) << "PrepareResources failed.";
    return;
  }
  do {
    std::unique_lock<std::mutex> lk(mutex_);
    if (demuxer->GetInfo(stream_info_) == true) {
      break;
    }
    usleep(1000);
  }while(1);
  stream_info_set_.store(true);

  LOGD(SOURCE) << "RTSP handler DemuxLoop.";

  while (!demux_exit_flag_) {
    if (demuxer->Process() != true) {
      break;
    }
  }
  demuxer->ClearResources(demux_exit_flag_);
  LOGD(SOURCE) << "RTSP handler DemuxLoop Exit";
}

void RtspHandlerImpl::DecodeLoop() {
  LOGD(SOURCE) << "RTSP handler DecodeLoop Start...";
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
    decoder_.reset(new MluDecoder(this));
  } else if (param_.decoder_type_ == DecoderType::DECODER_CPU) {
    decoder_.reset(new FFmpegCpuDecoder(this));
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
    std::unique_lock<std::mutex> lk(mutex_);
    bool ret = decoder_->Create(&stream_info_, &extra);
    if (!ret) {
      LOGE(SOURCE) << "Failed to create cndecoder";
      decoder_->Destroy();
      return;
    }
  } else {
    LOGE(SOURCE) << "Failed to create decoder";
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
      LOGD(SOURCE) << "Read Timeout";
      continue;
    }

    if (in->pkt_.flags & ESPacket::FLAG_EOS) {
      LOGI(SOURCE) << "RTSP handler stream_id: " << stream_id_ << " EOS reached";
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

  if (decoder_.get()) {
    decoder_->Destroy();
  }
  LOGD(SOURCE) << "RTSP handler DecodeLoop Exit";
}

// IDecodeResult methods
void RtspHandlerImpl::OnDecodeError(DecodeErrorCode error_code) {
  // FIXME,  handle decode error ...
  interrupt_.store(true);
}

void RtspHandlerImpl::OnDecodeFrame(DecodeFrame *frame) {
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

void RtspHandlerImpl::OnDecodeEos() {
  this->SendFlowEos();
}

}  // namespace cnstream
