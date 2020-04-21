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

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "cnstream_eventbus.hpp"
#include "rtsp_sink.hpp"

RtspSink::RtspSink(const std::string &name) : Module(name) {
  param_register_.SetModuleDesc("RtspSink is a module to deliver stream by RTSP protocol.");
  param_register_.Register("http-port", "Http port.");
  param_register_.Register("udp-port", "UDP port.");
  param_register_.Register("encoder-type", "Encode type. It should be 'mlu' or not 'mlu' but other string.");
  param_register_.Register("frame-rate", "Frame rate.");
  param_register_.Register("cols", "Video width.");
  param_register_.Register("rows", "Video height.");
  param_register_.Register("device_id", "Which device will be used. If there is only one device, it might be 0.");
  // hasTransmit_.store(1);  // for receive eos
}

RtspSinkContext *RtspSink::GetRtspSinkContext(CNFrameInfoPtr data) {
  RtspSinkContext *ctx = nullptr;
  if (is_mosaic_style_) {
    if (!is_get_channel) {
      is_get_channel = true;
      get_channel = data->channel_idx;
    }
    auto search = ctxs_.find(get_channel);
    if (search != ctxs_.end() && is_get_channel) {
      ctx = search->second;
    } else {
      ctx = new RtspSinkContext;
      ctx->stream_ = new RTSPSinkJoinStream;

      if (!ctx->stream_->Open(data->frame.width, data->frame.height, format_, frame_rate_ /* 30000.0f / 1001 */,
                              udp_port_, http_port_, rows_, cols_, device_id_,
                              enc_type == "mlu" ? RTSPSinkJoinStream::MLU : RTSPSinkJoinStream::FFMPEG)) {
        LOG(ERROR) << "[RTSPSink] Invalid parameter";
      }
      ctxs_[data->channel_idx] = ctx;
    }
  } else {
    auto search = ctxs_.find(data->channel_idx);
    if (search != ctxs_.end()) {
      // context exists
      ctx = search->second;
    } else {
      ctx = new RtspSinkContext;
      ctx->stream_ = new RTSPSinkJoinStream;

      if (!ctx->stream_->Open(data->frame.width, data->frame.height, format_, frame_rate_ /* 30000.0f / 1001 */,
                              udp_port_ + data->channel_idx, http_port_, -1, -1, device_id_,
                              enc_type == "mlu" ? RTSPSinkJoinStream::MLU : RTSPSinkJoinStream::FFMPEG)) {
        LOG(ERROR) << "[RTSPSink] Invalid parameter";
      }
      ctxs_[data->channel_idx] = ctx;
    }
  }
  return ctx;
}

RtspSink::~RtspSink() { Close(); }

bool RtspSink::Open(ModuleParamSet paramSet) {
  if (paramSet.find("http-port") == paramSet.end() || paramSet.find("udp-port") == paramSet.end() ||
      paramSet.find("encoder-type") == paramSet.end()) {
    return false;
  }

  http_port_ = std::stoi(paramSet["http-port"]);
  udp_port_ = std::stoi(paramSet["udp-port"]);
  enc_type = paramSet["encoder-type"];
  if (paramSet.find("frame-rate") == paramSet.end()) {
    frame_rate_ = 0;
  } else {
    frame_rate_ = std::stof(paramSet["frame-rate"]);
    if (frame_rate_ < 0) frame_rate_ = 0;
  }

  if (paramSet.find("cols") != paramSet.end() && paramSet.find("rows") != paramSet.end()) {
    cols_ = std::stoi(paramSet["cols"]);
    rows_ = std::stoi(paramSet["rows"]);
    is_mosaic_style_ = true;
    LOG(INFO) << "mosaic windows cols: " << cols_ << " ,rows: " << rows_;
  }

  if (paramSet.find("device_id") == paramSet.end()) {
    device_id_ = 0;
  } else {
    device_id_ = std::stoi(paramSet["device_id"]);
  }

  format_ = RTSPSinkJoinStream::NV21;  // BGR24

  return true;
}

void RtspSink::Close() {
  if (ctxs_.empty()) {
    return;
  }
  for (auto &pair : ctxs_) {
    pair.second->stream_->Close();
    delete pair.second->stream_;
    delete pair.second;
  }
  ctxs_.clear();
}

int RtspSink::Process(CNFrameInfoPtr data) {
  // bool eos = data->frame.flags & CNFrameFlag::CN_FRAME_FLAG_EOS;
  RtspSinkContext *ctx = GetRtspSinkContext(data);
  cv::Mat image = *data->frame.ImageBGR();
  if (is_mosaic_style_) {
    ctx->stream_->Update(image, data->frame.timestamp, data->channel_idx);
  } else {
    ctx->stream_->Update(image, data->frame.timestamp);
  }
  return 0;
}

bool RtspSink::CheckParamSet(const ModuleParamSet &paramSet) const {
  ParametersChecker checker;
  for (auto &it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOG(WARNING) << "[RtspSink] Unknown param: " << it.first;
    }
  }

  if (paramSet.find("http-port") == paramSet.end() || paramSet.find("udp-port") == paramSet.end() ||
      paramSet.find("encoder-type") == paramSet.end() || paramSet.find("device_id") == paramSet.end()) {
    LOG(ERROR) << "RtspSink must specify [http-port], [udp-port], [encoder-type], [device_id].";
    return false;
  }

  std::string err_msg;
  if (!checker.IsNum({"http-port", "udp-port", "frame-rate", "cols", "rows", "device_id"}, paramSet, err_msg, true)) {
    LOG(ERROR) << "[RtspSink] " << err_msg;
    return false;
  }

  if (paramSet.at("encoder-type") != "mlu" && paramSet.at("encoder-type") != "ffmpeg") {
    LOG(ERROR) << "[RtspSink] Not support encoder type: " << paramSet.at("encoder-type");
    return false;
  }
  return true;
}
