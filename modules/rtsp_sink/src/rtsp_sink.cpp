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
#include "rtsp_sink_stream.hpp"

#include <memory>
#include <sstream>
#include <string>

#include "cnstream_frame_va.hpp"

namespace cnstream {

struct RtspSinkContext {
  std::unique_ptr<RtspSinkJoinStream> rtsp_stream_ = nullptr;
};

RtspSinkContext* RtspSink::GetRtspSinkContext(CNFrameInfoPtr data) {
  RtspSinkContext* ctx = nullptr;
  RwLockWriteGuard lg(rtsp_lock_);
  if (is_mosaic_style_) {
    if (data->GetStreamIndex() >= static_cast<uint32_t>(params_.view_cols * params_.view_rows)) {
      LOGI(RTSP) << "================================================================================";
      LOGE(RTSP) << "[RtspSink] Input stream number must no more than " << params_.view_cols * params_.view_rows
                 << " (view window col: " << params_.view_cols << " row: " << params_.view_rows << ")";
      LOGI(RTSP) << "=================================================================================";
      return nullptr;
    }

    auto search = contexts_.find(0);
    if (search != contexts_.end()) {
      ctx = search->second;
    } else {
      ctx = CreateRtspSinkContext(data);
      contexts_[0] = ctx;
    }
  } else {
    uint32_t channel_idx = data->GetStreamIndex();
    auto search = contexts_.find(channel_idx);
    if (search != contexts_.end()) {
      ctx = search->second;
    } else {
      ctx = CreateRtspSinkContext(data);
      contexts_[channel_idx] = ctx;
    }
  }
  return ctx;
}

RtspSinkContext* RtspSink::CreateRtspSinkContext(CNFrameInfoPtr data) {
  RtspSinkContext* context = new RtspSinkContext();
  RtspSinkJoinStream* rtsp_sink = new RtspSinkJoinStream();
  context->rtsp_stream_.reset(rtsp_sink);

  RtspParam rtsp_param = GetRtspParam(data);
  bool ret = context->rtsp_stream_->Open(rtsp_param);

  if (!ret) {
    delete context;
    LOGE(RTSP) << "[RtspSink] Open rtsp stream failed. Invalid parameter";
    return nullptr;
  }
  return context;
}

RtspParam RtspSink::GetRtspParam(CNFrameInfoPtr data) {
  CNDataFramePtr frame = cnstream::GetCNDataFramePtr(data);
  switch (frame->fmt) {
    case CNDataFormat::CN_PIXEL_FORMAT_BGR24:
      params_.color_format = BGR24;
      if (params_.color_mode != "bgr") {
        params_.color_mode = "bgr";
        LOGW(RTSP) << "Color mode should be bgr.";
      }
      break;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
      params_.color_format = NV12;
      break;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
      params_.color_format = NV21;
      break;
    default:
      LOGW(RTSP) << "[CNEncoder] unsuport color format.";
      params_.color_format = BGR24;
      if (params_.color_mode != "bgr") {
        params_.color_mode = "bgr";
        LOGW(RTSP) << "Color mode should be bgr.";
      }
      break;
  }

  RtspParam rtsp_params;
  rtsp_params = params_;
  if (!is_mosaic_style_) {
    rtsp_params.udp_port += data->GetStreamIndex();
  }

  if (rtsp_params.dst_width <= 0) {
    rtsp_params.dst_width = frame->width;
  }
  if (rtsp_params.dst_height <= 0) {
    rtsp_params.dst_height = frame->height;
  }

  rtsp_params.src_width = frame->width;
  rtsp_params.src_height = frame->height;

  return rtsp_params;
}

RtspSink::~RtspSink() { Close(); }

void RtspSink::SetParam(const ModuleParamSet &paramSet, std::string name, int *variable, int default_value) {
  if (paramSet.find(name) == paramSet.end()) {
    *variable = default_value;
  } else {
    *variable = std::stoi(paramSet.at(name));
  }
}

void RtspSink::SetParam(const ModuleParamSet &paramSet, std::string name, std::string *variable,
                        std::string default_value) {
  if (paramSet.find(name) == paramSet.end()) {
    *variable = default_value;
  } else {
    *variable = paramSet.at(name);
  }
}

bool RtspSink::Open(ModuleParamSet paramSet) {
  if (!CheckParamSet(paramSet)) {
    return false;
  }
  SetParam(paramSet, "udp_port", &params_.udp_port, 9554);
  SetParam(paramSet, "http_port", &params_.http_port, 8080);
  SetParam(paramSet, "frame_rate", &params_.frame_rate, 25);

  SetParam(paramSet, "kbit_rate", &params_.kbps, 1000);

  SetParam(paramSet, "gop_size", &params_.gop, 30);

  SetParam(paramSet, "dst_width", &params_.dst_width, 0);
  SetParam(paramSet, "dst_height", &params_.dst_height, 0);

  SetParam(paramSet, "preproc_type", &params_.preproc_type, "cpu");
  SetParam(paramSet, "encoder_type", &params_.encoder_type, "mlu");
  params_.enc_type = params_.encoder_type == "mlu" ? MLU : FFMPEG;
  SetParam(paramSet, "device_id", &params_.device_id, 0);

  SetParam(paramSet, "color_mode", &params_.color_mode, "nv");
  SetParam(paramSet, "view_mode", &params_.view_mode, "single");

  if ("mosaic" == params_.view_mode) {
    params_.preproc_type = "cpu";
    params_.color_mode = "bgr";
    is_mosaic_style_ = true;
    SetParam(paramSet, "view_cols", &params_.view_cols, 4);
    SetParam(paramSet, "view_rows", &params_.view_rows, 4);
  }
  return true;
}

void RtspSink::Close() {
  for (auto& it : contexts_) {
    delete it.second;
  }
  contexts_.clear();
}

int RtspSink::Process(CNFrameInfoPtr data) {
  RtspSinkContext* ctx = GetRtspSinkContext(data);
  if (!ctx) return -1;
  CNDataFramePtr frame = cnstream::GetCNDataFramePtr(data);
  if ("cpu" == params_.preproc_type) {
    if ("bgr" == params_.color_mode || params_.color_format == BGR24) {
      cv::Mat image = *frame->ImageBGR();
      ctx->rtsp_stream_->UpdateBGR(image, data->timestamp, data->GetStreamIndex());
    } else if ("nv" == params_.color_mode) {
      uint8_t *image_data = nullptr;
      image_data = new uint8_t[frame->GetBytes()];
      uint8_t *plane_0 = reinterpret_cast<uint8_t *>(frame->data[0]->GetMutableCpuData());
      uint8_t *plane_1 = reinterpret_cast<uint8_t *>(frame->data[1]->GetMutableCpuData());
      memcpy(image_data, plane_0, frame->GetPlaneBytes(0) * sizeof(uint8_t));
      memcpy(image_data + frame->GetPlaneBytes(0), plane_1, frame->GetPlaneBytes(1) * sizeof(uint8_t));
      frame->deAllocator_.reset();
      ctx->rtsp_stream_->UpdateYUV(image_data, data->timestamp);
      delete[] image_data;
      image_data = nullptr;
    } else {
      LOGE(RTSP) << "color type must be set nv or bgr !!!";
      return -1;
    }
    /*
    } else if ("mlu" == preproc_type_) {
      ctx->UpdateYUVs(data->frame.data[0]->GetMutableMluData(),
                    data->frame.data[1]->GetMutableMluData(), data->frame.timestamp);
      data->frame.deAllocator_.reset();
    */
  }
  return 0;
}

bool RtspSink::CheckParamSet(const ModuleParamSet &paramSet) const {
  bool ret = true;
  ParametersChecker checker;
  for (auto &it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOGW(RTSP) << "[RtspSink] (WARNING) Unknown param: \"" << it.first << "\"";
    }
  }

  std::string err_msg;
  if (!checker.IsNum({"http_port", "udp_port", "frame_rate", "kbit_rate", "gop_size", "view_cols", "view_rows",
                      "device_id", "dst_width", "dst_height"},
                     paramSet, err_msg, true)) {
    LOGE(RTSP) << "[RtspSink] (ERROR) " << err_msg;
    ret = false;
  }

  if (paramSet.find("dst_width") == paramSet.end()) {
    LOGI(RTSP) << "[RtspSink] (INFO) destination *width* is not given. Keep source width.";
  }
  if (paramSet.find("dst_height") == paramSet.end()) {
    LOGI(RTSP) << "[RtspSink] (INFO) destination *height* is not given. Keep source height.";
  }

  if (paramSet.find("encoder_type") != paramSet.end()) {
    if (paramSet.at("encoder_type") != "mlu" && paramSet.at("encoder_type") != "ffmpeg") {
      LOGE(RTSP) << "[RtspSink] (ERROR) Not support encoder type: \"" << paramSet.at("encoder_type")
                 << "\". Choose from \"mlu\", \"ffmpeg\".";
      ret = false;
    }
  }
  if (paramSet.find("preproc_type") != paramSet.end()) {
    if (paramSet.at("preproc_type") != "cpu") {
      LOGE(RTSP) << "[RtspSink] (ERROR) Not support preprocess type: \"" << paramSet.at("preproc_type")
                 << "\". Choose from \"cpu\".";
      ret = false;
    }
  }

  if (paramSet.find("view_mode") != paramSet.end()) {
    if (paramSet.at("view_mode") != "single" && paramSet.at("view_mode") != "mosaic") {
      LOGE(RTSP) << "[RtspSink] (ERROR) Not support view mode: \"" << paramSet.at("view_mode")
                 << "\". Choose from \"single\",\" mosaic\".";
      ret = false;
    }
    if (paramSet.at("view_mode") == "mosaic") {
      if (paramSet.find("color_mode") != paramSet.end() && paramSet.at("color_mode") != "bgr") {
        LOGW(RTSP) << "[RtspSink] (WARNING) view mode is \"mosaic\". Only support plane type \"bgr\"!";
      }
      if (paramSet.find("view_cols") == paramSet.end()) {
        LOGW(RTSP) << "[RtspSink] (WARNING) View *column* number is not given. Default 4.";
      }
      if (paramSet.find("view_rows") == paramSet.end()) {
        LOGW(RTSP) << "[RtspSink] (WARNING) View *row* number is not given. Default 4.";
      }
    }
  }

  if (paramSet.find("color_mode") != paramSet.end()) {
    if (paramSet.at("color_mode") != "nv" && paramSet.at("color_mode") != "bgr") {
      LOGE(RTSP) << "[RtspSink] (ERROR) Not support plane type: \"" << paramSet.at("color_mode")
                 << "\". Choose from \"nv\", \"bgr\".";
      ret = false;
    }
  }
  return ret;
}

RtspSink::RtspSink(const std::string &name) : Module(name) {
  param_register_.SetModuleDesc("RtspSink is a module to deliver stream by RTSP protocol.");
  param_register_.Register("http_port", "Http port.");
  param_register_.Register("udp_port", "UDP port.");
  param_register_.Register("preproc_type", "Resize and colorspace convert type, e.g., cpu.");
  param_register_.Register("encoder_type", "Encode type. It should be 'mlu' or 'ffmpeg'");
  param_register_.Register("dst_width", "The image width of the output.");
  param_register_.Register("dst_height", "The image height of the output.");
  param_register_.Register("color_mode", "Input picture color mode, include nv and bgr.");
  param_register_.Register("view_mode", "Use set rtsp view mode, inlcude single and mosaic mode.");
  param_register_.Register("view_cols", "Divide the screen horizontally, set only for mosaic mode.");
  param_register_.Register("view_rows", "Divide the screen vertically, set only for mosaic mode.");
  param_register_.Register("device_id", "Which device will be used. If there is only one device, it might be 0.");
  param_register_.Register("frame_rate", "Frame rate of the encoded video.");
  param_register_.Register("kbit_rate",
                           "The amount data encoded for a unit of time."
                           "A higher bitrate means a higher quality video.");
  param_register_.Register("gop_size",
                           "Group of pictures is known as GOP."
                           "gop_size is the number of frames between two I-frames.");
  hasTransmit_.store(0);  // for receive eos
}

}  // namespace cnstream
