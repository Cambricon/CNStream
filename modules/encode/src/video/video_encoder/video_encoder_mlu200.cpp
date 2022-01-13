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

#include "cnrt.h"
#include "cn_codec_common.h"
#include "cn_jpeg_enc.h"
#include "cn_video_enc.h"

#include "cnstream_logging.hpp"

#include "video_encoder_mlu200.hpp"

namespace cnstream {

namespace video {

static const char *pf_str[] = {"I420", "NV12", "NV21", "BGR", "RGB"};
static const char *ct_str[] = {"H264", "H265", "MPEG4", "JPEG"};

#define CNCODEC_ALLOC_BITSTREAM_BUFFER_SIZE 0x400000
#define CNCODEC_PTS_MAX_VALUE (0xffffffffffffffffLL / 1000)

struct EncodingInfo {
  int64_t pts, dts;
  int64_t start_tick, end_tick;
  void *user_data;
};

struct VideoEncoderMlu200Private {
  cnvideoEncCreateInfo ve_param;
  cnjpegEncCreateInfo je_param;
  void *cn_encoder = nullptr;
  std::mutex list_mtx;
  std::list<cnjpegEncInput> ji_list;
  std::list<cnvideoEncInput> vi_list;
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

#define THREAD_NUMBER_PER_DEVICE 4

struct EventData {
  int event;
  union {
    cnjpegEncOutput jout;
    cnvideoEncOutput vout;
  } data;
  VideoEncoderMlu200 *encoder;
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
  std::map<VideoEncoderMlu200 *, InstanceContext> instances;
};

static std::mutex g_device_mutex;
static std::map<int, DeviceContext> g_device_contexts;

static inline int64_t CurrentTick() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

static i32_t EncoderEventCallback(cncodecCbEventType event, void *user_ctx, void *data) {
  VideoEncoderMlu200 *encoder = reinterpret_cast<VideoEncoderMlu200 *>(user_ctx);
  return encoder->EventHandlerCallback(event, data);
}

VideoEncoderMlu200::VideoEncoderMlu200(const VideoEncoder::Param &param) : VideoEncoderBase(param) {
  LOGI(VideoEncoderMlu) << "VideoEncoderMlu200(" << param.width << "x" << param.height << ", "
                        << pf_str[param_.pixel_format] << ", " << ct_str[param_.codec_type] << ")";
#if CNRT_MAJOR_VERSION < 5
  cnrtInit(0);
#endif
  priv_.reset(new (std::nothrow) VideoEncoderMlu200Private);
}

VideoEncoderMlu200::~VideoEncoderMlu200() {
  Stop();
#if CNRT_MAJOR_VERSION < 5
  cnrtDestroy();
#endif
  priv_.reset();
}

int VideoEncoderMlu200::Start() {
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

  std::unique_lock<std::mutex> dlk(g_device_mutex);
  if (param_.codec_type == VideoCodecType::JPEG) {
    memset(&priv_->je_param, 0, sizeof(priv_->je_param));
    priv_->je_param.deviceId = param_.mlu_device_id;
    priv_->je_param.instance = CNVIDEOENC_INSTANCE_AUTO;
    if (param_.pixel_format == VideoPixelFormat::NV12) {
      priv_->je_param.pixelFmt = CNCODEC_PIX_FMT_NV12;
    } else if (param_.pixel_format == VideoPixelFormat::NV21) {
      priv_->je_param.pixelFmt = CNCODEC_PIX_FMT_NV21;
    // } else if (param_.pixel_format == VideoPixelFormat::I420) {
    //   priv_->je_param.pixelFmt = CNCODEC_PIX_FMT_I420;
    } else {
      LOGE(VideoEncoderMlu) << "Start() unsupported pixel format: " << pf_str[param_.pixel_format];
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_PARAMETERS;
    }
    priv_->je_param.colorSpace = CNCODEC_COLOR_SPACE_BT_709;
    priv_->je_param.width = param_.width;
    priv_->je_param.height = param_.height;
    priv_->je_param.inputBuf = nullptr;
    priv_->je_param.outputBuf = nullptr;
    priv_->je_param.inputBufNum = param_.input_buffer_count;
    priv_->je_param.outputBufNum = 6;
    priv_->je_param.allocType = CNCODEC_BUF_ALLOC_LIB;
    priv_->je_param.userContext = reinterpret_cast<void *>(this);
    priv_->je_param.suggestedLibAllocBitStrmBufSize = CNCODEC_ALLOC_BITSTREAM_BUFFER_SIZE;

    i32_t ret = cnjpegEncCreate(reinterpret_cast<cnjpegEncoder *>(&priv_->cn_encoder), CNJPEGENC_RUN_MODE_ASYNC,
                                EncoderEventCallback, &priv_->je_param);
    if (CNCODEC_SUCCESS != ret) {
      LOGE(VideoEncoderMlu) << "Start() cnjpegEncCreate failed, ret=" << ret;
      priv_->cn_encoder = nullptr;
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
    /*
    ret = cnjpegEncSetAttribute(reinterpret_cast<cnjpegEncoder *>(&priv_->cn_encoder),
                                CNJPEGENC_ATTR_USER_PICTURE_QUALITY, (void *)&param_.jpeg_quality);
    if (ret != CNCODEC_SUCCESS) {
      LOGE(VideoEncoderMlu) << "Start() cnjpegEncSetAttribute(USER_PICTURE_QUALITY) failed, ret=" << ret;
      priv_->cn_encoder = nullptr;
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
    */
  } else {
    memset(&priv_->ve_param, 0, sizeof(priv_->ve_param));
    priv_->ve_param.deviceId = param_.mlu_device_id;
    priv_->ve_param.instance = CNVIDEOENC_INSTANCE_AUTO;
    if (param_.codec_type == VideoCodecType::H264) {
      priv_->ve_param.codec = CNCODEC_H264;
    } else if (param_.codec_type == VideoCodecType::H265) {
      priv_->ve_param.codec = CNCODEC_HEVC;
    } else {
      LOGE(VideoEncoderMlu) << "Start() unsupported codec type: " << ct_str[param_.codec_type];
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_PARAMETERS;
    }
    if (param_.pixel_format == VideoPixelFormat::NV12) {
      priv_->ve_param.pixelFmt = CNCODEC_PIX_FMT_NV12;
    } else if (param_.pixel_format == VideoPixelFormat::NV21) {
      priv_->ve_param.pixelFmt = CNCODEC_PIX_FMT_NV21;
    // } else if (param_.pixel_format == VideoPixelFormat::I420) {
    //   priv_->ve_param.pixelFmt = CNCODEC_PIX_FMT_I420;
    } else {
      LOGE(VideoEncoderMlu) << "Start() unsupported pixel format: " << pf_str[param_.pixel_format];
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_PARAMETERS;
    }
    priv_->ve_param.colorSpace = CNCODEC_COLOR_SPACE_BT_709;
    priv_->ve_param.width = param_.width;
    priv_->ve_param.height = param_.height;
    priv_->ve_param.userContext = reinterpret_cast<void *>(this);
    priv_->ve_param.inputBuf = nullptr;
    priv_->ve_param.outputBuf = nullptr;
    priv_->ve_param.inputBufNum = param_.input_buffer_count;
    priv_->ve_param.outputBufNum = 6;
    priv_->ve_param.allocType = CNCODEC_BUF_ALLOC_LIB;
    priv_->ve_param.suggestedLibAllocBitStrmBufSize = CNCODEC_ALLOC_BITSTREAM_BUFFER_SIZE;

    priv_->ve_param.rateCtrl.rcMode = CNVIDEOENC_RATE_CTRL_CBR;
    priv_->ve_param.fpsNumerator = param_.frame_rate;
    priv_->ve_param.fpsDenominator = 1;
    priv_->ve_param.rateCtrl.targetBitrate = param_.bit_rate;
    priv_->ve_param.rateCtrl.gopLength = param_.gop_size;

    if (param_.codec_type == VideoCodecType::H264) {
      priv_->ve_param.uCfg.h264.profile = CNVIDEOENC_PROFILE_H264_HIGH;
      priv_->ve_param.uCfg.h264.level = CNVIDEOENC_LEVEL_H264_51;
      priv_->ve_param.uCfg.h264.insertSpsPpsWhenIDR = 1;
      priv_->ve_param.uCfg.h264.IframeInterval = param_.gop_size;
      priv_->ve_param.uCfg.h264.BFramesNum = 1;
      priv_->ve_param.uCfg.h264.sliceMode = CNVIDEOENC_SLICE_MODE_SINGLE;
      priv_->ve_param.uCfg.h264.gopType = CNVIDEOENC_GOP_TYPE_BIDIRECTIONAL;
      priv_->ve_param.uCfg.h264.entropyMode = CNVIDEOENC_ENTROPY_MODE_CABAC;
    } else if (param_.codec_type == VideoCodecType::H265) {
      priv_->ve_param.uCfg.h265.profile = CNVIDEOENC_PROFILE_H265_MAIN;
      priv_->ve_param.uCfg.h265.level = CNVIDEOENC_LEVEL_H265_HIGH_51;
      priv_->ve_param.uCfg.h265.insertSpsPpsWhenIDR = 1;
      priv_->ve_param.uCfg.h265.IframeInterval = param_.gop_size;
      priv_->ve_param.uCfg.h265.BFramesNum = 2;
      priv_->ve_param.uCfg.h265.sliceMode = CNVIDEOENC_SLICE_MODE_SINGLE;
      priv_->ve_param.uCfg.h265.gopType = CNVIDEOENC_GOP_TYPE_BIDIRECTIONAL;
    }

    i32_t ret = cnvideoEncCreate(reinterpret_cast<cnvideoEncoder *>(&priv_->cn_encoder), EncoderEventCallback,
                                 &priv_->ve_param);
    if (CNCODEC_SUCCESS != ret) {
      LOGE(VideoEncoderMlu) << "Start() cnvideoEncCreate failed, ret=" << ret;
      priv_->cn_encoder = nullptr;
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
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

int VideoEncoderMlu200::Stop() {
  UniqueWriteLock slk(state_mtx_);
  if (state_ != RUNNING) {
    // LOGW(VideoEncoderMlu) << "Stop() state != RUNNING";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  state_ = STOPPING;
  slk.Unlock();

  std::unique_lock<std::mutex> lk(priv_->list_mtx);
  if (param_.codec_type == VideoCodecType::JPEG) {
    if (!priv_->ji_list.empty()) {
      LOGW(VideoEncoderMlu) << "Stop() " << priv_->ji_list.size() << " frame buffers still outside";
      priv_->list_cv.wait(lk, [this]() { return priv_->ji_list.empty(); });
    }
  } else {
    if (!priv_->vi_list.empty()) {
      LOGW(VideoEncoderMlu) << "Stop() " << priv_->vi_list.size() << " frame buffers still outside";
      priv_->list_cv.wait(lk, [this]() { return priv_->vi_list.empty(); });
    }
  }
  lk.unlock();

  if (!priv_->error) {
    // send and wait eos
    if (!priv_->eos_got) {
      if (!priv_->eos_sent) {
        LOGI(VideoEncoderMlu) << "Stop() send EOS";
        VideoFrame frame;
        memset(&frame, 0, sizeof(VideoFrame));
        frame.pts = INVALID_TIMESTAMP;
        frame.SetEOS();
        if (cnstream::VideoEncoder::SUCCESS != SendFrame(&frame)) {
          LOGE(VideoEncoderMlu) << "Stop() send EOS failed";
          state_ = RUNNING;
          return cnstream::VideoEncoder::ERROR_FAILED;
        }
        priv_->eos_sent = true;
      }
      std::unique_lock<std::mutex> eos_lk(priv_->eos_mtx);
      if (false == priv_->eos_cv.wait_for(eos_lk, std::chrono::seconds(10),
                                          [this]() { return priv_->eos_got.load(); })) {
        LOGE(VideoEncoderMlu) << "Stop() wait EOS for 10s timeout";
      }
    }

    if (priv_->cn_encoder) {
      i32_t ret;
      if (priv_->eos_sent && priv_->eos_got) {
        // destroy cn encoder
        if (param_.codec_type == VideoCodecType::JPEG) {
          ret = cnjpegEncDestroy(reinterpret_cast<cnjpegEncoder>(priv_->cn_encoder));
        } else {
          ret = cnvideoEncDestroy(reinterpret_cast<cnvideoEncoder>(priv_->cn_encoder));
        }
        if (CNCODEC_SUCCESS != ret) {
          LOGE(VideoEncoderMlu) << "Stop() destroy cn_encoder failed, ret=" << ret;
        }
      } else {
        // abort cn encoder
        LOGE(VideoEncoderMlu) << "Stop() abort cn_encoder for EOS error";
        if (param_.codec_type == VideoCodecType::JPEG) {
          ret = cnjpegEncAbort(reinterpret_cast<cnjpegEncoder>(priv_->cn_encoder));
        } else {
          ret = cnvideoEncAbort(reinterpret_cast<cnvideoEncoder>(priv_->cn_encoder));
        }
        if (CNCODEC_SUCCESS != ret) {
          LOGE(VideoEncoderMlu) << "Stop() abort cn_encoder failed, ret=" << ret;
        }
      }
    }
  } else {
    // abort cn encoder
    if (priv_->cn_encoder) {
      LOGE(VideoEncoderMlu) << "Stop() abort cn_encoder for error";
      i32_t ret;
      if (param_.codec_type == VideoCodecType::JPEG) {
        ret = cnjpegEncAbort(reinterpret_cast<cnjpegEncoder>(priv_->cn_encoder));
      } else {
        ret = cnvideoEncAbort(reinterpret_cast<cnvideoEncoder>(priv_->cn_encoder));
      }
      if (CNCODEC_SUCCESS != ret) {
        LOGE(VideoEncoderMlu) << "Stop() abort cn_encoder failed, ret=" << ret;
      }
    }
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

int VideoEncoderMlu200::RequestFrameBuffer(VideoFrame *frame, int timeout_ms) {
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

  cnjpegEncInput je_input;
  cnvideoEncInput ve_input;
  cncodecFrame *cn_frame = nullptr;
  if (param_.codec_type == VideoCodecType::JPEG) {
    memset(&je_input, 0, sizeof(cnjpegEncInput));
    cn_frame = &je_input.frame;
    i32_t ret = cnjpegEncWaitAvailInputBuf(reinterpret_cast<cnjpegEncoder>(priv_->cn_encoder), cn_frame, timeout_ms);
    if (CNCODEC_TIMEOUT == ret) {
      LOGE(VideoEncoderMlu) << "RequestFrameBuffer() cnjpegEncWaitAvailInputBuf timeout";
      return cnstream::VideoEncoder::ERROR_TIMEOUT;
    } else if (CNCODEC_SUCCESS != ret) {
      LOGE(VideoEncoderMlu) << "RequestFrameBuffer() cnjpegEncWaitAvailInputBuf failed, ret=" << ret;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
  } else {
    memset(&ve_input, 0, sizeof(cnvideoEncInput));
    cn_frame = &ve_input.frame;
    i32_t ret = cnvideoEncWaitAvailInputBuf(reinterpret_cast<cnvideoEncoder>(priv_->cn_encoder), cn_frame, timeout_ms);
    if (-CNCODEC_TIMEOUT == ret) {
      LOGE(VideoEncoderMlu) << "RequestFrameBuffer() cnvideoEncWaitAvailInputBuf timeout";
      return cnstream::VideoEncoder::ERROR_TIMEOUT;
    } else if (CNCODEC_SUCCESS != ret) {
      LOGE(VideoEncoderMlu) << "RequestFrameBuffer() cnvideoEncWaitAvailInputBuf failed, ret=" << ret;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
  }

  memset(frame, 0, sizeof(VideoFrame));
  frame->width = cn_frame->width;
  frame->height = cn_frame->height;
  frame->data[0] = reinterpret_cast<uint8_t *>(cn_frame->plane[0].addr);
  frame->stride[0] = cn_frame->stride[0];
  frame->data[1] = reinterpret_cast<uint8_t *>(cn_frame->plane[1].addr);
  frame->stride[1] = cn_frame->stride[1];
  if (param_.pixel_format == VideoPixelFormat::I420) {
    frame->data[2] = reinterpret_cast<uint8_t *>(cn_frame->plane[2].addr);
    frame->stride[2] = cn_frame->stride[2];
  }
  frame->pixel_format = param_.pixel_format;
  frame->SetMluDeviceId(param_.mlu_device_id);
  frame->SetMluMemoryChannel(cn_frame->channel);

  std::lock_guard<std::mutex> llk(priv_->list_mtx);
  if (param_.codec_type == VideoCodecType::JPEG) {
    priv_->ji_list.push_back(je_input);
  } else {
    priv_->vi_list.push_back(ve_input);
  }

  return cnstream::VideoEncoder::SUCCESS;
}

int VideoEncoderMlu200::SendFrame(const VideoFrame *frame, int timeout_ms) {
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

  cnjpegEncInput je_input;
  cnvideoEncInput ve_input;
  bool is_back_frame = false;
  if (frame->IsMluMemory()) {
    std::unique_lock<std::mutex> lk(priv_->list_mtx);
    if (param_.codec_type == VideoCodecType::JPEG) {
      if (!priv_->ji_list.empty()) {
        auto input = std::find_if(priv_->ji_list.begin(), priv_->ji_list.end(),
                                  [frame, this](const cnjpegEncInput &input) {
          return ((param_.pixel_format == VideoPixelFormat::I420 &&
                   frame->data[0] == reinterpret_cast<uint8_t *>(input.frame.plane[0].addr) &&
                   frame->data[1] == reinterpret_cast<uint8_t *>(input.frame.plane[1].addr) &&
                   frame->data[2] == reinterpret_cast<uint8_t *>(input.frame.plane[2].addr)) ||
                  ((param_.pixel_format == VideoPixelFormat::NV12 || param_.pixel_format == VideoPixelFormat::NV21) &&
                   frame->data[0] == reinterpret_cast<uint8_t *>(input.frame.plane[0].addr) &&
                   frame->data[1] == reinterpret_cast<uint8_t *>(input.frame.plane[1].addr)));
        });
        if (input != priv_->ji_list.end()) {
          je_input = *input;
          priv_->ji_list.erase(input);
          is_back_frame = true;
          if (priv_->error) {
            if (priv_->ji_list.empty() && state_ == RUNNING) {
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
    } else {
      if (!priv_->vi_list.empty()) {
        auto input = std::find_if(priv_->vi_list.begin(), priv_->vi_list.end(),
                                  [frame, this](const cnvideoEncInput &input) {
          return ((param_.pixel_format == VideoPixelFormat::I420 &&
                   frame->data[0] == reinterpret_cast<uint8_t *>(input.frame.plane[0].addr) &&
                   frame->data[1] == reinterpret_cast<uint8_t *>(input.frame.plane[1].addr) &&
                   frame->data[2] == reinterpret_cast<uint8_t *>(input.frame.plane[2].addr)) ||
                  ((param_.pixel_format == VideoPixelFormat::NV12 || param_.pixel_format == VideoPixelFormat::NV21) &&
                   frame->data[0] == reinterpret_cast<uint8_t *>(input.frame.plane[0].addr) &&
                   frame->data[1] == reinterpret_cast<uint8_t *>(input.frame.plane[1].addr)));
        });
        if (input != priv_->vi_list.end()) {
          ve_input = *input;
          priv_->vi_list.erase(input);
          is_back_frame = true;
          if (priv_->error) {
            if (priv_->vi_list.empty() && state_ == RUNNING) {
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

  int timeout = timeout_ms;
  if ((!is_back_frame && frame->data[0] != nullptr) || (frame->HasEOS() && frame->data[0] == nullptr)) {
    cncodecFrame *cn_frame = nullptr;
    int64_t start = CurrentTick();
    if (param_.codec_type == VideoCodecType::JPEG) {
      memset(&je_input, 0, sizeof(cnjpegEncInput));
      cn_frame = &je_input.frame;
      i32_t ret = cnjpegEncWaitAvailInputBuf(reinterpret_cast<cnjpegEncoder>(priv_->cn_encoder), cn_frame, timeout);
      if (CNCODEC_TIMEOUT == ret) {
        LOGE(VideoEncoderMlu) << "SendFrame() cnjpegEncWaitAvailInputBuf timeout";
        return cnstream::VideoEncoder::ERROR_TIMEOUT;
      } else if (CNCODEC_SUCCESS != ret) {
        LOGE(VideoEncoderMlu) << "SendFrame() cnjpegEncWaitAvailInputBuf failed, ret=" << ret;
        return cnstream::VideoEncoder::ERROR_FAILED;
      }
    } else {
      memset(&ve_input, 0, sizeof(cnvideoEncInput));
      cn_frame = &ve_input.frame;
      i32_t ret = cnvideoEncWaitAvailInputBuf(reinterpret_cast<cnvideoEncoder>(priv_->cn_encoder), cn_frame, timeout);
      if (-CNCODEC_TIMEOUT == ret) {
        LOGE(VideoEncoderMlu) << "SendFrame() cnvideoEncWaitAvailInputBuf timeout";
        return cnstream::VideoEncoder::ERROR_TIMEOUT;
      } else if (CNCODEC_SUCCESS != ret) {
        LOGE(VideoEncoderMlu) << "SendFrame() cnvideoEncWaitAvailInputBuf failed, ret=" << ret;
        return cnstream::VideoEncoder::ERROR_FAILED;
      }
    }
    timeout = std::max(timeout - (CurrentTick() - start), 0L);

    if (frame->data[0] != nullptr) {
#if CNRT_MAJOR_VERSION < 5
      cnrtDev_t dev;
      cnrtGetDeviceHandle(&dev, param_.mlu_device_id);
      cnrtSetCurrentDevice(dev);
#else
      cnrtSetDevice(param_.mlu_device_id);
#endif
      size_t copy_size;
      switch (param_.pixel_format) {
        case VideoPixelFormat::NV12:
        case VideoPixelFormat::NV21:
          {
          cn_frame->stride[0] = frame->stride[0];
          copy_size = frame->stride[0] * frame->height;
          auto ret = cnrtMemcpy(reinterpret_cast<void *>(cn_frame->plane[0].addr), frame->data[0], copy_size,
                                CNRT_MEM_TRANS_DIR_HOST2DEV);
          LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "SendFrame() cnrtMemcpy failed, error code: " << ret;
          cn_frame->stride[1] = frame->stride[1];
          copy_size = frame->stride[1] * frame->height / 2;
          ret = cnrtMemcpy(reinterpret_cast<void *>(cn_frame->plane[1].addr), frame->data[1], copy_size,
                                CNRT_MEM_TRANS_DIR_HOST2DEV);
          LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "SendFrame() cnrtMemcpy failed, error code: " << ret;
          }
          break;
        case VideoPixelFormat::I420:
          {
          cn_frame->stride[0] = frame->stride[0];
          copy_size = frame->stride[0] * frame->height;
          auto ret = cnrtMemcpy(reinterpret_cast<void *>(cn_frame->plane[0].addr), frame->data[0], copy_size,
                                CNRT_MEM_TRANS_DIR_HOST2DEV);
          LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "SendFrame() cnrtMemcpy failed, error code: " << ret;
          cn_frame->stride[1] = frame->stride[1];
          copy_size = frame->stride[1] * frame->height / 2;
          ret = cnrtMemcpy(reinterpret_cast<void *>(cn_frame->plane[1].addr), frame->data[1], copy_size,
                                CNRT_MEM_TRANS_DIR_HOST2DEV);
          LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "SendFrame() cnrtMemcpy failed, error code: " << ret;
          cn_frame->stride[2] = frame->stride[2];
          copy_size = frame->stride[2] * frame->height / 2;
          ret = cnrtMemcpy(reinterpret_cast<void *>(cn_frame->plane[2].addr), frame->data[2], copy_size,
                                CNRT_MEM_TRANS_DIR_HOST2DEV);
          LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "SendFrame() cnrtMemcpy failed, error code: " << ret;
          }
          break;
        default:
          LOGE(VideoEncoderMlu) << "SendFrame() unsupported pixel format: " << param_.pixel_format;
          return cnstream::VideoEncoder::ERROR_FAILED;
      }
    }
  }

  u64_t pts = priv_->data_index++ % CNCODEC_PTS_MAX_VALUE;
  if (frame->data[0] != nullptr) {
    std::lock_guard<std::mutex> lk(priv_->info_mtx);
    int64_t frame_pts = frame->pts;
    if (frame_pts == INVALID_TIMESTAMP) {
      frame_pts = priv_->frame_count * param_.time_base / param_.frame_rate;
    }
    priv_->encoding_info[pts] = (EncodingInfo){frame_pts, frame->dts, CurrentTick(), 0, frame->user_data};
  }

  int ret = cnstream::VideoEncoder::SUCCESS;
  if (param_.codec_type == VideoCodecType::JPEG) {
    je_input.pts = pts;
    if (frame->HasEOS()) {
      je_input.flags |= CNJPEGENC_FLAG_EOS;
      if (!frame->data[0]) {
        je_input.flags |= CNJPEGENC_FLAG_INVALID;
        LOGI(VideoEncoderMlu) << "SendFrame() send jpeg EOS individually";
      } else {
        LOGI(VideoEncoderMlu) << "SendFrame() send jpeg EOS with data";
      }
    } else {
      je_input.flags &= (~CNJPEGENC_FLAG_EOS);
    }
    cnjpegEncParameters params;
    memset(&params, 0, sizeof(cnjpegEncParameters));
    params.quality = param_.jpeg_quality;
    params.restartInterval = 0;
    i32_t cnret = cnjpegEncFeedFrame(reinterpret_cast<cnjpegEncoder>(priv_->cn_encoder), &je_input, &params, timeout);
    if (CNCODEC_TIMEOUT == cnret) {
      LOGE(VideoEncoderMlu) << "SendFrame() cnjpegEncFeedFrame timeout";
      ret = cnstream::VideoEncoder::ERROR_TIMEOUT;
    } else if (CNCODEC_SUCCESS != cnret) {
      LOGE(VideoEncoderMlu) << "SendFrame() cnjpegEncFeedFrame failed, ret=" << cnret;
      ret = cnstream::VideoEncoder::ERROR_FAILED;
    }
  } else {
    ve_input.pts = pts;
    if (frame->HasEOS()) {
      ve_input.flags |= CNVIDEOENC_FLAG_EOS;
      if (!frame->data[0]) {
        ve_input.flags |= CNVIDEOENC_FLAG_INVALID_FRAME;
        LOGI(VideoEncoderMlu) << "SendFrame() send video EOS individually";
      } else {
        LOGI(VideoEncoderMlu) << "SendFrame() send video EOS with data";
      }
    } else {
      ve_input.flags &= (~CNVIDEOENC_FLAG_EOS);
    }
    i32_t cnret = cnvideoEncFeedFrame(reinterpret_cast<cnvideoEncoder>(priv_->cn_encoder), &ve_input, timeout);
    if (-CNCODEC_TIMEOUT == cnret) {
      LOGE(VideoEncoderMlu) << "SendFrame() cnvideoEncFeedFrame timeout";
      ret = cnstream::VideoEncoder::ERROR_TIMEOUT;
    } else if (CNCODEC_SUCCESS != cnret) {
      LOGE(VideoEncoderMlu) << "SendFrame() cnvideoEncFeedFrame failed, ret=" << cnret;
      ret = cnstream::VideoEncoder::ERROR_FAILED;
    }
  }

  if (ret == cnstream::VideoEncoder::SUCCESS) {
    if (frame->HasEOS()) priv_->eos_sent = true;
    if (frame->data[0] != nullptr) priv_->frame_count++;
  } else {
    std::lock_guard<std::mutex> lk(priv_->info_mtx);
    if (frame->data[0] != nullptr) priv_->encoding_info.erase(pts);
  }

  return ret;
}

int VideoEncoderMlu200::GetPacket(VideoPacket *packet, PacketInfo *info) {
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

bool VideoEncoderMlu200::GetPacketInfo(int64_t index, PacketInfo *info) {
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

i32_t VideoEncoderMlu200::EventHandlerCallback(int event, void *data) {
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
  if (event == CNCODEC_CB_EVENT_NEW_FRAME) {
    if (state_ != RUNNING) {
      LOGW(VideoEncoderMlu) << "EventHandlerCallback() not running";
      return 0;
    }
    if (param_.codec_type == VideoCodecType::JPEG) {
      // cnjpegEncOutput *output = reinterpret_cast<cnjpegEncOutput *>(data);
      // cnjpegEncAddReference(reinterpret_cast<cnjpegEncoder>(priv_->cn_encoder), &(output->streamBuffer));
      // event_data.data.vout = *output;
      ReceivePacket(data);
      return 0;
    } else {
      cnvideoEncOutput *output = reinterpret_cast<cnvideoEncOutput *>(data);
      cnvideoEncAddReference(reinterpret_cast<cnvideoEncoder>(priv_->cn_encoder), &(output->streamBuffer));
      event_data.data.vout = *output;
    }
  }
  // return EventHandler(event, data);
  event_data.index = ictx.enqueue_index++;
  ctx.queue.push(event_data);
  lk.unlock();
  ctx.queue_cv.notify_one();

  return 0;
}

i32_t VideoEncoderMlu200::EventHandler(int event, void *data) {
  switch (event) {
    case CNCODEC_CB_EVENT_NEW_FRAME:
      ReceivePacket(data);
      break;
    case CNCODEC_CB_EVENT_EOS:
      ReceiveEOS();
      break;
    default:
      return ErrorHandler(event);
  }
  return 0;
}

void VideoEncoderMlu200::ReceivePacket(void *data) {
  ReadLockGuard slk(state_mtx_);
  if (state_ != RUNNING) {
    LOGW(VideoEncoderMlu) << "ReceivePacket() not running";
    if (param_.codec_type != VideoCodecType::JPEG) {
      cnvideoEncOutput *output = reinterpret_cast<cnvideoEncOutput *>(data);
      cnvideoEncReleaseReference(reinterpret_cast<cnvideoEncoder>(priv_->cn_encoder), &(output->streamBuffer));
    }
    return;
  }

#if CNRT_MAJOR_VERSION < 5
  cnrtDev_t dev;
  cnrtGetDeviceHandle(&dev, param_.mlu_device_id);
  cnrtSetCurrentDevice(dev);
#else
  cnrtSetDevice(param_.mlu_device_id);
#endif

  VideoPacket packet;
  bool eos = false;
  memset(&packet, 0, sizeof(VideoPacket));
  if (param_.codec_type == VideoCodecType::JPEG) {
    cnjpegEncOutput *output = reinterpret_cast<cnjpegEncOutput *>(data);
    LOGT(VideoEncoderMlu) << "ReceiveJPEGPacket size=" << output->streamLength << ", pts=" << output->pts;
    if (priv_->stream_buffer_size < output->streamLength) {
      priv_->stream_buffer.reset(new (std::nothrow) uint8_t[output->streamLength]);
      priv_->stream_buffer_size = output->streamLength;
    }
    auto ret = cnrtMemcpy(priv_->stream_buffer.get(),
                         reinterpret_cast<void *>(output->streamBuffer.addr + output->dataOffset),
                         output->streamLength, CNRT_MEM_TRANS_DIR_DEV2HOST);
    LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "ReceivePacket() cnrtMemcpy failed, error code: " << ret;
    packet.data = priv_->stream_buffer.get();
    packet.size = output->streamLength;
    packet.pts = output->pts;
    packet.dts = INVALID_TIMESTAMP;
    eos = output->flags & CNJPEGENC_FLAG_EOS;
    // cnjpegEncReleaseReference(reinterpret_cast<cnjpegEncoder>(priv_->cn_encoder), &(output->streamBuffer));
  } else {
    cnvideoEncOutput *output = reinterpret_cast<cnvideoEncOutput *>(data);
    LOGT(VideoEncoderMlu) << "ReceiveVideoPacket size=" << output->streamLength << ", pts=" << output->pts
                          << ", type=" << output->sliceType;
    if (output->sliceType == CNCODEC_SLICE_H264_SPS_PPS || output->sliceType == CNCODEC_SLICE_HEVC_VPS_SPS_PPS) {
      LOGI(VideoEncoderMlu) << "ReceivePacket() got parameter sets, size=" << output->streamLength;
      priv_->ps_buffer.reset(new (std::nothrow) uint8_t[output->streamLength]);
      priv_->ps_size = output->streamLength;
      auto ret = cnrtMemcpy(priv_->ps_buffer.get(),
                           reinterpret_cast<void *>(output->streamBuffer.addr + output->dataOffset),
                           output->streamLength, CNRT_MEM_TRANS_DIR_DEV2HOST);
      LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "ReceivePacket() cnrtMemcpy failed, error code: " << ret;
      cnvideoEncReleaseReference(reinterpret_cast<cnvideoEncoder>(priv_->cn_encoder), &(output->streamBuffer));
      return;
    } else if (output->sliceType == CNCODEC_SLICE_NALU_IDR || output->sliceType == CNCODEC_SLICE_NALU_I) {
      LOGT(VideoEncoderMlu) << "ReceivePacket() got key frame";
      packet.SetKey();
    }
    if (priv_->stream_buffer_size < output->streamLength) {
      priv_->stream_buffer.reset(new (std::nothrow) uint8_t[output->streamLength]);
      priv_->stream_buffer_size = output->streamLength;
    }
    auto ret = cnrtMemcpy(priv_->stream_buffer.get(),
                          reinterpret_cast<void *>(output->streamBuffer.addr + output->dataOffset),
                          output->streamLength, CNRT_MEM_TRANS_DIR_DEV2HOST);
    LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret) << "ReceivePacket() cnrtMemcpy failed, error code: " << ret;
    packet.data = priv_->stream_buffer.get();
    packet.size = output->streamLength;
    packet.pts = output->pts;
    packet.dts = INVALID_TIMESTAMP;
    cnvideoEncReleaseReference(reinterpret_cast<cnvideoEncoder>(priv_->cn_encoder), &(output->streamBuffer));
  }

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
    if (!eos) LOGE(VideoEncoderMlu) << "ReceivePacket() restore encoding info failed, index=" << index;
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

void VideoEncoderMlu200::ReceiveEOS() {
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

i32_t VideoEncoderMlu200::ErrorHandler(int event) {
  std::lock_guard<std::mutex> lk(cb_mtx_);
  switch (event) {
    case CNCODEC_CB_EVENT_SW_RESET:
    case CNCODEC_CB_EVENT_HW_RESET:
      LOGE(VideoEncoderMlu) << "ErrorHandler() firmware crash event: " << event;
      priv_->error = true;
      if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_ERROR);
      break;
    case CNCODEC_CB_EVENT_OUT_OF_MEMORY:
      LOGE(VideoEncoderMlu) << "ErrorHandler() out of memory error thrown from cncodec";
      priv_->error = true;
      if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_ERROR);
      break;
    case CNCODEC_CB_EVENT_ABORT_ERROR:
      LOGE(VideoEncoderMlu) << "ErrorHandler() abort error thrown from cncodec";
      priv_->error = true;
      if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_ERROR);
      break;
    default:
      LOGE(VideoEncoderMlu) << "ErrorHandler() unknown event: " << event;
      priv_->error = true;
      if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_ERROR);
      return -1;
  }
  return 0;
}

}  // namespace video

}  // namespace cnstream
