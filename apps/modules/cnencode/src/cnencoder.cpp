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
#include "cnencoder.hpp"
#include "easycodec/easy_encode.h"
#include "easycodec/vformat.h"
#include "easyinfer/mlu_context.h"

namespace cnstream {

CNEncoder::CNEncoder(const std::string &name) : Module(name) {
  param_register_.SetModuleDesc("CNEncoder is a module to encode use cnencode.");
  param_register_.Register("frame_rate", "Frame rate.");
  param_register_.Register("bit_rate", "Bit rate.");
  param_register_.Register("gop_size", "Gop size.");
  param_register_.Register("device_id", "Device_Id.");
  hasTransmit_.store(1);  // for receive eos
}

CNEncoderContext *CNEncoder::GetCNEncoderContext(CNFrameInfoPtr data) {
  CNEncoderContext *ctx = nullptr;
  auto search = ctxs_.find(data->channel_idx);
  if (search != ctxs_.end()) {
    ctx = search->second;
  } else {
    ctx = new CNEncoderContext;
    // build cnencoder
    ctx->stream_ = new CNEncoderStream(data->frame.width, data->frame.height, frame_rate_,
                  cn_format_, bit_rate_, gop_size_, cn_type_, data->channel_idx, device_id_);
    // open cnencoder
    ctx->stream_->Open();
    // add into map
    ctxs_[data->channel_idx] = ctx;
  }
  return ctx;
}

CNEncoder::~CNEncoder() { Close(); }

bool CNEncoder::Open(ModuleParamSet paramSet) {
  if (paramSet.find("frame_rate") == paramSet.end()) {
    frame_rate_ = 25;
  } else {
    frame_rate_ = std::stoi(paramSet["frame_rate"]);
  }
  if (paramSet.find("bit_rate") == paramSet.end()) {
    bit_rate_ = 0x100000;
  } else {
    bit_rate_ = std::stoi(paramSet["bit_rate"]);
    bit_rate_ *= 1024;
  }
  if (paramSet.find("gop_size") == paramSet.end()) {
    gop_size_ = 10;
  } else {
    gop_size_ = std::stoi(paramSet["gop_size"]);
  }
  if (paramSet.find("device_id") == paramSet.end()) {
    device_id_ = 0;
  } else {
    device_id_ = std::stoi(paramSet["device_id"]);
  }

  cn_type_ = CNEncoderStream::H264;
  cn_format_ = CNEncoderStream::NV12;

  edk::MluContext tx;
  tx.SetDeviceId(device_id_);
  tx.ConfigureForThisThread();
  return true;
}

void CNEncoder::Close() {
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

int CNEncoder::Process(CNFrameInfoPtr data) {
  bool eos = data->frame.flags & CNFrameFlag::CN_FRAME_FLAG_EOS;

  CNEncoderContext *ctx = GetCNEncoderContext(data);
  if (!eos) {
    cv::Mat image = *data->frame.ImageBGR();
    ctx->stream_->Update(image, data->frame.timestamp, data->channel_idx);
  } else {
    ctx->stream_->RefreshEOS(eos);
    // TransmitData(data);
  }
  return 0;
}

bool CNEncoder::CheckParamSet(ModuleParamSet paramSet) {
  ParametersChecker checker;
  for (auto& it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOG(WARNING) << "[CNEncoder] Unknown param: " << it.first;
    }
  }

  if (paramSet.find("frame_rate") == paramSet.end()
      || paramSet.find("bit_rate") == paramSet.end()
      || paramSet.find("gop_size") == paramSet.end()
      || paramSet.find("device_id") == paramSet.end()) {
    LOG(ERROR) << "CNEncoder must specify [frame_rate], [bit_rate], [gop_size], [device_id].";
    return false;
  }

  std::string err_msg;
  if (!checker.IsNum({"frame_rate", "bit_rate", "gop_size", "device_id"},
        paramSet, err_msg, true)) {
    LOG(ERROR) << "[CNEncoder] " << err_msg;
    return false;
  }
  return true;
}

}  // namespace cnstream
