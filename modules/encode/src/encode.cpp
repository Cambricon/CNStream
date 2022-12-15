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
#include "encode.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cnedk_platform.h"

#include "cnstream_frame_va.hpp"
#include "cnstream_logging.hpp"

#include "encode_handler_mlu.hpp"
#include "encode_handler_ffmpeg.hpp"

#include "fmp4_muxer/fmp4_muxer.hpp"
#include "rtsp/rtsp_sink.hpp"

namespace cnstream {

struct VEncImplParam {
  VEncParam venc_param;
  std::string stream_id = "";
  uint32_t stream_index;
  uint32_t stream_width;
  uint32_t stream_height;
};

static inline int64_t CurrentTick() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

class VEncodeImplement {
 public:
  VEncodeImplement() {}
  ~VEncodeImplement() {
    if (handler_) {
      handler_.reset();
      handler_ = nullptr;
    }
    if (mp4_muxer_) {
      mp4_muxer_->Close();
      mp4_muxer_.reset();
      mp4_muxer_ = nullptr;
    }
    if (rtsp_sink_) {
      rtsp_sink_->Close();
      rtsp_sink_.reset();
      rtsp_sink_ = nullptr;
    }
    if (file_.is_open()) {
      file_.close();
    }
  }

  int SetParams(VEncImplParam iparams) {
    VEncParam& params = iparams.venc_param;

    if (params.mlu_encoder) {
      handler_.reset(new VencMluHandler(params.device_id));
    } else {
      handler_.reset(new VEncodeFFmpegHandler());
    }

    if (!params.file_name.empty()) {
      auto dot = params.file_name.find_last_of(".");
      if (dot == std::string::npos) {
        LOGE(VENC) << "Process() unknown file type \"" << params.file_name << "\"";
      } else {
        std::string file_name = params.file_name.substr(0, dot);
        std::string ext_name = params.file_name.substr(dot + 1);

        std::string lower_file_name = file_name;
        std::string lower_ext_name = ext_name;
        std::transform(lower_file_name.begin(), lower_file_name.end(), lower_file_name.begin(), ::tolower);
        std::transform(lower_ext_name.begin(), lower_ext_name.end(), lower_ext_name.begin(), ::tolower);

        if (lower_file_name.find("hevc") != std::string::npos || lower_file_name.find("h265") != std::string::npos ||
            lower_ext_name.find("hevc") != std::string::npos || lower_ext_name.find("h265") != std::string::npos) {
          handle_param_.codec_type = VideoCodecType::H265;
        }
        if (lower_ext_name == "mp4") {
          with_container_ = true;
        } else if (lower_ext_name == "jpg" || lower_ext_name == "jpeg") {
          jpeg_file_name_ = file_name + "_" + iparams.stream_id;
          jpeg_ext_name_ = ext_name;
          LOGI(VENC) << "jpeg_file_name " << jpeg_file_name_ << std::endl;
          handle_param_.codec_type = VideoCodecType::JPEG;
        }
        file_name_ = file_name + "_" + iparams.stream_id + "." + ext_name;
      }
    }

    handle_param_.width = params.dst_width;
    handle_param_.height = params.dst_height;
    handle_param_.frame_rate = params.frame_rate;
    handle_param_.bitrate = params.bit_rate;
    handle_param_.gop_size = params.gop_size;

    if (handle_param_.width == 0) {
      handle_param_.width = iparams.stream_width;
    }
    if (handle_param_.height == 0) {
      handle_param_.height = iparams.stream_height;
    }

    if (with_container_ && !mp4_muxer_ && params.mlu_encoder) {  // fix me ffmpeg encode not suport mp4
      mp4_muxer_.reset(new Mp4Muxer());
      if (mp4_muxer_) {
        int ret = mp4_muxer_->Open(file_name_, handle_param_.width, handle_param_.height, handle_param_.codec_type);
        if (ret < 0) {
          mp4_muxer_.reset();
          mp4_muxer_ = nullptr;
          LOGE(VENC) << "failed to create mp4 muxer, stream_id = " << iparams.stream_id;
        }
      }
    }

    if (params.rtsp_port > 0) {
      rtsp_sink_.reset(new RtspSink());
      if (rtsp_sink_) {
        int ret = rtsp_sink_->Open(params.rtsp_port + iparams.stream_index);
        if (ret < 0) {
          rtsp_sink_.reset();
          rtsp_sink_ = nullptr;
          LOGE(VENC) << "failed to create rtsp server, stream_id = " << iparams.stream_id;
        }
      }
    }

    handle_param_.on_framebits = std::bind(&VEncodeImplement::OnFrameBits, this, iparams, std::placeholders::_1);

    handler_->SetParams(handle_param_);
    return 0;
  }

  void SetFrameRate(uint32_t frame_rate) {
    if (frame_rate) frame_rate_ = frame_rate;
  }

  int OnFrameBits(VEncImplParam iparams, CnedkVEncFrameBits *framebits) {
    framebits->pts = static_cast<uint64_t>(frame_count_++ * 1000 / frame_rate_);
    if (mp4_muxer_) {
      mp4_muxer_->Write(framebits);
    } else if (!file_name_.empty()) {
      if (handle_param_.codec_type == VideoCodecType::JPEG) {
        std::string jpeg_file_name =
            jpeg_file_name_ + "_" + std::to_string(frame_count_) + "." + jpeg_ext_name_;
        std::ofstream file(jpeg_file_name);
        if (!file.good()) {
          LOGE(VENC) << "OnFrameBits() open " << jpeg_file_name << " failed";
        } else {
          file.write(reinterpret_cast<char *>(framebits->bits), framebits->len);
        }
        file.close();
      } else {
        if (!file_.is_open()) {
          file_.open(file_name_);
          if (!file_.good()) {
            LOGE(VENC) << "EventCallback() open " << file_name_ << " failed";
            return -1;
          }
        }
        if (file_.good()) file_.write(reinterpret_cast<char *>(framebits->bits), framebits->len);
      }
    }
    if (rtsp_sink_) rtsp_sink_->SendFrame(framebits);
    return 0;
  }

  int SendFrame(std::shared_ptr<CNFrameInfo> data) {
    return handler_->SendFrame(data);
  }

  int SendFrame(Scaler::Buffer* data) {
    return handler_->SendFrame(data);
  }

  void Close() {
    if (handler_) {
      handler_.reset(nullptr);
    }
    if (mp4_muxer_) {
      mp4_muxer_->Close();
      mp4_muxer_.reset(nullptr);
    }
    if (rtsp_sink_) {
      rtsp_sink_->Close();
      rtsp_sink_.reset(nullptr);
    }
  }

 private:
  std::unique_ptr<VencHandler> handler_ = nullptr;
  std::unique_ptr<Mp4Muxer> mp4_muxer_ = nullptr;
  std::unique_ptr<RtspSink> rtsp_sink_ = nullptr;

  VEncHandlerParam handle_param_;

  std::string file_name_ = "";
  uint32_t stream_index_;

  bool with_container_ = false;
  std::string jpeg_file_name_ = "";
  std::string jpeg_ext_name_ = "";

  uint64_t frame_count_ = 0;
  std::ofstream file_;
  uint32_t frame_rate_ = 25;
};


class FrameRateControl {
 public:
  explicit FrameRateControl(uint32_t target_fps) : target_fps_(target_fps) {}
  ~FrameRateControl() = default;
  void UpdateFrame() {
    uint64_t cur_ms = CurrentTick();
    if (frame_count_  == 0) {
      priv_ms_ = cur_ms;
    }
    frame_count_++;
    if ((cur_ms - priv_ms_) > 1000) {  // every 1s update framerate
      input_fps_ = frame_count_ * 1000 / (cur_ms - priv_ms_);
      priv_ms_ = cur_ms;
      frame_count_ = 0;
    }
  }

  uint32_t GetInputFrameRate() {
    return input_fps_;
  }

  bool IsKeyFrame() {
    bool ret = false;
    key_frame_ += target_fps_;
    if (key_frame_ >= input_fps_) {
      key_frame_ -= input_fps_;
      ret = true;
    }
    return ret;
  }

 private:
  uint32_t  input_fps_ = 30;
  uint32_t  target_fps_;
  uint32_t  key_frame_ = 0;
  uint32_t  frame_count_ = 0;
  uint64_t  priv_ms_;
};

VEncode::VEncode(const std::string &name) : ModuleEx(name) {
  param_register_.SetModuleDesc("VEncode is a module to encode videos or images."
                                 "And save to file or deliver by RTSP protocol.");
  param_helper_.reset(new (std::nothrow) ModuleParamsHelper<VEncParam>(name));

  static const std::vector<ModuleParamDesc> register_param = {
      {"device_id", "0", "Which device will be used.", PARAM_OPTIONAL, OFFSET(VEncParam, device_id),
        ModuleParamParser<int>::Parser, "int"},
      {"hw_accel", "true", "use hardware to encode", PARAM_OPTIONAL,
        OFFSET(VEncParam, mlu_encoder), ModuleParamParser<bool>::Parser, "bool"},
      {"dst_width", "0", "Output video width. 0 means dst width is same with source", PARAM_OPTIONAL,
        OFFSET(VEncParam, dst_width), ModuleParamParser<int>::Parser, "int"},
      {"dst_height", "0", "Output video height. 0 means dst height is same with source", PARAM_OPTIONAL,
        OFFSET(VEncParam, dst_height), ModuleParamParser<int>::Parser, "int"},
      {"view_cols", "1", "Grids in horizontally of video tiling, only support cpu input.", PARAM_OPTIONAL,
       OFFSET(VEncParam, tile_cols), ModuleParamParser<int>::Parser, "int"},
      {"view_rows", "1", "Grids in vertically of video tiling, only support cpu input.", PARAM_OPTIONAL,
       OFFSET(VEncParam, tile_rows), ModuleParamParser<int>::Parser, "int"},
      {"resample", "false", "Resample. If set true, some frame will be dropped.", PARAM_OPTIONAL,
       OFFSET(VEncParam, resample), ModuleParamParser<bool>::Parser, "bool"},
      {"frame_rate", "25", "Frame rate of video encoding. Higher value means more fluent.", PARAM_OPTIONAL,
        OFFSET(VEncParam, frame_rate), ModuleParamParser<double>::Parser, "double"},
      {"bit_rate", "4000000", "Bit rate of video encoding. Higher value means better video quality.", PARAM_OPTIONAL,
        OFFSET(VEncParam, bit_rate), ModuleParamParser<int>::Parser, "int"},
      {"gop_size", "10", "Group of pictures. gop_size is the number of frames between two IDR frames.", PARAM_OPTIONAL,
        OFFSET(VEncParam, gop_size), ModuleParamParser<int>::Parser, "int"},
      {"file_name", "",
        "File name and path to store, the final name will be added with stream id or frame count", PARAM_OPTIONAL,
        OFFSET(VEncParam, file_name), ModuleParamParser<std::string>::Parser, "string"},
      {"rtsp_port", "-1", "RTSP port. If this value is greater than 0, stream will be delivered by RTSP protocol.",
        PARAM_OPTIONAL, OFFSET(VEncParam, rtsp_port),
        ModuleParamParser<int>::Parser, "int"}
  };
  param_helper_->Register(register_param, &param_register_);
}

VEncode::~VEncode() {}

bool VEncode::Open(ModuleParamSet param_set) {
  if (false == CheckParamSet(param_set)) {
    return false;
  }

  auto params = param_helper_->GetParams();
  if (params.tile_rows > 1 || params.tile_cols > 1) {
    tiler_enable_ = true;
  }

  return true;
}


bool VEncode::CheckParamSet(const ModuleParamSet& param_set) const {
  if (!param_helper_->ParseParams(param_set)) {
    LOGE(VENC) << "[" << GetName() << "] parse parameters failed.";
    return false;
  }

  auto params = param_helper_->GetParams();

  if (params.dst_height % 2 != 0 || params.dst_width % 2 != 0) {
    LOGE(VENC) << "[" << GetName() << "] dst width and height must be even, which dst_width: " << params.dst_width
               << ", dst_height: " << params.dst_height;
    return false;
  }

  if (params.mlu_encoder) {
    uint32_t dev_cnt = 0;
    if (cnrtGetDeviceCount(&dev_cnt) != cnrtSuccess || params.device_id < 0 ||
        static_cast<uint32_t>(params.device_id) >= dev_cnt) {
      LOGE(VENC) << "[" << GetName() << "] hardware encoding, device " << params.device_id << " does not exist.";
      return false;
    }
  }

  return true;
}

void VEncode::Close() {
  if (tiler_) {
    Scaler::Buffer* encode_buffer = nullptr;
    ivenc_[tiler_key_name_]->SendFrame(encode_buffer);
    ivenc_[tiler_key_name_]->Close();
    ivenc_.clear();
    tiler_.reset();
    tiler_ = nullptr;
  }

  for (auto iter = ivenc_.begin(); iter != ivenc_.end(); ++iter) {
    iter->second->Close();
    iter->second.reset();
    iter->second = nullptr;
  }
}

int VEncode::Process(std::shared_ptr<CNFrameInfo> data) {
  if (!data) {
    LOGE(VENC) << "Process input data is nulltpr!";
    return -1;
  }

  if (!data->IsEos() && data->IsRemoved()) {
    return 0;
  }

  if (!data->IsEos()) {
    CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
    if (!frame->buf_surf) {
      TransmitData(data);
      LOGE(VENC) << "surface is nulltpr!";
      return -1;
    }
    auto params = param_helper_->GetParams();

    if (params.resample) {
      std::unique_lock<std::mutex> frame_rate_guard(frame_rate_mutex_);
      if (!frame_rate_ctx_.count(data->stream_id)) {
        frame_rate_ctx_[data->stream_id] = std::make_shared<FrameRateControl>(params.frame_rate);
      }
      frame_rate_ctx_[data->stream_id]->UpdateFrame();
      bool key_frame = frame_rate_ctx_[data->stream_id]->IsKeyFrame();
      frame_rate_guard.unlock();
      if (!key_frame) {
        TransmitData(data);
        return 0;   // resample
      }
    }

    std::unique_lock<std::mutex> guard(venc_mutex_);
    if (tiler_enable_ && !ivenc_.count(tiler_key_name_)) {   // create tiler context
      ivenc_[tiler_key_name_] = std::make_shared<VEncodeImplement>();
      uint32_t width = params.dst_width;
      uint32_t height = params.dst_height;

      if (width == 0) {
        width = frame->buf_surf->GetWidth();
      }

      if (height == 0) {
        height = frame->buf_surf->GetHeight();
      }

      tiler_.reset(new (std::nothrow) Tiler(params.tile_cols, params.tile_rows,
                                            Scaler::ColorFormat::YUV_NV12, width, height));

      VEncImplParam iparam;
      iparam.venc_param = params;
      iparam.stream_id = data->stream_id;
      iparam.stream_index = 0;

      iparam.stream_height = frame->buf_surf->GetHeight();
      iparam.stream_width = frame->buf_surf->GetWidth();
      ivenc_[tiler_key_name_]->SetParams(iparam);

    } else if (!tiler_enable_ && !ivenc_.count(data->stream_id)) {  // create normal context
      VEncImplParam iparam;
      ivenc_[data->stream_id] = std::make_shared<VEncodeImplement>();
      iparam.venc_param = params;
      iparam.stream_id = data->stream_id;
      iparam.stream_index = data->GetStreamIndex();

      iparam.stream_height = frame->buf_surf->GetHeight();
      iparam.stream_width = frame->buf_surf->GetWidth();
      ivenc_[data->stream_id]->SetParams(iparam);
    }
    guard.unlock();

    if (tiler_) {   // enable tiler
      std::unique_lock<std::mutex> lk(venc_mutex_);
      CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
      Scaler::Buffer buffer;
      Scaler::MatToBuffer(frame->ImageBGR(), Scaler::ColorFormat::BGR, &buffer);

      tiler_->Blit(&buffer, data->GetStreamIndex());
      static int64_t last_tick = 0;
      int64_t tick = CurrentTick();
      if ((tick - last_tick) >= (1000 / params.frame_rate)) {
        Scaler::Buffer* encode_buffer = tiler_->GetCanvas();
        ivenc_[tiler_key_name_]->SetFrameRate(params.frame_rate);
        ivenc_[tiler_key_name_]->SendFrame(encode_buffer);
        tiler_->ReleaseCanvas();
        last_tick = tick;
      }
      lk.unlock();
    } else {
      ivenc_[data->stream_id]->SetFrameRate(params.frame_rate);
      ivenc_[data->stream_id]->SendFrame(data);
    }
  } else {
    std::unique_lock<std::mutex> frame_rate_guard(frame_rate_mutex_);
    auto frame_rate_iter = frame_rate_ctx_.find(data->stream_id);
    if (frame_rate_iter != frame_rate_ctx_.end()) {
      frame_rate_ctx_.erase(data->stream_id);
    }
    frame_rate_guard.unlock();

    auto iter = ivenc_.find(data->stream_id);
    if (iter != ivenc_.end()) {
      ivenc_[data->stream_id]->SendFrame(data);
      ivenc_[data->stream_id]->Close();
      ivenc_.erase(data->stream_id);
    }
  }
  TransmitData(data);  // data forwarded by this module
  return 0;
}

}  // namespace cnstream
