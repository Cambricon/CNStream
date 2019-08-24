/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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

#include <algorithm>
#include <atomic>
#include <cassert>
#include <functional>

#include "cninfer/mlu_context.h"
#include "cnstream_frame.hpp"
#include "cnstream_timer.hpp"
#include "decoder.hpp"
#include "glog/logging.h"

namespace cnstream {

static CNDataFormat CnPixelFormat2CnDataFormat(libstream::CnPixelFormat pformat) {
  switch (pformat) {
    case libstream::YUV420SP_NV12:
      return CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12;
    case libstream::YUV420SP_NV21:
      return CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
    case libstream::RGB24:
      return CNDataFormat::CN_PIXEL_FORMAT_RGB24;
    case libstream::BGR24:
      return CNDataFormat::CN_PIXEL_FORMAT_BGR24;
    default:
      return CNDataFormat::CN_INVALID;
  }
  return CNDataFormat::CN_INVALID;
}

struct DecodeHandler {
  DecoderAttribute attribute;
  uint32_t chn_idx;
  DevContext dev_ctx;
  std::function<void(std::shared_ptr<CNFrameInfo> data)> post_function = NULL;
  std::shared_ptr<libstream::CnDecode> instance;
  uint64_t frame_id = 0;

  CNTimer fps_calculators[4];

  void PrintPerformanceInfomation() const {
    printf("channel index: %u:\n", chn_idx);
    fps_calculators[0].PrintFps("transfer memory: ");
    fps_calculators[1].PrintFps("decode delay: ");
    fps_calculators[2].PrintFps("send data to codec: ");
    fps_calculators[3].PrintFps("output :");
  }

  void FrameCallback(const libstream::CnFrame& frame) {
    if (post_function) {
      auto data = std::make_shared<CNFrameInfo>();

      if (DevContext::CPU == dev_ctx.dev_type) {
        LOG(FATAL) << "Unsupported!!!";
      } else {
        void* frame_data[CN_MAX_PLANES];
        assert(frame.planes <= CN_MAX_PLANES);
        for (uint32_t pi = 0; pi < frame.planes; ++pi) {
          frame_data[pi] = reinterpret_cast<void*>(frame.data.ptrs[pi]);
        }
        data->frame.CopyFrameFromMLU(dev_ctx.dev_id, dev_ctx.ddr_channel, CnPixelFormat2CnDataFormat(frame.pformat),
                                     frame.width, frame.height, frame_data, frame.strides);

        // frame position
        data->channel_idx = chn_idx;
        data->frame.frame_id = frame_id++;
        data->frame.timestamp = frame.pts;
      }

      post_function(data);
    }

    instance->ReleaseBuffer(frame.buf_id);
  }

  void PerfCallback(const libstream::CnDecodePerfInfo& info) {
    fps_calculators[0].Dot(1.0f * info.transfer_us / 1000, 1);
    fps_calculators[1].Dot(1.0f * info.decode_us / 1000, 1);
    fps_calculators[2].Dot(1.0f * info.total_us / 1000, 1);
    fps_calculators[3].Dot(1);
  }

  void EOSCallback() {
    if (post_function) {
      auto data = std::make_shared<CNFrameInfo>();
      data->channel_idx = chn_idx;
      data->frame.flags |= CNFrameFlag::CN_FRAME_FLAG_EOS;
      LOG(INFO) << "[Decoder] Channel " << chn_idx << " receive eos.";
      post_function(data);
    }
  }
};

bool Decoder::IsSupported(libstream::CnCodecType type) {
#ifdef CNS_MLU100
  switch (type) {
    case libstream::H264:
    case libstream::H265:
    case libstream::JPEG:
    case libstream::MPEG4:
    case libstream::MJPEG:
      return true;
    default:
      return false;
  }
#endif

#ifdef CNS_MLU270
  switch (type) {
    case libstream::H264:
    case libstream::H265:
    case libstream::JPEG:
    case libstream::MPEG4:
    case libstream::MJPEG:
      return true;
    default:
      return false;
  }
#endif

  return true;
}

bool Decoder::IsSupported(libstream::CnPixelFormat format) {
#ifdef CNS_MLU100
  switch (format) {
    case libstream::YUV420SP_NV21:
    case libstream::RGB24:
    case libstream::BGR24:
      return true;
    default:
      return false;
  }
#endif

#ifdef CNS_MLU270
  switch (format) {
    case libstream::YUV420SP_NV21:
    case libstream::YUV420SP_NV12:
      return true;
    default:
      return false;
  }
#endif

  return true;
}

bool Decoder::IsSupported(DecoderExtraAbility ability) {
#ifdef CNS_MLU100
  return true;
#endif
#ifdef CNS_MLU270
  return false;
#endif
}

Decoder::Decoder(const std::string& name) : Module(name) {}

Decoder::~Decoder() {
  for (auto& it : handlers_) {
    delete it.second;
  }
}

bool Decoder::Open(ModuleParamSet paramSet) {
  (void)paramSet;
  return true;
}
void Decoder::Close() {}
int Decoder::Process(std::shared_ptr<CNFrameInfo> data) { return 0; }

int Decoder::OpenDecodeChannel(const DecoderAttribute& attr) {
  DecodeHandler* handler = new DecodeHandler;
  handler->attribute = attr;

  // get a channel index
  uint32_t new_chn_idx = 0;
  if (closed_channels_.size() > 0) {
    new_chn_idx = closed_channels_[0];
    closed_channels_.erase(closed_channels_.begin());
  } else {
    new_chn_idx = ++max_channel_idx_;
  }
  handler->chn_idx = new_chn_idx;

  // bind post data callback
  if (container_) handler->post_function = std::bind(&Pipeline::ProvideData, container_, this, std::placeholders::_1);

  // set output device context
  if (attr.output_on_cpu) {
    handler->dev_ctx.dev_type = DevContext::CPU;
  } else {
    handler->dev_ctx.dev_type = DevContext::MLU;
    handler->dev_ctx.dev_id = attr.dev_id;
    handler->dev_ctx.ddr_channel = handler->chn_idx % 4;
  }

  libstream::CnDecode::Attr instance_attr;
  memset(&instance_attr, 0, sizeof(instance_attr));
  // common attrs
  instance_attr.maximum_geometry.w = attr.max_video_w;
  instance_attr.maximum_geometry.h = attr.max_video_h;
  instance_attr.codec_type = attr.codec_type;
  instance_attr.pixel_format = attr.pixel_format;
  instance_attr.output_geometry.w = attr.output_frame_w;
  instance_attr.output_geometry.h = attr.output_frame_h;
  instance_attr.drop_rate = attr.drop_rate;
  instance_attr.frame_buffer_num = attr.frame_buffer_num;
  instance_attr.dev_id = attr.dev_id;
  instance_attr.video_mode = attr.video_mode;

  instance_attr.silent = false;

  // callbacks
  instance_attr.frame_callback = std::bind(&DecodeHandler::FrameCallback, handler, std::placeholders::_1);
  instance_attr.perf_callback = std::bind(&DecodeHandler::PerfCallback, handler, std::placeholders::_1);
  instance_attr.eos_callback = std::bind(&DecodeHandler::EOSCallback, handler);

  // create CnDecode
  try {
    libstream::MluContext mlu_ctx;
    mlu_ctx.set_dev_id(attr.dev_id);
    mlu_ctx.set_channel_id(handler->chn_idx % 4);
    mlu_ctx.ConfigureForThisThread();
    handler->instance = std::shared_ptr<libstream::CnDecode>(libstream::CnDecode::Create(instance_attr));
  } catch (libstream::StreamlibsError& e) {
    LOG(ERROR) << "[Decoder] " << e.what();
    delete handler;
    return -1;
  }

  assert(handlers_.find(handler->chn_idx) == handlers_.end());
  handlers_.insert(std::make_pair(handler->chn_idx, handler));

  LOG(INFO) << "MLU Codec channel opened";
  LOG(INFO) << "Running...";

  return handler->chn_idx;
}

bool Decoder::CloseDecodeChannel(uint32_t channel_idx, bool print_perf_info) {
  auto pos = handlers_.find(channel_idx);
  if (handlers_.end() == pos) {
    LOG(WARNING) << "[Decoder] Decode channel: " << channel_idx << " not opened.";
    return false;
  }

  uint32_t closed_channel_idx = pos->first;
  closed_channels_.push_back(closed_channel_idx);
  std::sort(closed_channels_.begin(), closed_channels_.end());

  DecodeHandler* handler = pos->second;

  if (print_perf_info) handler->PrintPerformanceInfomation();

  delete handler;
  handlers_.erase(pos);

  return true;
}

bool Decoder::SendPacket(uint32_t channel_idx, const libstream::CnPacket& packet, bool eos) {
  auto iter = handlers_.find(channel_idx);
  if (handlers_.end() == iter) {
    LOG(WARNING) << "[Decoder] Decode channel: " << channel_idx << " not opened.";
    return false;
  }

  LOG_IF(INFO, eos) << "[Decoder] Channel " << channel_idx << " send eos.";

  std::shared_ptr<libstream::CnDecode> instance = iter->second->instance;
  try {
    if (instance->SendData(packet, eos)) return true;
  } catch (libstream::StreamlibsError& e) {
    LOG(ERROR) << "[Decoder] " << e.what();
    return false;
  }

  return false;
}

}  // namespace cnstream
