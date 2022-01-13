/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <map>
#include <vector>

#ifdef ENABLE_MLU300_CODEC

#include "cnrt.h"
#include "cncodec_v3_common.h"
#include "cncodec_v3_enc.h"

#include "cnstream_logging.hpp"

#include "video_encoder_mlu300.hpp"

namespace cnstream {

namespace video {

static const char *pf_str[] = {"I420", "NV12", "NV21", "BGR", "RGB"};
static const char *ct_str[] = {"H264", "H265", "MPEG4", "JPEG"};

#define CNCODEC_PTS_MAX_VALUE (0xffffffffffffffffLL / 1000)

#define ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))

struct EncodingInfo {
  int64_t pts, dts;
  int64_t start_tick, end_tick;
  void *user_data;
};

struct VideoEncoderMlu300Private {
  cncodecEncParam_t cn_param;
  cncodecHandle_t cn_encoder;
  std::mutex list_mtx;
  std::list<cncodecFrame_t> input_list;
  std::condition_variable list_cv;
  std::mutex info_mtx;
  std::map<int64_t, EncodingInfo> encoding_info;
  std::mutex eos_mtx;
  std::condition_variable eos_cv;
  std::atomic<bool> eos_sent{false};
  std::atomic<bool> eos_got{false};
  std::atomic<bool> error{false};
  std::unique_ptr<uint8_t[]> stream_buffer = nullptr;
  uint32_t stream_buffer_size = 0;
  std::unique_ptr<uint8_t[]> ps_buffer = nullptr;
  uint32_t ps_size = 0;
  int64_t frame_count = 0;
  int64_t packet_count = 0;
  int64_t data_index = 0;
};

#define THREAD_NUMBER_PER_DEVICE 8  // 2(video encoder) + 6(jpeg encoder)

struct EventData {
  int event;
  cncodecStream_t data;
  VideoEncoderMlu300 *encoder;
  int64_t index;
};

struct InstanceContext {
  int64_t enqueue_index;
  int64_t process_index;
};

struct DeviceContext {
  std::mutex mutex;
  std::condition_variable queue_cv;
  std::queue<EventData> queue;
  std::condition_variable index_cv;
  std::vector<std::thread> threads;
  std::map<VideoEncoderMlu300 *, InstanceContext> instances;
};

static std::mutex g_device_mutex;
static std::map<int, DeviceContext> g_device_contexts;

static inline int64_t CurrentTick() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

static i32_t EncoderEventCallback(cncodecEventType_t event_type, void *user_context, void *data) {
  VideoEncoderMlu300 *encoder = reinterpret_cast<VideoEncoderMlu300 *>(user_context);
  return encoder->EventHandlerCallback(event_type, data);
}

VideoEncoderMlu300::VideoEncoderMlu300(const VideoEncoder::Param &param) : VideoEncoderBase(param) {
  LOGI(VideoEncoderMlu) << "VideoEncoderMlu300(" << param.width << "x" << param.height << ", "
                        << pf_str[param_.pixel_format] << ", " << ct_str[param_.codec_type] << ")";
  priv_.reset(new (std::nothrow) VideoEncoderMlu300Private);
}

VideoEncoderMlu300::~VideoEncoderMlu300() {
  Stop();
  priv_.reset();
}

int VideoEncoderMlu300::Start() {
  UniqueWriteLock slk(state_mtx_);
  if (state_ != IDLE) {
    LOGW(VideoEncoderMlu) << "Start() state != IDLE";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  state_ = STARTING;

  if (param_.mlu_device_id < 0) {
    LOGE(VideoEncoderMlu) << "Start() mlu device id must >= 0";
    state_ = IDLE;
    return cnstream::VideoEncoder::ERROR_PARAMETERS;
  }
  if (param_.input_buffer_count < 3) {
    LOGW(VideoEncoderMlu) << "Start() input buffer count must no fewer than 3";
    param_.input_buffer_count = 3;
  }

  param_.width = param_.width % 2 ? param_.width - 1 : param_.width;
  param_.height = param_.height % 2 ? param_.height - 1 : param_.height;
  param_.frame_rate = param_.frame_rate > 0 ? param_.frame_rate : 30;
  param_.frame_rate = param_.frame_rate < 120 ? param_.frame_rate : 120;
  param_.time_base = param_.time_base > 0 ? param_.time_base : 1000;

  memset(&priv_->cn_param, 0, sizeof(priv_->cn_param));
  priv_->cn_param.device_id = param_.mlu_device_id;
  priv_->cn_param.run_mode = CNCODEC_RUN_MODE_ASYNC;
  if (param_.codec_type == VideoCodecType::H264) {
    priv_->cn_param.coding_attr.codec_attr.codec = CNCODEC_H264;
  } else if (param_.codec_type == VideoCodecType::H265) {
    priv_->cn_param.coding_attr.codec_attr.codec = CNCODEC_HEVC;
  } else if (param_.codec_type == VideoCodecType::JPEG) {
    priv_->cn_param.coding_attr.codec_attr.codec = CNCODEC_JPEG;
  } else {
    LOGE(VideoEncoderMlu) << "Start() unsupported codec type: " << ct_str[param_.codec_type];
    state_ = IDLE;
    return cnstream::VideoEncoder::ERROR_PARAMETERS;
  }
  if (param_.pixel_format == VideoPixelFormat::NV12) {
    priv_->cn_param.pixel_format = CNCODEC_PIX_FMT_NV12;
  } else if (param_.pixel_format == VideoPixelFormat::NV21) {
    priv_->cn_param.pixel_format = CNCODEC_PIX_FMT_NV21;
  } else if (param_.pixel_format == VideoPixelFormat::I420) {
    priv_->cn_param.pixel_format = CNCODEC_PIX_FMT_I420;
  } else {
    LOGE(VideoEncoderMlu) << "Start() unsupported pixel format: " << pf_str[param_.pixel_format];
    state_ = IDLE;
    return cnstream::VideoEncoder::ERROR_PARAMETERS;
  }
  priv_->cn_param.color_space = CNCODEC_COLOR_SPACE_BT_709;
  priv_->cn_param.pic_width = param_.width;
  priv_->cn_param.pic_height = param_.height;
  priv_->cn_param.max_width = param_.width;
  priv_->cn_param.max_height = param_.height;
  priv_->cn_param.frame_rate_num = param_.frame_rate;
  priv_->cn_param.frame_rate_den = 1;
  priv_->cn_param.input_stride_align = 64;
  priv_->cn_param.input_buf_num = param_.input_buffer_count;
  priv_->cn_param.input_buf_source = CNCODEC_BUF_SOURCE_LIB;
  priv_->cn_param.user_context = reinterpret_cast<void *>(this);

  priv_->cn_param.coding_attr.gop_size = param_.gop_size;
  priv_->cn_param.coding_attr.stream_type = CNCODEC_ENC_BYTE_STREAM;

  priv_->cn_param.coding_attr.rc_attr.rc_mode = CNCODEC_ENC_RATE_CTRL_VBR;
  priv_->cn_param.coding_attr.rc_attr.initial_qp = -1;
  priv_->cn_param.coding_attr.rc_attr.target_bitrate = param_.bit_rate;
  priv_->cn_param.coding_attr.rc_attr.rc_windows = 100;

  if (param_.codec_type == VideoCodecType::H264) {
    priv_->cn_param.coding_attr.profile = CNCODEC_ENC_PROFILE_H264_HIGH;
    priv_->cn_param.coding_attr.level = CNCODEC_ENC_LEVEL_H264_51;
    priv_->cn_param.coding_attr.frame_interval_p = 2;
    priv_->cn_param.coding_attr.codec_attr.h264_attr.enable_repeat_sps_pps = 1;
    priv_->cn_param.coding_attr.codec_attr.h264_attr.idr_period = param_.gop_size;
    priv_->cn_param.coding_attr.codec_attr.h264_attr.entropy_mode = CNCODEC_ENC_ENTROPY_MODE_CABAC;
  } else if (param_.codec_type == VideoCodecType::H265) {
    priv_->cn_param.coding_attr.profile = CNCODEC_ENC_PROFILE_HEVC_MAIN;
    priv_->cn_param.coding_attr.level = CNCODEC_ENC_LEVEL_HEVC_51;
    priv_->cn_param.coding_attr.frame_interval_p = 3;
    priv_->cn_param.coding_attr.codec_attr.hevc_attr.enable_repeat_sps_pps = 1;
    priv_->cn_param.coding_attr.codec_attr.hevc_attr.idr_period = param_.gop_size;
    priv_->cn_param.coding_attr.codec_attr.hevc_attr.tier = CNCODEC_ENC_TIER_HEVC_HIGHT;
  }

  std::unique_lock<std::mutex> dlk(g_device_mutex);
  i32_t ret = cncodecEncCreate(&priv_->cn_encoder, EncoderEventCallback, &priv_->cn_param);
  if (CNCODEC_SUCCESS != ret) {
    LOGE(VideoEncoderMlu) << "Start() cncodecEncCreate failed, ret=" << ret;
    state_ = IDLE;
    return cnstream::VideoEncoder::ERROR_FAILED;
  }

  auto event_handler_loop = [](int device_id) {
    std::unique_lock<std::mutex> dlk(g_device_mutex);
    if (g_device_contexts.count(device_id) == 0) {
      LOGE(VideoEncoderMlu) << "EventHandlerLoop() context is not exist for device " << device_id;
      return;
    }
    DeviceContext &ctx = g_device_contexts[device_id];
    dlk.unlock();

    while (1) {
      std::unique_lock<std::mutex> lk(ctx.mutex);
      ctx.queue_cv.wait(lk, [&ctx]() { return (ctx.instances.size() < ctx.threads.size() || !ctx.queue.empty()); });
      if (ctx.instances.size() == 0) {
        lk.unlock(); dlk.lock(); lk.lock();
        LOGI(VideoEncoderMlu) << "EventHandlerLoop() destory context for device " << device_id << " now!";
        while (!ctx.queue.empty()) ctx.queue.pop();
        for (auto &thread : ctx.threads) thread.detach();
        g_device_contexts.erase(device_id);
        break;
      } else if (ctx.instances.size() < ctx.threads.size()) {
        LOGT(VideoEncoderMlu) << "EventHandlerLoop() reduce event handler thread number to " << ctx.instances.size()
                              << " for device " << device_id;
        auto thread = std::find_if(ctx.threads.begin(), ctx.threads.end(),
                                   [](const std::thread &t) { return t.get_id() == std::this_thread::get_id(); });
        if (thread != ctx.threads.end()) {
          thread->detach();
          ctx.threads.erase(thread);
          break;
        }
      }

      if (ctx.queue.empty()) continue;

      EventData event_data = ctx.queue.front();
      ctx.queue.pop();
      if (!event_data.encoder) {
        LOGW(VideoEncoderMlu) << "EventHandlerLoop() instance is invalid";
        continue;
      }
      if (ctx.instances.count(event_data.encoder) == 0) {
        LOGW(VideoEncoderMlu) << "EventHandlerLoop() instance " << event_data.encoder << " is not exist";
        continue;
      }
      InstanceContext &ictx = ctx.instances[event_data.encoder];
      ctx.index_cv.wait(lk, [&]() { return (event_data.index == ictx.process_index); });
      lk.unlock();

      event_data.encoder->EventHandler(event_data.event, &(event_data.data));

      lk.lock();
      ictx.process_index++;
      lk.unlock();
      ctx.index_cv.notify_all();
    }
  };

  DeviceContext &ctx = g_device_contexts[param_.mlu_device_id];
  std::unique_lock<std::mutex> clk(ctx.mutex);
  if (ctx.instances.count(this) == 0) {
    InstanceContext ictx;
    ictx.enqueue_index = ictx.process_index = 0;
    ctx.instances.emplace(this, ictx);
  }
  if (ctx.instances.size() <= THREAD_NUMBER_PER_DEVICE && ctx.instances.size() > ctx.threads.size()) {
    ctx.threads.emplace_back(event_handler_loop, param_.mlu_device_id);
    LOGT(VideoEncoderMlu) << "Start() increase event handler thread number to " << ctx.instances.size()
                          << " for device " << param_.mlu_device_id;
  }

  state_ = RUNNING;
  return cnstream::VideoEncoder::SUCCESS;
}

int VideoEncoderMlu300::Stop() {
  UniqueWriteLock slk(state_mtx_);
  if (state_ != RUNNING) {
    // LOGW(VideoEncoderMlu) << "Stop() state != RUNNING";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  state_ = STOPPING;
  slk.Unlock();

  std::unique_lock<std::mutex> lk(priv_->list_mtx);
  if (!priv_->input_list.empty()) {
    LOGW(VideoEncoderMlu) << "Stop() " << priv_->input_list.size() << " frame buffers still outside";
    priv_->list_cv.wait(lk, [this]() { return priv_->input_list.empty(); });
  }
  lk.unlock();

  if (!priv_->error) {
    // wait eos
    if (priv_->eos_sent && !priv_->eos_got) {
      LOGI(VideoEncoderMlu) << "Stop() waiting EOS";
      std::unique_lock<std::mutex> eos_lk(priv_->eos_mtx);
      if (false == priv_->eos_cv.wait_for(eos_lk, std::chrono::seconds(10),
                                          [this]() { return priv_->eos_got.load(); })) {
        LOGE(VideoEncoderMlu) << "Stop() wait EOS for 10s timeout";
      }
    }
  }

  // destroy cn encoder
  i32_t ret = cncodecEncDestroy(priv_->cn_encoder);
  if (CNCODEC_SUCCESS != ret) {
    LOGE(VideoEncoderMlu) << "Stop() cncodecEncDestroy failed, ret=" << ret;
  }

  priv_->stream_buffer.reset();
  priv_->stream_buffer_size = 0;
  priv_->ps_buffer.reset();
  priv_->ps_size = 0;
  priv_->eos_sent = priv_->eos_got = false;

  std::unique_lock<std::mutex> dlk(g_device_mutex);
  if (g_device_contexts.count(param_.mlu_device_id) > 0) {
    DeviceContext &ctx = g_device_contexts[param_.mlu_device_id];
    std::unique_lock<std::mutex> lk(ctx.mutex);
    if (ctx.instances.count(this) > 0) {
      InstanceContext &ictx = ctx.instances[this];
      ctx.index_cv.wait(lk, [&]() { return (ictx.enqueue_index == ictx.process_index); });
      ctx.instances.erase(this);
    }
    lk.unlock();
    ctx.queue_cv.notify_all();
    ctx.index_cv.notify_all();
  }

  state_ = IDLE;

  return cnstream::VideoEncoder::SUCCESS;
}

int VideoEncoderMlu300::RequestFrameBuffer(VideoFrame *frame, int timeout_ms) {
  UniqueReadLock slk(state_mtx_);
  if (state_ != RUNNING) {
    LOGW(VideoEncoderMlu) << "RequestFrameBuffer() not running";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  if (priv_->error) {
    slk.Unlock();
    LOGE(VideoEncoderMlu) << "RequestFrameBuffer() stop for error";
    return Stop();
  }
  if (priv_->eos_sent) {
    LOGE(VideoEncoderMlu) << "RequestFrameBuffer() got EOS already";
    return cnstream::VideoEncoder::ERROR_FAILED;
  }
  if (!frame) return cnstream::VideoEncoder::ERROR_PARAMETERS;

  cncodecFrame_t cn_frame;
  memset(&cn_frame, 0, sizeof(cncodecFrame_t));
  cn_frame.width = param_.width;
  cn_frame.height = param_.height;
  cn_frame.pixel_format = priv_->cn_param.pixel_format;
  i32_t ret = cncodecEncWaitAvailInputBuf(priv_->cn_encoder, &cn_frame, timeout_ms);
  if (CNCODEC_ERROR_TIMEOUT == ret) {
    LOGE(VideoEncoderMlu) << "RequestFrameBuffer() cncodecEncWaitAvailInputBuf timeout";
    return cnstream::VideoEncoder::ERROR_TIMEOUT;
  } else if (CNCODEC_SUCCESS != ret) {
    LOGE(VideoEncoderMlu) << "RequestFrameBuffer() cncodecEncWaitAvailInputBuf failed, ret=" << ret;
    return cnstream::VideoEncoder::ERROR_FAILED;
  }

  uint32_t stride = ALIGN(param_.width, priv_->cn_param.input_stride_align);
  memset(frame, 0, sizeof(VideoFrame));
  frame->width = cn_frame.width > 0 ? cn_frame.width : param_.width;
  frame->height = cn_frame.height > 0 ? cn_frame.height : param_.height;
  frame->data[0] = reinterpret_cast<uint8_t *>(cn_frame.plane[0].dev_addr);
  frame->stride[0] = cn_frame.plane[0].stride > param_.width ? cn_frame.plane[0].stride : stride;
  if (param_.pixel_format == VideoPixelFormat::I420) {
    frame->data[1] = reinterpret_cast<uint8_t *>(cn_frame.plane[1].dev_addr);
    frame->stride[1] = cn_frame.plane[1].stride > param_.width / 2 ? cn_frame.plane[1].stride : stride / 2;
    frame->data[2] = reinterpret_cast<uint8_t *>(cn_frame.plane[2].dev_addr);
    frame->stride[2] = cn_frame.plane[2].stride > param_.width / 2 ? cn_frame.plane[2].stride : stride / 2;
  } else {
    frame->data[1] = reinterpret_cast<uint8_t *>(cn_frame.plane[1].dev_addr);
    frame->stride[1] = cn_frame.plane[1].stride > param_.width ? cn_frame.plane[1].stride : stride;
  }
  frame->pixel_format = param_.pixel_format;
  frame->SetMluDeviceId(param_.mlu_device_id);
  frame->SetMluMemoryChannel(cn_frame.mem_channel);

  std::lock_guard<std::mutex> lk(priv_->list_mtx);
  priv_->input_list.push_back(cn_frame);

  return cnstream::VideoEncoder::SUCCESS;
}

int VideoEncoderMlu300::SendFrame(const VideoFrame *frame, int timeout_ms) {
  UniqueReadLock slk(state_mtx_);
  if (state_ != RUNNING && !(state_ >= RUNNING && ((frame->HasEOS() && !frame->data[0]) || priv_->error))) {
    LOGW(VideoEncoderMlu) << "SendFrame() not running";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  if (priv_->eos_sent) {
    LOGE(VideoEncoderMlu) << "SendFrame() got EOS already";
    return cnstream::VideoEncoder::ERROR_FAILED;
  }
  if (!frame) return cnstream::VideoEncoder::ERROR_PARAMETERS;

  if (!(frame->HasEOS()) && frame->data[0] != nullptr) LOGT(VideoEncoderMlu) << "SendFrame() pts=" << frame->pts;

  cncodecFrame_t cn_frame;
  bool is_back_frame = false;
  if (frame->IsMluMemory()) {
    std::unique_lock<std::mutex> lk(priv_->list_mtx);
    if (!priv_->input_list.empty()) {
      auto input = std::find_if(priv_->input_list.begin(), priv_->input_list.end(),
          [frame, this] (const cncodecFrame_t &input) {
            return ((param_.pixel_format == VideoPixelFormat::I420 &&
                frame->data[0] == reinterpret_cast<uint8_t *>(input.plane[0].dev_addr) &&
                frame->data[1] == reinterpret_cast<uint8_t *>(input.plane[1].dev_addr) &&
                frame->data[2] == reinterpret_cast<uint8_t *>(input.plane[2].dev_addr)) ||
                ((param_.pixel_format == VideoPixelFormat::NV12 || param_.pixel_format == VideoPixelFormat::NV21) &&
                frame->data[0] == reinterpret_cast<uint8_t *>(input.plane[0].dev_addr) &&
                frame->data[1] == reinterpret_cast<uint8_t *>(input.plane[1].dev_addr)));
          });
      if (input != priv_->input_list.end()) {
        cn_frame = *input;
        priv_->input_list.erase(input);
        is_back_frame = true;
        if (priv_->error) {
          if (priv_->input_list.empty() && state_ == RUNNING) {
            lk.unlock();
            slk.Unlock();
            LOGE(VideoEncoderMlu) << "SendFrame() stop for error";
            return Stop();
          } else {
            lk.unlock();
            priv_->list_cv.notify_all();
            return cnstream::VideoEncoder::ERROR_FAILED;
          }
        } else {
          priv_->list_cv.notify_one();
        }
      }
    }

    if (!is_back_frame) {
      LOGE(VideoEncoderMlu) << "SendFrame() memory is not requested from encoder on device " << param_.mlu_device_id;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
    if (frame->GetMluDeviceId() != param_.mlu_device_id) {
      LOGW(VideoEncoderMlu) << "SendFrame() memory is requested from encoder on device " << param_.mlu_device_id
                            << " with bad device id: " << frame->GetMluDeviceId();
      VideoFrame *f = const_cast<VideoFrame *>(frame);
      f->SetMluDeviceId(param_.mlu_device_id);
    }
  } else {
    if (priv_->error) {
      slk.Unlock();
      LOGE(VideoEncoderMlu) << "SendFrame() stop for error";
      return Stop();
    }
  }

  int timeout = timeout_ms < 0 ? 0x7fffffff : timeout_ms;  // TODO(hqw): temp fix for cncodec bug
  if (!is_back_frame && frame->data[0] != nullptr) {
    cncodecFrame_t cn_frame;
    memset(&cn_frame, 0, sizeof(cncodecFrame_t));
    cn_frame.width = param_.width;
    cn_frame.height = param_.height;
    cn_frame.pixel_format = priv_->cn_param.pixel_format;
    int64_t start = CurrentTick();
    i32_t ret = cncodecEncWaitAvailInputBuf(priv_->cn_encoder, &cn_frame, timeout);
    if (CNCODEC_ERROR_TIMEOUT == ret) {
      LOGE(VideoEncoderMlu) << "SendFrame() cncodecEncWaitAvailInputBuf timeout";
      return cnstream::VideoEncoder::ERROR_TIMEOUT;
    } else if (CNCODEC_SUCCESS != ret) {
      LOGE(VideoEncoderMlu) << "SendFrame() cncodecEncWaitAvailInputBuf failed, ret=" << ret;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
    timeout = std::max(timeout - (CurrentTick() - start), 0L);

    cnrtSetDevice(param_.mlu_device_id);

    size_t copy_size;
    switch (param_.pixel_format) {
      case VideoPixelFormat::NV12:
      case VideoPixelFormat::NV21:
        {
        cn_frame.plane[0].stride = frame->stride[0];
        copy_size = frame->stride[0] * frame->height;
        auto ret = cnrtMemcpy(reinterpret_cast<void *>(cn_frame.plane[0].dev_addr), frame->data[0], copy_size,
                              CNRT_MEM_TRANS_DIR_HOST2DEV);
        LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "SendFrame() cnrtMemcpy failed, error code: " << ret;
        cn_frame.plane[1].stride = frame->stride[1];
        copy_size = frame->stride[1] * frame->height / 2;
        ret = cnrtMemcpy(reinterpret_cast<void *>(cn_frame.plane[1].dev_addr), frame->data[1], copy_size,
                              CNRT_MEM_TRANS_DIR_HOST2DEV);
        LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "SendFrame() cnrtMemcpy failed, error code: " << ret;
        }
        break;
      case VideoPixelFormat::I420:
        {
        cn_frame.plane[0].stride = frame->stride[0];
        copy_size = frame->stride[0] * frame->height;
        auto ret = cnrtMemcpy(reinterpret_cast<void *>(cn_frame.plane[0].dev_addr), frame->data[0], copy_size,
                              CNRT_MEM_TRANS_DIR_HOST2DEV);
        LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "SendFrame() cnrtMemcpy failed, error code: " << ret;
        cn_frame.plane[1].stride = frame->stride[1];
        copy_size = frame->stride[1] * frame->height / 2;
        ret = cnrtMemcpy(reinterpret_cast<void *>(cn_frame.plane[1].dev_addr), frame->data[1], copy_size,
                              CNRT_MEM_TRANS_DIR_HOST2DEV);
        LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "SendFrame() cnrtMemcpy failed, error code: " << ret;
        cn_frame.plane[2].stride = frame->stride[2];
        copy_size = frame->stride[2] * frame->height / 2;
        ret = cnrtMemcpy(reinterpret_cast<void *>(cn_frame.plane[2].dev_addr), frame->data[2], copy_size,
                                        CNRT_MEM_TRANS_DIR_HOST2DEV);
        LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "SendFrame() cnrtMemcpy failed, error code: " << ret;
        }
        break;
      default:
        LOGE(VideoEncoderMlu) << "SendFrame() unsupported pixel format: " << param_.pixel_format;
        return cnstream::VideoEncoder::ERROR_FAILED;
    }
  }

  int ret = cnstream::VideoEncoder::SUCCESS;

  u64_t pts = priv_->data_index++ % CNCODEC_PTS_MAX_VALUE;
  if (frame->data[0] != nullptr) {
    std::unique_lock<std::mutex> lk(priv_->info_mtx);
    int64_t frame_pts = frame->pts;
    if (frame_pts == INVALID_TIMESTAMP) {
      frame_pts = priv_->frame_count * param_.time_base / param_.frame_rate;
    }
    priv_->encoding_info[pts] = (EncodingInfo){frame_pts, frame->dts, CurrentTick(), 0, frame->user_data};
    lk.unlock();

    cn_frame.pts = pts;

    cncodecEncPicAttr_t frame_attr;
    memset(&frame_attr, 0, sizeof(cncodecEncPicAttr_t));
    if (param_.codec_type == VideoCodecType::JPEG) {
      frame_attr.jpg_pic_attr.jpeg_param.quality = param_.jpeg_quality;
    }
    i32_t cnret = cncodecEncSendFrame(priv_->cn_encoder, &cn_frame, &frame_attr, timeout);
    if (CNCODEC_ERROR_TIMEOUT == cnret) {
      LOGE(VideoEncoderMlu) << "SendFrame() cncodecEncSendFrame timeout";
      ret = cnstream::VideoEncoder::ERROR_TIMEOUT;
    } else if (CNCODEC_SUCCESS != cnret) {
      LOGE(VideoEncoderMlu) << "SendFrame() cncodecEncSendFrame failed, ret=" << cnret;
      ret = cnstream::VideoEncoder::ERROR_FAILED;
    }

    if (ret == cnstream::VideoEncoder::SUCCESS) {
      priv_->frame_count++;
    } else {
      lk.lock();
      priv_->encoding_info.erase(pts);
      lk.unlock();
    }
  }

  if (frame->HasEOS()) {
    i32_t cnret = cncodecEncSetEos(priv_->cn_encoder);
    if (CNCODEC_SUCCESS != cnret) {
      LOGE(VideoEncoderMlu) << "SendFrame() cncodecEncSetEos failed, ret=" << cnret;
      ret = cnstream::VideoEncoder::ERROR_FAILED;
    } else {
      priv_->eos_sent = true;
    }
  }

  return ret;
}

int VideoEncoderMlu300::GetPacket(VideoPacket *packet, PacketInfo *info) {
  UniqueReadLock slk(state_mtx_);
  if (state_ != RUNNING) {
    LOGW(VideoEncoderMlu) << "GetPacket() not running";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  if (priv_->error) {
    slk.Unlock();
    LOGE(VideoEncoderMlu) << "GetPacket() stop for error";
    return Stop();
  }
  return VideoEncoderBase::GetPacket(packet, info);
}

bool VideoEncoderMlu300::GetPacketInfo(int64_t index, PacketInfo *info) {
  if (!info) return false;

  std::lock_guard<std::mutex> lk(priv_->info_mtx);
  if (priv_->encoding_info.count(index) == 0) {
    LOGE(VideoEncoderMlu) << "GetPacketInfo() find index: " << index << " failed";
    return false;
  }
  auto &enc_info = priv_->encoding_info[index];
  info->start_tick = enc_info.start_tick;
  info->end_tick = enc_info.end_tick;
  priv_->encoding_info.erase(index);
  return true;
}

i32_t VideoEncoderMlu300::EventHandlerCallback(int event, void *data) {
  std::lock_guard<std::mutex> dlk(g_device_mutex);
  if (g_device_contexts.count(param_.mlu_device_id) == 0) {
    LOGE(VideoEncoderMlu) << "EventHandlerCallback() context is not exist for device " << param_.mlu_device_id;
    return 0;
  }
  DeviceContext &ctx = g_device_contexts[param_.mlu_device_id];
  std::unique_lock<std::mutex> lk(ctx.mutex);
  if (ctx.instances.count(this) == 0) {
    LOGE(VideoEncoderMlu) << "EventHandlerCallback() instance " << this << " is not exist";
    return 0;
  }
  InstanceContext &ictx = ctx.instances[this];
  EventData event_data;
  event_data.event = event;
  event_data.encoder = this;
  if (event == CNCODEC_EVENT_NEW_FRAME) {
    if (state_ != RUNNING) {
      LOGW(VideoEncoderMlu) << "EventHandlerCallback() not running";
      return 0;
    }
    cncodecStream_t *stream = reinterpret_cast<cncodecStream_t *>(data);
    cncodecEncStreamRef(priv_->cn_encoder, stream);
    event_data.data = *stream;
  }
  // return EventHandler(event, data);
  event_data.index = ictx.enqueue_index++;
  ctx.queue.push(event_data);
  lk.unlock();
  ctx.queue_cv.notify_one();

  return 0;
}

i32_t VideoEncoderMlu300::EventHandler(int event, void *data) {
  switch (event) {
    case CNCODEC_EVENT_NEW_FRAME:
      ReceivePacket(data);
      break;
    case CNCODEC_EVENT_EOS:
      ReceiveEOS();
      break;
    case CNCODEC_EVENT_FRAME_PROCESSED:
      LOGI(VideoEncoderMlu) << "EventHandler(FRAME_PROCESSED)";
      break;
    default:
      return ErrorHandler(event);
  }
  return 0;
}

void VideoEncoderMlu300::ReceivePacket(void *data) {
  ReadLockGuard slk(state_mtx_);
  if (state_ != RUNNING) {
    LOGW(VideoEncoderMlu) << "ReceivePacket() not running";
    cncodecStream_t *stream = reinterpret_cast<cncodecStream_t *>(data);
    cncodecEncStreamUnref(priv_->cn_encoder, stream);
    return;
  }

  cnrtSetDevice(param_.mlu_device_id);

  VideoPacket packet;
  memset(&packet, 0, sizeof(VideoPacket));
  cncodecStream_t *stream = reinterpret_cast<cncodecStream_t *>(data);
  LOGT(VideoEncoderMlu) << "ReceivePacket size=" << stream->data_len << ", pts=" << stream->pts
                        << ", type=" << stream->stream_type;
  cncodecStreamType_t stream_type = stream->stream_type;
  if (stream_type == CNCODEC_H264_NALU_TYPE_SPS_PPS || stream_type == CNCODEC_HEVC_NALU_TYPE_VPS_SPS_PPS) {
    LOGI(VideoEncoderMlu) << "ReceivePacket() got parameter sets, size=" << stream->data_len;
    priv_->ps_buffer.reset(new (std::nothrow) uint8_t[stream->data_len]);
    priv_->ps_size = stream->data_len;
    auto ret = cnrtMemcpy(priv_->ps_buffer.get(), reinterpret_cast<void *>(stream->mem_addr + stream->data_offset),
                          stream->data_len, CNRT_MEM_TRANS_DIR_DEV2HOST);
    LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "ReceivePacket() cnrtMemcpy failed, error code: " << ret;
    cncodecEncStreamUnref(priv_->cn_encoder, stream);
    return;
  } else if (stream_type == CNCODEC_NALU_TYPE_IDR || stream_type == CNCODEC_NALU_TYPE_I) {
    LOGT(VideoEncoderMlu) << "ReceivePacket() got key frame";
    packet.SetKey();
  } else if (stream_type == CNCODEC_NALU_TYPE_EOS) {
    cncodecEncStreamUnref(priv_->cn_encoder, stream);
    return;
  }
  if (priv_->stream_buffer_size < stream->data_len) {
    priv_->stream_buffer.reset(new (std::nothrow) uint8_t[stream->data_len]);
    priv_->stream_buffer_size = stream->data_len;
  }
  auto ret = cnrtMemcpy(priv_->stream_buffer.get(), reinterpret_cast<void *>(stream->mem_addr + stream->data_offset),
                        stream->data_len, CNRT_MEM_TRANS_DIR_DEV2HOST);
  LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "ReceivePacket() cnrtMemcpy failed, error code: " << ret;

  cncodecEncStreamUnref(priv_->cn_encoder, stream);

  packet.data = priv_->stream_buffer.get();
  packet.size = stream->data_len;
  packet.pts = stream->pts;
  packet.dts = INVALID_TIMESTAMP;

  // find out packet and update encoding info
  std::unique_lock<std::mutex> lk(priv_->info_mtx);
  int64_t index = packet.pts;
  auto info = priv_->encoding_info.find(index);
  if (info != priv_->encoding_info.end()) {
    info->second.end_tick = CurrentTick();
    packet.pts = info->second.pts;
    if (info->second.dts == INVALID_TIMESTAMP) {
      packet.dts = (priv_->packet_count - 2) * param_.time_base / param_.frame_rate;
    } else {
      packet.dts = info->second.dts;
    }
    packet.user_data = info->second.user_data;
  } else {
    LOGE(VideoEncoderMlu) << "ReceivePacket() restore encoding info failed, index=" << index;
    return;
  }
  lk.unlock();
  LOGT(VideoEncoderMlu) << "ReceivePacket() got packet, size=" << packet.size << ", pts=" << packet.pts
                        << ", dts=" << packet.dts << ", user_data=" << packet.user_data;
  IndexedVideoPacket vpacket;
  vpacket.packet = packet;
  vpacket.index = index;
  PushBuffer(&vpacket);
  priv_->packet_count++;
  std::lock_guard<std::mutex> cblk(cb_mtx_);
  if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_DATA);
}

void VideoEncoderMlu300::ReceiveEOS() {
  if (state_ < RUNNING) {
    LOGW(VideoEncoderMlu) << "ReceiveEOS() not running";
    return;
  }
  LOGI(VideoEncoderMlu) << "ReceiveEOS()";
  std::unique_lock<std::mutex> eos_lk(priv_->eos_mtx);
  priv_->eos_got = true;
  eos_lk.unlock();
  priv_->eos_cv.notify_one();
  std::lock_guard<std::mutex> lk(cb_mtx_);
  if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_EOS);
}

i32_t VideoEncoderMlu300::ErrorHandler(int event) {
  std::lock_guard<std::mutex> lk(cb_mtx_);
  switch (event) {
    case CNCODEC_EVENT_OUT_OF_MEMORY:
      LOGE(VideoEncoderMlu) << "ErrorHandler() out of memory error thrown from cncodec";
      priv_->error = true;
      if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_ERROR);
      break;
    case CNCODEC_EVENT_FATAL_ERROR:
      LOGE(VideoEncoderMlu) << "ErrorHandler() fatal error thrown from cncodec";
      priv_->error = true;
      if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_ERROR);
      break;
    default:
      LOGE(VideoEncoderMlu) << "ErrorHandler() unknown event: " << event;
      if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_ERROR);
      return -1;
  }
  return 0;
}

}  // namespace video

}  // namespace cnstream

#endif
