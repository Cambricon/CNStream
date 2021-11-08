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

#include "rtsp_sink.hpp"

#include <memory>
#include <string>
#include <vector>

#include "rtsp_server/rtsp_server.hpp"
#include "video/video_stream/video_stream.hpp"

namespace cnstream {

typedef struct RtspSinkParam {
  int port = 8554;
  bool rtsp_over_http = false;
  int device_id = 0;
  bool mlu_encoder = true;
  bool mlu_input_frame = false;  // The input frame. true: source data , false: ImageBGR()
  int width = 0;
  int height = 0;
  double frame_rate = 0;
  int bit_rate = 4000000;
  int gop_size = 10;
  int tile_cols = 0;
  int tile_rows = 0;
  bool resample = false;
} RtspSinkParam;

struct RtspSinkContext {
  std::unique_ptr<VideoStream> stream = nullptr;
  std::unique_ptr<RtspServer> server = nullptr;
};

RtspSinkContext *RtspSink::GetContext(CNFrameInfoPtr data) {
  auto params = param_helper_->GetParams();
  std::lock_guard<std::mutex> lk(ctx_lock_);
  RtspSinkContext *ctx = nullptr;
  if (params.tile_cols > 1 || params.tile_rows > 1) {
    if (!contexts_.empty()) {
      ctx = contexts_.begin()->second;
    } else {
      ctx = CreateContext(data, "0");
    }
    if (!ctx) return nullptr;
    if (!tile_streams_.count(data->stream_id)) {
      if (tile_streams_.size() < static_cast<size_t>(params.tile_cols * params.tile_rows)) {
        tile_streams_.emplace(data->stream_id);
      } else {
        LOGE(RtspSink) << "GetContext() input video stream count over " <<
            params.tile_cols << " * " << params.tile_rows << " = " << params.tile_cols * params.tile_rows;
        return nullptr;
      }
    }
  } else {
    auto search = contexts_.find(data->stream_id);
    if (search != contexts_.end()) {
      ctx = search->second;
    } else {
      ctx = CreateContext(data, data->stream_id);
    }
  }
  return ctx;
}

RtspSinkContext * RtspSink::CreateContext(CNFrameInfoPtr data, const std::string &stream_id) {
  if (data->IsEos()) return nullptr;

  CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
  auto params = param_helper_->GetParams();
  RtspSinkContext *ctx = new RtspSinkContext;

  constexpr int time_base = 90000;

  VideoStream::Param sparam;
  sparam.width = params.width > 0 ? params.width : frame->width;
  sparam.height = params.height > 0 ? params.height : frame->height;
  sparam.tile_cols = params.tile_cols;
  sparam.tile_rows = params.tile_rows;
  sparam.resample = params.resample;
  sparam.frame_rate = params.frame_rate;
  sparam.time_base = time_base;
  sparam.bit_rate = params.bit_rate;
  sparam.gop_size = params.gop_size;
  VideoPixelFormat pixel_format =
      frame->fmt == CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12 ? VideoPixelFormat::NV12 : VideoPixelFormat::NV21;
  sparam.pixel_format = (params.mlu_encoder || params.mlu_input_frame) ? pixel_format : VideoPixelFormat::I420;
  sparam.codec_type = VideoCodecType::H264;
  sparam.mlu_encoder = params.mlu_encoder;
  sparam.device_id = params.device_id;
  ctx->stream.reset(new VideoStream(sparam));
  if (!ctx->stream) {
    LOGE(RtspSink) << "CreateContext() create video stream failed";
    delete ctx;
    return nullptr;
  }

  auto get_packet = [time_base](VideoStream *stream, uint8_t *data, int size, double *timestamp, int *buffer_percent) {
    if (!stream) return -1;
    VideoPacket packet, *pkt;
    VideoStream::PacketInfo info;
    memset(&packet, 0, sizeof(VideoPacket));
    if (size < 0) {  // skip packet
      pkt = nullptr;
    } else if (!data) {  // get packet size/timestamp
      packet.data = nullptr;
      pkt = &packet;
    } else {  // read out packet data
      packet.data = data;
      packet.size = size;
      pkt = &packet;
    }
    int ret = stream->GetPacket(pkt, &info);
    if (ret > 0) {
      if (pkt && timestamp) *timestamp = static_cast<double>(pkt->pts) / time_base;
      if (buffer_percent) *buffer_percent = info.buffer_size * 100 / info.buffer_capacity;
    }
    return ret;
  };

  RtspServer::Param rparam;
  rparam.port = params.port + stream_index_++;
  rparam.rtsp_over_http = params.rtsp_over_http;
  rparam.authentication = false;
  rparam.width = sparam.width;
  rparam.height = sparam.height;
  rparam.bit_rate = sparam.bit_rate;
  rparam.codec_type = sparam.codec_type == VideoCodecType::H264 ? RtspServer::H264 : RtspServer::H265;
  rparam.get_packet = std::bind(get_packet, ctx->stream.get(), std::placeholders::_1, std::placeholders::_2,
                                std::placeholders::_3, std::placeholders::_4);
  ctx->server.reset(new RtspServer(rparam));
  if (!ctx->server) {
    LOGE(RtspSink) << "CreateContext() create rtsp server failed";
    delete ctx;
    return nullptr;
  }
  if (!ctx->server->Start()) {
    LOGE(RtspSink) << "CreateContext() start rtsp server failed";
    delete ctx;
    return nullptr;
  }

  auto event_callback = [](RtspServer *server, VideoStream::Event event) {
    if (!server) return;
    if (event == VideoStream::Event::EVENT_DATA) {
      server->OnEvent(RtspServer::Event::EVENT_DATA);
    } else if (event == VideoStream::Event::EVENT_EOS) {
      LOGI(RtspSink) << "CreateContext() EVENT_EOS";
      server->OnEvent(RtspServer::Event::EVENT_EOS);
    } else if (event == VideoStream::Event::EVENT_ERROR) {
      LOGE(RtspSink) << "EventCallback() EVENT_ERROR";
    }
  };

  if (!ctx->stream->Open()) {
    LOGE(RtspSink) << "CreateContext() open video stream failed";
    ctx->server->Stop();
    delete ctx;
    return nullptr;
  }
  ctx->stream->SetEventCallback(std::bind(event_callback, ctx->server.get(), std::placeholders::_1));

  contexts_[stream_id] = ctx;
  return ctx;
}

RtspSink::~RtspSink() {
  if (param_helper_) {
    delete param_helper_;
    param_helper_ = nullptr;
  }
  Close();
}

bool RtspSink::Open(ModuleParamSet paramSet) {
  if (!param_helper_->ParseParams(paramSet)) {
    LOGE(RtspSink) << "[" << GetName() << "] parse parameters failed.";
    return false;
  }
  auto params = param_helper_->GetParams();
  if (params.mlu_encoder && params.device_id < 0) {
    LOGE(RtspSink) << "Open() mlu encoder, but specified device_id < 0";
    return false;
  }
  if (params.mlu_input_frame && (params.tile_cols > 1 || params.tile_rows > 1)) {
    LOGE(RtspSink) << "Open() mlu input tiling is not supported";
    return false;
  }
  return true;
}

void RtspSink::Close() {
  if (contexts_.empty()) {
    return;
  }
  for (auto &it : contexts_) {
    RtspSinkContext *ctx = it.second;
    if (ctx) {
      if (ctx->stream) ctx->stream->Close();
      if (ctx->server) ctx->server->Stop();
      delete ctx;
    }
  }
  contexts_.clear();
}

int RtspSink::Process(CNFrameInfoPtr data) {
  RtspSinkContext *ctx = GetContext(data);
  auto params = param_helper_->GetParams();
  if (!ctx) {
    LOGE(RtspSink) << "Get RtspSink Context Failed.";
    return -1;
  }

  CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);

  if (!params.mlu_input_frame) {
    if (!ctx->stream->Update(frame->ImageBGR(), VideoStream::ColorFormat::BGR, data->timestamp, data->stream_id)) {
      LOGE(RtspSink) << "Process() video stream update failed";
    }
  } else {
#ifndef HAVE_CNCV
    if (params.mlu_input_frame && params.mlu_encoder) {
      LOGE(RtspSink) << "Process() Encode mlu input frame on mlu is not supported. Please install CNCV.";
      return -1;
    }
#endif
    if (frame->dst_device_id != params.device_id) {
      LOGE(RtspSink) << "Process() Encode mlu input frame on different device is not supported";
      return -1;
    }
    VideoStream::Buffer buffer;
    memset(&buffer, 0, sizeof(VideoStream::Buffer));
    buffer.width = frame->width;
    buffer.height = frame->height;
    buffer.data[0] = static_cast<uint8_t *>(const_cast<void *>(frame->data[0]->GetMluData()));
    buffer.data[1] = static_cast<uint8_t *>(const_cast<void *>(frame->data[1]->GetMluData()));
    buffer.stride[0] = frame->stride[0];
    buffer.stride[1] = frame->stride[1];
    buffer.color = frame->fmt == CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12 ? VideoStream::ColorFormat::YUV_NV12
                                                                           : VideoStream::ColorFormat::YUV_NV21;
    buffer.mlu_device_id = frame->dst_device_id;
    if (!ctx->stream->Update(&buffer, data->timestamp, data->stream_id)) {
      LOGE(RtspSink) << "Process() video stream update failed";
    }
  }

  return 0;
}

RtspSink::RtspSink(const std::string &name) : Module(name) {
  param_register_.SetModuleDesc("RtspSink is a module to deliver stream by RTSP protocol.");
  param_helper_ = new ModuleParamsHelper<RtspSinkParam>(name);
  auto input_encoder_type_parser = [](const ModuleParamSet &param_set, const std::string &param_name,
                                      const std::string &value, void *result) -> bool {
    if (value == "cpu") {
      *static_cast<bool *>(result) = false;
    } else if (value == "mlu") {
      *static_cast<bool *>(result) = true;
    } else {
      LOGE(RtspSink) << "[ModuleParamParser] [" << param_name << "]:" << value << " failed"
                     << "\". Choose from \"mlu\", \"cpu\".";
      return false;
    }
    return true;
  };

  static const std::vector<ModuleParamDesc> regist_param = {
      {"port", "8554", "RTSP port.", PARAM_REQUIRED, OFFSET(RtspSinkParam, port), ModuleParamParser<int>::Parser,
       "int"},
      {"rtsp_over_http", "false", "RTSP Over HTTP.", PARAM_OPTIONAL, OFFSET(RtspSinkParam, rtsp_over_http),
       ModuleParamParser<bool>::Parser, "bool"},
      {"device_id", "0", "Which MLU device will be used.", PARAM_OPTIONAL, OFFSET(RtspSinkParam, device_id),
       ModuleParamParser<int>::Parser, "int"},
      {"encoder_type", "mlu", "Selection for encoder type. It should be 'mlu' or 'cpu.", PARAM_OPTIONAL,
       OFFSET(RtspSinkParam, mlu_encoder), input_encoder_type_parser, "bool"},
      {"input_frame", "cpu", "Frame source type. It should be 'mlu' or 'cpu'.", PARAM_OPTIONAL,
       OFFSET(RtspSinkParam, mlu_input_frame), input_encoder_type_parser, "bool"},
      {"dst_width", "0", "Output video width. 0 means dst width is same with source", PARAM_OPTIONAL,
       OFFSET(RtspSinkParam, width), ModuleParamParser<int>::Parser, "int"},
      {"dst_height", "0", "Output video height. 0 means dst height is same with source", PARAM_OPTIONAL,
       OFFSET(RtspSinkParam, height), ModuleParamParser<int>::Parser, "int"},
      {"frame_rate", "30", "Frame rate of video encoding. Higher value means more fluent.", PARAM_OPTIONAL,
       OFFSET(RtspSinkParam, frame_rate), ModuleParamParser<double>::Parser, "double"},
      {"bit_rate", "4000000", "Bit rate of video encoding. Higher value means better video quality.", PARAM_OPTIONAL,
       OFFSET(RtspSinkParam, bit_rate), ModuleParamParser<int>::Parser, "int"},
      {"gop_size", "10", "Group of pictures. gop_size is the number of frames between two IDR frames.", PARAM_OPTIONAL,
       OFFSET(RtspSinkParam, gop_size), ModuleParamParser<int>::Parser, "int"},
      {"view_cols", "1", "Grids in horizontally of video tiling, only support cpu input.", PARAM_OPTIONAL,
       OFFSET(RtspSinkParam, tile_cols), ModuleParamParser<int>::Parser, "int"},
      {"view_rows", "1", "Grids in vertically of video tiling, only support cpu input.", PARAM_OPTIONAL,
       OFFSET(RtspSinkParam, tile_rows), ModuleParamParser<int>::Parser, "int"},
      {"resample", "false", "Resample frame with canvas, only support cpu input.", PARAM_OPTIONAL,
       OFFSET(RtspSinkParam, resample), ModuleParamParser<bool>::Parser, "bool"},
      {"udp_port", "", "Replaced by port", PARAM_DEPRECATED},
      {"http_port", "", "Replaced by rtsp_over_http", PARAM_DEPRECATED},
      {"kbit_rate", "", "Replaced by bit_rate", PARAM_DEPRECATED},
      {"view_mode", "", "Replaced by view_rows & view_cols", PARAM_DEPRECATED},
      {"preproc_type", "", "selected automatically.", PARAM_DEPRECATED},
      {"color_mode", "", "selected automatically.", PARAM_DEPRECATED}};

  param_helper_->Register(regist_param, &param_register_);
  hasTransmit_.store(0);  // for receive eos
}

void RtspSink::OnEos(const std::string &stream_id) {
  auto params = param_helper_->GetParams();
  std::lock_guard<std::mutex> lk(ctx_lock_);
  if (params.tile_cols > 1 || params.tile_rows > 1) {
    if (contexts_.empty()) return;
    RtspSinkContext *ctx = contexts_.begin()->second;
    // clear last grid
    if (ctx->stream) ctx->stream->Clear(stream_id);
    tile_streams_.erase(stream_id);
    if (tile_streams_.empty()) {
      LOGI(RtspSink) << "OnEos() all streams stopped";
      RtspSinkContext *ctx = contexts_.begin()->second;
      if (ctx) {
        if (ctx->stream) ctx->stream->Close();
        if (ctx->server) ctx->server->Stop();
        delete ctx;
      }
      contexts_.clear();
    }
  } else {
    auto search = contexts_.find(stream_id);
    if (search != contexts_.end()) {
      RtspSinkContext *ctx = search->second;
      if (ctx) {
        if (ctx->stream) ctx->stream->Close();
        if (ctx->server) ctx->server->Stop();
        delete ctx;
      }
      contexts_.erase(stream_id);
    }
  }
}

}  // namespace cnstream
