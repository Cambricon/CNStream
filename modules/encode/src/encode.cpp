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
#include "encode.hpp"

#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "video/video_sink/video_sink.hpp"
#include "video/video_stream/video_stream.hpp"

namespace cnstream {

struct EncoderContext {
  std::unique_ptr<VideoStream> stream = nullptr;
  std::unique_ptr<VideoSink> sink = nullptr;
  std::unique_ptr<uint8_t[]> buffer = nullptr;
  int buffer_size = 0;
  std::ofstream file;
  int64_t frame_count = 0;
};

EncoderContext *Encode::GetContext(CNFrameInfoPtr data) {
  auto params = param_helper_->GetParams();
  std::lock_guard<std::mutex> lk(ctx_lock_);
  EncoderContext *ctx = nullptr;
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
        LOGE(Encode) << "GetContext() input video stream count over " <<
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


EncoderContext *Encode::CreateContext(CNFrameInfoPtr data, const std::string &stream_id) {
  if (data->IsEos()) {
    LOGI(Encode) << "CreateContext() the data is an EOS frame";
    return nullptr;
  }
  CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
  bool with_container = false;
  VideoCodecType codec_type = VideoCodecType::H264;
  auto params = param_helper_->GetParams();
  auto dot = params.file_name.find_last_of(".");
  if (dot == std::string::npos) {
    LOGE(Encode) << "CreateContext() unknown file type \"" << params.file_name << "\"";
    return nullptr;
  }
  std::string file_name = params.file_name;
  std::transform(file_name.begin(), file_name.end(), file_name.begin(), ::tolower);
  if (file_name.find("hevc") != std::string::npos || file_name.find("h265") != std::string::npos) {
    codec_type = VideoCodecType::H265;
  }
  std::string ext_name = file_name.substr(dot + 1);
  file_name = file_name.substr(0, dot);
  if (ext_name == "mp4" || ext_name == "mkv") {
    with_container = true;
  } else if (ext_name == "jpg" || ext_name == "jpeg") {
    codec_type = VideoCodecType::JPEG;
  }

  EncoderContext *ctx = new EncoderContext;
  VideoStream::Param sparam;
  sparam.width = params.dst_width > 0 ? params.dst_width : frame->width;
  sparam.height = params.dst_height > 0 ? params.dst_height : frame->height;
  sparam.tile_cols = params.tile_cols;
  sparam.tile_rows = params.tile_rows;
  sparam.resample = params.resample;
  sparam.frame_rate = params.frame_rate;
  sparam.time_base = 90000;
  sparam.bit_rate = params.bit_rate;
  sparam.gop_size = params.gop_size;
  VideoPixelFormat pixel_format =
      frame->fmt == CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12 ? VideoPixelFormat::NV12 : VideoPixelFormat::NV21;
  sparam.pixel_format = (params.mlu_encoder) ? pixel_format : VideoPixelFormat::I420;
  sparam.codec_type = codec_type;
  sparam.mlu_encoder = params.mlu_encoder;
  sparam.device_id = params.device_id;
  ctx->stream.reset(new VideoStream(sparam));
  if (!ctx->stream) {
    LOGE(Encode) << "CreateContext() create video stream failed";
    delete ctx;
    return nullptr;
  }

  if (with_container) {
    VideoSink::Param kparam;
    kparam.width = sparam.width;
    kparam.height = sparam.height;
    kparam.frame_rate = sparam.frame_rate;
    kparam.time_base = sparam.time_base;
    kparam.bit_rate = sparam.bit_rate;
    kparam.gop_size = sparam.gop_size;
    kparam.pixel_format = VideoPixelFormat::I420;
    kparam.codec_type = sparam.codec_type;
    kparam.file_name = params.file_name.substr(0, dot) + "_" + stream_id + "." + ext_name;
    ctx->sink.reset(new VideoSink(kparam));
    if (!ctx->sink) {
      LOGE(Encode) << "CreateContext() create video sink failed";
      delete ctx;
      return nullptr;
    }
    if (VideoSink::SUCCESS != ctx->sink->Start()) {
      LOGE(Encode) << "CreateContext() start video sink failed";
      return nullptr;
    }
  }

  auto event_callback = [=](EncoderContext *ctx, VideoStream::Event event) {
    if (!ctx || !ctx->stream || (with_container && !ctx->sink)) return;
    if (event == VideoStream::Event::EVENT_DATA) {
      VideoPacket packet;
      memset(&packet, 0, sizeof(VideoPacket));
      int ret = ctx->stream->GetPacket(&packet);
      if (ret < 0) {
        LOGE(Encode) << "EventCallback() stream get packet size failed, ret=" << ret;
        return;
      }
      if (ctx->buffer_size < ret) {
        ctx->buffer.reset(new uint8_t[ret]);
        ctx->buffer_size = ret;
      }
      packet.data = ctx->buffer.get();
      packet.size = ctx->buffer_size;
      ret = ctx->stream->GetPacket(&packet);
      if (ret <= 0) {
        LOGE(Encode) << "EventCallback() stream get packet failed, ret=" << ret;
        return;
      }
      if (with_container) {
        ret = ctx->sink->Write(&packet);
        if (ret != VideoSink::SUCCESS) {
          LOGE(Encode) << "EventCallback() sink write failed, ret=" << ret;
          return;
        }
      } else if (codec_type == VideoCodecType::JPEG) {
        auto jpeg_file_name = file_name + "_" + stream_id + "_" + std::to_string(ctx->frame_count++) + "." + ext_name;
        std::ofstream file(jpeg_file_name);
        if (!file.good()) {
          LOGE(Encode) << "EventCallback() open " << jpeg_file_name << " failed";
        } else {
          file.write(reinterpret_cast<char *>(packet.data), packet.size);
        }
        file.close();
      } else {
        if (!ctx->file.is_open()) {
          auto video_file_name = file_name + "_" + stream_id + "." + ext_name;
          ctx->file.open(video_file_name);
          if (!ctx->file.good()) {
            LOGE(Encode) << "EventCallback() open " << video_file_name << " failed";
            return;
          }
        }
        if (ctx->file.good()) ctx->file.write(reinterpret_cast<char *>(packet.data), packet.size);
      }
    } else if (event == VideoStream::Event::EVENT_EOS) {
      LOGI(Encode) << "EventCallback() EVENT_EOS";
    } else if (event == VideoStream::Event::EVENT_ERROR) {
      LOGE(Encode) << "EventCallback() EVENT_ERROR";
      PostEvent(EventType::EVENT_ERROR, "encode receives error event");
    }
  };

  if (!ctx->stream->Open()) {
    LOGE(Encode) << "CreateContext() open video stream failed. stream_id [" << stream_id << "]";
    if (ctx->sink) ctx->sink->Stop();
    delete ctx;
    return nullptr;
  }
  ctx->stream->SetEventCallback(std::bind(event_callback, ctx, std::placeholders::_1));

  contexts_[stream_id] = ctx;
  return ctx;
}

Encode::Encode(const std::string &name) : Module(name) {
  param_register_.SetModuleDesc("Encode is a module to encode videos or images.");
  param_helper_.reset(new (std::nothrow) ModuleParamsHelper<EncodeParam>(name));
  auto input_encoder_type_parser = [](const ModuleParamSet &param_set, const std::string &param_name,
                                      const std::string &value, void *result) -> bool {
    if (value == "cpu") {
      *static_cast<bool *>(result) = false;
    } else if (value == "mlu") {
      *static_cast<bool *>(result) = true;
    } else {
      LOGE(Encode) << "[ModuleParamParser] [" << param_name << "]: " << value << " failed"
                    << "\". Choose from \"mlu\", \"cpu\".";
      return false;
    }
    return true;
  };

  static const std::vector<ModuleParamDesc> regist_param = {
      {"device_id", "0", "Which MLU device will be used.", PARAM_OPTIONAL, OFFSET(EncodeParam, device_id),
       ModuleParamParser<int>::Parser, "int"},
      {"input_frame", "cpu", "Selection for the input frame. It should be 'mlu' or 'cpu." , PARAM_OPTIONAL,
       OFFSET(EncodeParam, mlu_input_frame), input_encoder_type_parser, "bool"},
      {"encoder_type", "cpu", "Selection for encoder type. It should be 'mlu' or 'cpu.", PARAM_OPTIONAL,
       OFFSET(EncodeParam, mlu_encoder), input_encoder_type_parser, "bool"},
      {"dst_width", "0", "Output video width. 0 means dst width is same with source", PARAM_OPTIONAL,
       OFFSET(EncodeParam, dst_width), ModuleParamParser<int>::Parser, "int"},
      {"dst_height", "0", "Output video height. 0 means dst height is same with source", PARAM_OPTIONAL,
       OFFSET(EncodeParam, dst_height), ModuleParamParser<int>::Parser, "int"},
      {"frame_rate", "30", "Frame rate of video encoding. Higher value means more fluent.", PARAM_OPTIONAL,
       OFFSET(EncodeParam, frame_rate), ModuleParamParser<double>::Parser, "double"},
      {"bit_rate", "4000000", "Bit rate of video encoding. Higher value means better video quality.", PARAM_OPTIONAL,
       OFFSET(EncodeParam, bit_rate), ModuleParamParser<int>::Parser, "int"},
      {"gop_size", "10", "Group of pictures. gop_size is the number of frames between two IDR frames.", PARAM_OPTIONAL,
       OFFSET(EncodeParam, gop_size), ModuleParamParser<int>::Parser, "int"},
      {"view_cols", "1", "Grids in horizontally of video tiling, only support cpu input.", PARAM_OPTIONAL,
       OFFSET(EncodeParam, tile_cols), ModuleParamParser<int>::Parser, "int"},
      {"view_rows", "1", "Grids in vertically of video tiling, only support cpu input.", PARAM_OPTIONAL,
       OFFSET(EncodeParam, tile_rows), ModuleParamParser<int>::Parser, "int"},
      {"resample", "false", "Resample frame with canvas, only support cpu input.", PARAM_OPTIONAL,
       OFFSET(EncodeParam, resample), ModuleParamParser<bool>::Parser, "bool"},
      {"file_name", "output/output.mp4",
       "File name and path to store, the final name will be added with stream id or frame count", PARAM_OPTIONAL,
       OFFSET(EncodeParam, file_name), ModuleParamParser<std::string>::Parser, "string"},
      {"codec_type", "", "Replaced by file_name's extension name.", PARAM_DEPRECATED},
      {"output_dir", "", "Replaced by file_name's path.", PARAM_DEPRECATED},
      {"use_ffmpeg", "", "Always is FFMpeg if doing CPU encoding.", PARAM_DEPRECATED},
      {"kbit_rate", "", "Replaced by bit_rate", PARAM_DEPRECATED},
      {"preproc_type", "", "selected automatically.", PARAM_DEPRECATED}};

  param_helper_->Register(regist_param, &param_register_);
}

Encode::~Encode() {
  Close();
}

bool Encode::Open(ModuleParamSet paramSet) {
  if (!param_helper_->ParseParams(paramSet)) {
    LOGE(Encode) << "[" << GetName() << "] parse parameters failed.";
    return false;
  }
  auto params = param_helper_->GetParams();
  if (params.mlu_encoder && params.device_id < 0) {
    LOGE(Encode) << "Open() device_id is required to be greater than 0, if mlu encoding is used";
    return false;
  }
  if (params.mlu_input_frame && (params.tile_cols > 1 || params.tile_rows > 1)) {
    LOGE(Encode) << "Open() mlu input tiling is not supported";
    return false;
  }
  return true;
}

void Encode::Close() {
  if (contexts_.empty()) {
    return;
  }
  for (auto &it : contexts_) {
    EncoderContext *ctx = it.second;
    if (ctx) {
      if (ctx->stream) ctx->stream->Close();
      if (ctx->sink) ctx->sink->Stop();
      ctx->buffer_size = 0;
      ctx->file.close();
      delete ctx;
    }
  }
  contexts_.clear();
}

int Encode::Process(CNFrameInfoPtr data) {
  if (nullptr == data) {
    return -1;
  }
  if (data->IsRemoved()) {
    return 0;
  }

  EncoderContext *ctx = GetContext(data);
  if (!ctx) {
    LOGE(Encode) << "Get Encoder Context Failed.";
    return -1;
  }

  auto params = param_helper_->GetParams();
  CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);

  if (!params.mlu_input_frame) {
    if (!ctx->stream->Update(frame->ImageBGR(), VideoStream::ColorFormat::BGR, data->timestamp, data->stream_id)) {
      LOGE(Encode) << "Process() video stream update failed.";
    }
  } else {
#ifndef HAVE_CNCV
    if (params.mlu_input_frame && params.mlu_encoder) {
      LOGE(Encode) << "Process() Encode mlu input frame on mlu is not supported. Please install CNCV.";
      return -1;
    }
#endif
    if (frame->dst_device_id != params.device_id) {
      LOGE(Encode) << "Process() Encode mlu input frame on different device is not supported";
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
      LOGE(Encode) << "Process() video stream update failed";
    }
  }

  return 0;
}

void Encode::OnEos(const std::string &stream_id) {
  auto params = param_helper_->GetParams();
  std::lock_guard<std::mutex> lk(ctx_lock_);
  if (params.tile_cols > 1 || params.tile_rows > 1) {
    if (contexts_.empty()) return;
    EncoderContext *ctx = contexts_.begin()->second;
    // clear last grid
    if (ctx->stream) ctx->stream->Clear(stream_id);
    tile_streams_.erase(stream_id);
    if (tile_streams_.empty()) {
      LOGI(Encode) << "OnEos() all streams stopped";
      EncoderContext *ctx = contexts_.begin()->second;
      if (ctx) {
        if (ctx->stream) ctx->stream->Close();
        if (ctx->sink) ctx->sink->Stop();
        ctx->buffer_size = 0;
        ctx->file.close();
        delete ctx;
      }
      contexts_.clear();
    }
  } else {
    auto search = contexts_.find(stream_id);
    if (search != contexts_.end()) {
      EncoderContext *ctx = search->second;
      if (ctx) {
        if (ctx->stream) ctx->stream->Close(!IsStreamRemoved(stream_id));
        if (ctx->sink) ctx->sink->Stop();
        ctx->buffer_size = 0;
        ctx->file.close();
        delete ctx;
      }
      contexts_.erase(stream_id);
    }
  }
}

}  // namespace cnstream
