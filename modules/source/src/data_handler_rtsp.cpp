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

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
//
// FFMPEG use AVCodecParameters instead of AVCodecContext
// since from version 3.1(libavformat/version:57.40.100)
//
#define FFMPEG_VERSION_3_1 AV_VERSION_INT(57, 40, 100)

static uint64_t GetTickCount() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

class FFmpegDemuxer : public IDemuxer {
 public:
  FFmpegDemuxer(FrameQueue *queue, const std::string &url)
    :IDemuxer(), queue_(queue), url_name_(url) {
  }

  ~FFmpegDemuxer() { }

  static int InterruptCallBack(void* ctx) {
    FFmpegDemuxer* demux = reinterpret_cast<FFmpegDemuxer*>(ctx);
    if (demux->CheckTimeOut(GetTickCount())) {
      return 1;
    }
    return 0;
  }

  bool CheckTimeOut(uint64_t ul_current_time) {
    if ((ul_current_time - last_receive_frame_time_) / 1000 > max_receive_time_out_) {
      return true;
    }
    return false;
  }

  bool PrepareResources(std::atomic<int> &exit_flag) override {
    // format context
    p_format_ctx_ = avformat_alloc_context();
    if (!p_format_ctx_) {
      return false;
    }

    int ret_code;
    const char* p_rtsp_start_str = "rtsp://";
    if (0 == strncasecmp(url_name_.c_str(), p_rtsp_start_str, strlen(p_rtsp_start_str))) {
      AVIOInterruptCB intrpt_callback = {InterruptCallBack, this};
      p_format_ctx_->interrupt_callback = intrpt_callback;
      last_receive_frame_time_ = GetTickCount();
    }
    // options
    av_dict_set(&options_, "buffer_size", "1024000", 0);
    av_dict_set(&options_, "max_delay", "500000", 0);
    av_dict_set(&options_, "stimeout", "20000000", 0);
    av_dict_set(&options_, "rtsp_transport", "tcp", 0);
    // open input
    ret_code = avformat_open_input(&p_format_ctx_, url_name_.c_str(), NULL, &options_);
    if (0 != ret_code) {
      LOG(ERROR) << "Couldn't open input stream.";
      return false;
    }

    // find video stream information
    ret_code = avformat_find_stream_info(p_format_ctx_, NULL);
    if (ret_code < 0) {
      LOG(ERROR) << "Couldn't find stream information.";
      return false;
    }

    VideoStreamInfo info;
    if (!GetVideoStreamInfo(p_format_ctx_, video_index_, info)) {
      return false;
    }
    this->SetInfo(info);

    av_init_packet(&packet_);
    packet_.data = NULL;
    packet_.size = 0;
    return true;
  }

  void ClearResources(std::atomic<int> &exit_flag) override {
    if (p_format_ctx_) {
      avformat_close_input(&p_format_ctx_);
      avformat_free_context(p_format_ctx_);
      av_dict_free(&options_);
      options_ = nullptr;
      p_format_ctx_ = nullptr;
    }
    first_frame_ = true;
  }

  bool Extract() {
    while (true) {
      last_receive_frame_time_ = GetTickCount();

      if (av_read_frame(p_format_ctx_, &packet_) < 0) {
        return false;
      }

      if (packet_.stream_index != video_index_) {
        av_packet_unref(&packet_);
        continue;
      }

      if (first_frame_) {
        if (packet_.flags & AV_PKT_FLAG_KEY) {
          first_frame_ = false;
        } else {
          av_packet_unref(&packet_);
          continue;
        }
      }

      // find pts information
      if (AV_NOPTS_VALUE == packet_.pts && find_pts_) {
        find_pts_ = false;
        LOG(WARNING) << "Didn't find pts informations, "
                     << "use ordered numbers instead. "
                     << "stream url: " << url_name_.c_str();
      } else if (AV_NOPTS_VALUE != packet_.pts) {
        find_pts_ = true;
        AVStream* vstream = p_format_ctx_->streams[video_index_];
        packet_.pts = av_rescale_q(packet_.pts, vstream->time_base, {1, 90000});
      }
      if (!find_pts_) {
        packet_.pts = pts_++;
      }
      return true;
    }
  }

  bool Process() override {
    bool ret = Extract();
    if (!ret) {
      LOG(INFO) << "Read EOS";
      ESPacket pkt;
      pkt.flags = ESPacket::FLAG_EOS;
      if (queue_) {
        queue_->Push(std::make_shared<EsPacket>(&pkt));
      }
      return false;
    }
    ESPacket pkt;
    pkt.data = packet_.data;
    pkt.size = packet_.size;
    pkt.pts = packet_.pts;
    pkt.flags = 0;
    if (packet_.flags & AV_PKT_FLAG_KEY) {
      pkt.flags |= ESPacket::FLAG_KEY_FRAME;
    }
    if (queue_) {
      queue_->Push(std::make_shared<EsPacket>(&pkt));
    }
    av_packet_unref(&packet_);
    return true;
  }

 private:
  AVFormatContext* p_format_ctx_ = nullptr;
  AVDictionary* options_ = NULL;
  AVPacket packet_;
  bool first_frame_ = true;
  int video_index_ = -1;
  uint64_t last_receive_frame_time_ = 0;
  uint8_t max_receive_time_out_ = 3;
  bool find_pts_ = false;
  uint64_t pts_ = 0;
  FrameQueue *queue_ = nullptr;
  std::string url_name_;
};  // class FFmpegDemuxer

class Live555Demuxer : public IDemuxer, public IRtspCB {
 public:
  Live555Demuxer(FrameQueue *queue, const std::string &url, int reconnect)
    :IDemuxer(), queue_(queue), url_(url), reconnect_(reconnect) {
  }

  virtual ~Live555Demuxer() {
    parser_.Free();
  }

  bool PrepareResources(std::atomic<int> &exit_flag) override {
    // start rtsp_client
    cnstream::OpenParam param;
    param.url = url_;
    param.reconnect = reconnect_;
    param.cb = this;
    rtsp_session_.Open(param);

    // waiting for stream info...
    VideoStreamInfo info;
    while (1) {
      if (parser_.GetInfo(info)) {
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
    this->SetInfo(info);
    parser_.Free();
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
  void OnFrame(unsigned char *data, size_t size, FrameInfo *frame_info)  override {
    if (data && size && frame_info) {
      switch (frame_info->codec_type) {
      case FrameInfo::H264: parser_.Init("h264"); break;
      case FrameInfo::H265: parser_.Init("h265"); break;
      default: {
          LOG(ERROR) << "unsupported codec type";
          return;
        }
      }
      ESPacket pkt;
      pkt.data = data;
      pkt.size = size;
      pkt.flags = 0;
      pkt.pts = frame_info->pts;
      this->Write(&pkt);
      if (!connect_done_) connect_done_.store(true);
    } else {
      if (!connect_done_) {
        // Failed to connect server...
        connect_failed_.store(true);
        return;
      }
      ESPacket pkt;
      pkt.flags = ESPacket::FLAG_EOS;
      this->Write(&pkt);
    }
  }

  void OnEvent(int type) override {}

  int Write(ESPacket *pkt) {
    if (pkt && pkt->data && pkt->size) {
        parser_.Parse(pkt->data, pkt->size);
    }
    if (queue_) {
      queue_->Push(std::make_shared<EsPacket>(pkt));
    }
    return 0;
  }

 private:
  FrameQueue *queue_ = nullptr;
  std::string url_;
  int reconnect_ = 0;
  ParserHelper parser_;
  RtspSession rtsp_session_;
  std::atomic<bool> connect_done_{false};
  std::atomic<bool> connect_failed_{false};
};  // class Live555Demuxer

RtspHandler::RtspHandler(DataSource *module, const std::string &stream_id, const std::string &url_name, bool use_ffmpeg,
                         int reconnect)
    : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) RtspHandlerImpl(module, url_name, *this, use_ffmpeg, reconnect);
}

RtspHandler::~RtspHandler() {
  if (impl_) {
    delete impl_;
  }
}

bool RtspHandler::Open() {
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

void RtspHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

bool RtspHandlerImpl::Open() {
  // updated with paramSet
  DataSource *source = dynamic_cast<DataSource *>(module_);
  param_ = source->GetSourceParam();
  if (param_.output_type_ != OUTPUT_MLU) {
    LOG(ERROR) << "output_type not supported:" << param_.output_type_;
    return false;
  }
  this->interval_ = param_.interval_;
  perf_manager_ = source->GetPerfManager(stream_id_);

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
  LOG(INFO) << "DemuxLoop Start...";
  std::shared_ptr<IDemuxer> demuxer;
  if (use_ffmpeg_) {
    demuxer.reset(new (std::nothrow) FFmpegDemuxer(queue_, url_name_));
  } else {
    demuxer.reset(new (std::nothrow) Live555Demuxer(queue_, url_name_, reconnect_));
  }
  if (!demuxer) {
    LOG(ERROR) << "Failed to create demuxer";
    return;
  }
  if (!demuxer->PrepareResources(demux_exit_flag_)) {
    if (nullptr != module_)
      module_->PostEvent(
          EVENT_ERROR, "stream_id " + stream_id_ + "Prepare codec resources failed.");
    return;
  }
  {
    std::unique_lock<std::mutex> lk(mutex_);
    demuxer->GetInfo(stream_info_);
    stream_info_set_ = true;
  }

  while (!demux_exit_flag_) {
    if (demuxer->Process() != true) {
      break;
    }
  }
  demuxer->ClearResources(demux_exit_flag_);
  LOG(INFO) << "DemuxLoop Exit";
}

void RtspHandlerImpl::DecodeLoop() {
  LOG(INFO) << "DecodeLoop Start...";
  /*meet cnrt requirement*/
  if (param_.device_id_ >=0) {
    try {
      edk::MluContext mlu_ctx;
      mlu_ctx.SetDeviceId(param_.device_id_);
      // mlu_ctx.SetChannelId(dev_ctx_.ddr_channel);
      mlu_ctx.ConfigureForThisThread();
    } catch (edk::Exception &e) {
      if (nullptr != module_)
        module_->PostEvent(EVENT_ERROR, \
            "stream_id " + stream_id_ + " failed to setup dev/channel.");
      return;
    }
  }
  // wait stream_info
  while (!decode_exit_flag_) {
    std::unique_lock<std::mutex> lk(mutex_);
    if (stream_info_set_) {
      break;
    }
    usleep(1000);
  }
  if (decode_exit_flag_) {
    return;
  }

  std::shared_ptr<Decoder> decoder_ = nullptr;
  if (param_.decoder_type_ == DecoderType::DECODER_MLU) {
    decoder_ = std::make_shared<MluDecoder>(this);
  } else if (param_.decoder_type_ == DecoderType::DECODER_CPU) {
    decoder_ = std::make_shared<FFmpegCpuDecoder>(this);
  } else {
    LOG(ERROR) << "unsupported decoder_type";
    return;
  }
  if (decoder_.get()) {
    std::unique_lock<std::mutex> lk(mutex_);
    bool ret = decoder_->Create(&stream_info_, interval_);
    if (!ret) {
      LOG(ERROR) << "Failed to create cndecoder";
      decoder_->Destroy();
      return;
    }
  } else {
    LOG(ERROR) << "Failed to create decoder";
    decoder_->Destroy();
    return;
  }

  // feed extradata first
  if (stream_info_.extra_data.size()) {
    ESPacket pkt;
    pkt.data = stream_info_.extra_data.data();
    pkt.size = stream_info_.extra_data.size();
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
      LOG(INFO) << "Read Timeout";
      continue;
    } else {
      if (perf_manager_ != nullptr) {
        std::string thread_name = "cn-" + module_->GetName() + "-" + NumToFormatStr(handler_.GetStreamUniqueIdx(), 2);
        perf_manager_->Record(false, PerfManager::GetDefaultType(), module_->GetName(), in->pkt_.pts);
        perf_manager_->Record(PerfManager::GetDefaultType(), PerfManager::GetPrimaryKey(), std::to_string(in->pkt_.pts),
            module_->GetName() + PerfManager::GetThreadSuffix(), "'" + thread_name + "'");
      }
    }

    if (in->pkt_.flags & ESPacket::FLAG_EOS) {
      LOG(INFO) << "Read EOS";
      ESPacket pkt;
      pkt.data = in->pkt_.data;
      pkt.size = in->pkt_.size;
      pkt.pts = in->pkt_.pts;
      pkt.flags = ESPacket::FLAG_EOS;
      decoder_->Process(&pkt);
      break;
    }  // if (eos)

    ESPacket pkt;
    pkt.data = in->pkt_.data;
    pkt.size = in->pkt_.size;
    pkt.pts = in->pkt_.pts;
    pkt.flags &= ~ESPacket::FLAG_EOS;
    if (!decoder_->Process(&pkt)) {
      break;
    }
  }

  if (decoder_.get()) {
    decoder_->Destroy();
  }
  LOG(INFO) << "DecodeLoop Exit";
}

}  // namespace cnstream
