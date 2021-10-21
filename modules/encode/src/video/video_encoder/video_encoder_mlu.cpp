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
#include <cstring>
#include <iostream>

#include "cnrt.h"
#include "cnstream_logging.hpp"
#include "video_encoder_mlu.hpp"

namespace cnstream {

namespace video {

static const char *pf_str[] = {"I420", "NV12", "NV21", "BGR", "RGB"};
static const char *ct_str[] = {"H264", "H265", "MPEG4", "JPEG"};

#define ENC_CNRT_CHECK(__EXPRESSION__)                                     \
  do {                                                                     \
    cnrtRet_t ret = (__EXPRESSION__);                                      \
    LOGF_IF(VideoEncoderMlu, CNRT_RET_SUCCESS != ret)                      \
        << "Call [" << #__EXPRESSION__ << "] failed, error code: " << ret; \
  } while (0)

#define CALL_CNRT_BY_CONTEXT(__EXPRESSION__, __DEV_ID__, __DDR_CHN__)          \
  do {                                                                         \
    int dev_id = (__DEV_ID__);                                                 \
    cnrtDev_t dev;                                                             \
    cnrtChannelType_t ddr_chn = static_cast<cnrtChannelType_t>((__DDR_CHN__)); \
    ENC_CNRT_CHECK(cnrtGetDeviceHandle(&dev, dev_id));                         \
    ENC_CNRT_CHECK(cnrtSetCurrentDevice(dev));                                 \
    if (ddr_chn >= 0) ENC_CNRT_CHECK(cnrtSetCurrentChannel(ddr_chn));          \
    ENC_CNRT_CHECK(__EXPRESSION__);                                            \
  } while (0)

#define CNCODEC_ALLOC_BITSTREAM_BUFFER_SIZE 0x400000
#define CNCODEC_PTS_MAX_VALUE (0xffffffffffffffffLL / 1000)

static inline int64_t CurrentTick() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

static i32_t EncoderEventCallback(cncodecCbEventType event, void *user_ctx, void *info) {
  VideoEncoderMlu *encoder = reinterpret_cast<VideoEncoderMlu *>(user_ctx);
  return encoder->EventHandler(event, info);
}

VideoEncoderMlu::VideoEncoderMlu(const VideoEncoder::Param &param) : VideoEncoderBase(param) {
  LOGI(VideoEncoderMlu) << "VideoEncoderMlu(" << param.width << "x" << param.height << ", "
                        << pf_str[param_.pixel_format] << ", " << ct_str[param_.codec_type] << ")";
  cnrtInit(0);
}

VideoEncoderMlu::~VideoEncoderMlu() { Stop(); }

int VideoEncoderMlu::Start() {
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
  param_.frame_rate = param_.frame_rate > 0 ? param_.frame_rate : 30;
  param_.frame_rate = param_.frame_rate < 120 ? param_.frame_rate : 120;
  param_.time_base = param_.time_base > 0 ? param_.time_base : 1000;

  if (param_.codec_type == VideoCodecType::JPEG) {
    memset(&je_param_, 0, sizeof(je_param_));
    je_param_.deviceId = param_.mlu_device_id;
    je_param_.instance = CNVIDEOENC_INSTANCE_AUTO;
    if (param_.pixel_format == VideoPixelFormat::NV12) {
      je_param_.pixelFmt = CNCODEC_PIX_FMT_NV12;
    } else if (param_.pixel_format == VideoPixelFormat::NV21) {
      je_param_.pixelFmt = CNCODEC_PIX_FMT_NV21;
    // }else if (param_.pixel_format == VideoPixelFormat::I420) {
    //   je_param_.pixelFmt = CNCODEC_PIX_FMT_I420;
    } else {
      LOGE(VideoEncoderMlu) << "Start() unsupported pixel format: " << pf_str[param_.pixel_format];
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_PARAMETERS;
    }
    je_param_.colorSpace = CNCODEC_COLOR_SPACE_BT_709;
    je_param_.width = param_.width;
    je_param_.height = param_.height;
    je_param_.inputBuf = nullptr;
    je_param_.outputBuf = nullptr;
    je_param_.inputBufNum = param_.input_buffer_count;
    je_param_.outputBufNum = 6;
    je_param_.allocType = CNCODEC_BUF_ALLOC_LIB;
    je_param_.userContext = reinterpret_cast<void *>(this);
    je_param_.suggestedLibAllocBitStrmBufSize = CNCODEC_ALLOC_BITSTREAM_BUFFER_SIZE;

    i32_t ret = cnjpegEncCreate(reinterpret_cast<cnjpegEncoder *>(&cn_encoder_), CNJPEGENC_RUN_MODE_ASYNC,
                                EncoderEventCallback, &je_param_);
    if (CNCODEC_SUCCESS != ret) {
      LOGE(VideoEncoderMlu) << "Start() cnjpegEncCreate failed, ret=" << ret;
      cn_encoder_ = nullptr;
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
    /*
    ret = cnjpegEncSetAttribute(reinterpret_cast<cnjpegEncoder *>(&cn_encoder_),
                                CNJPEGENC_ATTR_USER_PICTURE_QUALITY, (void *)&param_.jpeg_quality);
    if (ret != CNCODEC_SUCCESS) {
      LOGE(VideoEncoderMlu) << "Start() cnjpegEncSetAttribute(USER_PICTURE_QUALITY) failed, ret=" << ret;
      cn_encoder_ = nullptr;
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
    */
  } else {
    memset(&ve_param_, 0, sizeof(ve_param_));
    ve_param_.deviceId = param_.mlu_device_id;
    ve_param_.instance = CNVIDEOENC_INSTANCE_AUTO;
    if (param_.codec_type == VideoCodecType::H264) {
      ve_param_.codec = CNCODEC_H264;
    } else if (param_.codec_type == VideoCodecType::H265) {
      ve_param_.codec = CNCODEC_HEVC;
    } else {
      LOGE(VideoEncoderMlu) << "Start() unsupported codec type: " << ct_str[param_.codec_type];
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_PARAMETERS;
    }
    if (param_.pixel_format == VideoPixelFormat::NV12) {
      ve_param_.pixelFmt = CNCODEC_PIX_FMT_NV12;
    } else if (param_.pixel_format == VideoPixelFormat::NV21) {
      ve_param_.pixelFmt = CNCODEC_PIX_FMT_NV21;
    // } else if (param_.pixel_format == VideoPixelFormat::I420) {
    //   ve_param_.pixelFmt = CNCODEC_PIX_FMT_I420;
    } else {
      LOGE(VideoEncoderMlu) << "Start() unsupported pixel format: " << pf_str[param_.pixel_format];
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_PARAMETERS;
    }
    ve_param_.colorSpace = CNCODEC_COLOR_SPACE_BT_709;
    ve_param_.width = param_.width;
    ve_param_.height = param_.height;
    ve_param_.userContext = reinterpret_cast<void *>(this);
    ve_param_.inputBuf = nullptr;
    ve_param_.outputBuf = nullptr;
    ve_param_.inputBufNum = param_.input_buffer_count;
    ve_param_.outputBufNum = 6;
    ve_param_.allocType = CNCODEC_BUF_ALLOC_LIB;
    ve_param_.suggestedLibAllocBitStrmBufSize = CNCODEC_ALLOC_BITSTREAM_BUFFER_SIZE;

    ve_param_.rateCtrl.rcMode = CNVIDEOENC_RATE_CTRL_CBR;
    ve_param_.fpsNumerator = param_.frame_rate;
    ve_param_.fpsDenominator = 1;
    ve_param_.rateCtrl.targetBitrate = param_.bit_rate;
    ve_param_.rateCtrl.gopLength = param_.gop_size;

    if (ve_param_.codec == CNCODEC_H264) {
      ve_param_.uCfg.h264.profile = CNVIDEOENC_PROFILE_H264_HIGH;
      ve_param_.uCfg.h264.level = CNVIDEOENC_LEVEL_H264_41;
      ve_param_.uCfg.h264.insertSpsPpsWhenIDR = 1;
      ve_param_.uCfg.h264.IframeInterval = param_.gop_size;
      ve_param_.uCfg.h264.BFramesNum = 1;
      ve_param_.uCfg.h264.sliceMode = CNVIDEOENC_SLICE_MODE_SINGLE;
      ve_param_.uCfg.h264.gopType = CNVIDEOENC_GOP_TYPE_BIDIRECTIONAL;
      ve_param_.uCfg.h264.entropyMode = CNVIDEOENC_ENTROPY_MODE_CABAC;
    } else if (ve_param_.codec == CNCODEC_HEVC) {
      ve_param_.uCfg.h265.profile = CNVIDEOENC_PROFILE_H265_MAIN;
      ve_param_.uCfg.h265.level = CNVIDEOENC_LEVEL_H265_HIGH_41;
      ve_param_.uCfg.h265.insertSpsPpsWhenIDR = 1;
      ve_param_.uCfg.h265.IframeInterval = param_.gop_size;
      ve_param_.uCfg.h265.BFramesNum = 2;
      ve_param_.uCfg.h265.sliceMode = CNVIDEOENC_SLICE_MODE_SINGLE;
      ve_param_.uCfg.h265.gopType = CNVIDEOENC_GOP_TYPE_BIDIRECTIONAL;
    }

    i32_t ret = cnvideoEncCreate(reinterpret_cast<cnvideoEncoder *>(&cn_encoder_), EncoderEventCallback, &ve_param_);
    if (CNCODEC_SUCCESS != ret) {
      LOGE(VideoEncoderMlu) << "Start() cnvideoEncCreate failed, ret=" << ret;
      cn_encoder_ = nullptr;
      state_ = IDLE;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
  }

  state_ = RUNNING;
  return cnstream::VideoEncoder::SUCCESS;
}

int VideoEncoderMlu::Stop() {
  UniqueWriteLock slk(state_mtx_);
  if (state_ != RUNNING) {
    // LOGW(VideoEncoderMlu) << "Stop() state != RUNNING";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  state_ = STOPPING;
  slk.Unlock();

  std::unique_lock<std::mutex> lk(list_mtx_);
  if (param_.codec_type == VideoCodecType::JPEG) {
    if (!ji_list_.empty()) {
      LOGW(VideoEncoderMlu) << "Stop() " << ji_list_.size() << " frame buffers still outside";
      list_cv_.wait(lk, [this]() { return ji_list_.empty(); });
    }
  } else {
    if (!vi_list_.empty()) {
      LOGW(VideoEncoderMlu) << "Stop() " << vi_list_.size() << " frame buffers still outside";
      list_cv_.wait(lk, [this]() { return vi_list_.empty(); });
    }
  }
  lk.unlock();

  if (!error_) {
    // send and wait eos
    if (!eos_got_) {
      if (!eos_sent_) {
        LOGI(VideoEncoderMlu) << "Stop() send EOS";
        VideoFrame frame;
        memset(&frame, 0, sizeof(VideoFrame));
        frame.SetEOS();
        if (cnstream::VideoEncoder::SUCCESS != SendFrame(&frame)) {
          LOGE(VideoEncoderMlu) << "Stop() send EOS failed";
          state_ = RUNNING;
          return cnstream::VideoEncoder::ERROR_FAILED;
        }
        eos_sent_ = true;
      }
      std::unique_lock<std::mutex> eos_lk(eos_mtx_);
      if (false == eos_cv_.wait_for(eos_lk, std::chrono::seconds(10), [this]() { return eos_got_.load(); })) {
        LOGE(VideoEncoderMlu) << "Stop() wait EOS for 10s timeout";
        state_ = RUNNING;
        return cnstream::VideoEncoder::ERROR_TIMEOUT;
      }
    }
    // destroy cn encoder
    if (cn_encoder_) {
      i32_t ret;
      if (param_.codec_type == VideoCodecType::JPEG) {
        ret = cnjpegEncDestroy(reinterpret_cast<cnjpegEncoder>(cn_encoder_));
      } else {
        ret = cnvideoEncDestroy(reinterpret_cast<cnvideoEncoder>(cn_encoder_));
      }
      if (CNCODEC_SUCCESS != ret) {
        LOGE(VideoEncoderMlu) << "Stop() destroy cn_encoder failed, ret=" << ret;
      }
    }
  } else {
    // abort cn encoder
    if (cn_encoder_) {
      LOGE(VideoEncoderMlu) << "Stop() abort cn_encoder for error";
      i32_t ret;
      if (param_.codec_type == VideoCodecType::JPEG) {
        ret = cnjpegEncAbort(reinterpret_cast<cnjpegEncoder>(cn_encoder_));
      } else {
        ret = cnvideoEncAbort(reinterpret_cast<cnvideoEncoder>(cn_encoder_));
      }
      if (CNCODEC_SUCCESS != ret) {
        LOGE(VideoEncoderMlu) << "Stop() abort cn_encoder failed, ret=" << ret;
      }
    }
  }

  if (stream_buffer_) {
    delete[] stream_buffer_;
    stream_buffer_ = nullptr;
    stream_buffer_size_ = 0;
  }
  if (ps_buffer_) {
    delete[] ps_buffer_;
    ps_buffer_ = nullptr;
    ps_size_ = 0;
  }

  eos_sent_ = eos_got_ = false;

  state_ = IDLE;

  return cnstream::VideoEncoder::SUCCESS;
}

int VideoEncoderMlu::RequestFrameBuffer(VideoFrame *frame, int timeout_ms) {
  UniqueReadLock slk(state_mtx_);
  if (state_ != RUNNING) {
    LOGW(VideoEncoderMlu) << "RequestFrameBuffer() not running";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  if (error_) {
    slk.Unlock();
    LOGE(VideoEncoderMlu) << "RequestFrameBuffer() stop for error";
    return Stop();
  }
  if (eos_sent_) {
    LOGE(VideoEncoderMlu) << "RequestFrameBuffer() got EOS already";
    return cnstream::VideoEncoder::ERROR_FAILED;
  }
  if (!frame) return cnstream::VideoEncoder::ERROR_PARAMETERS;

  cnjpegEncInput je_input;
  cnvideoEncInput ve_input;
  cncodecFrame *input_frame = nullptr;
  if (param_.codec_type == VideoCodecType::JPEG) {
    memset(&je_input, 0, sizeof(cnjpegEncInput));
    input_frame = &je_input.frame;
    i32_t ret = cnjpegEncWaitAvailInputBuf(reinterpret_cast<cnjpegEncoder>(cn_encoder_), input_frame, timeout_ms);
    if (CNCODEC_TIMEOUT == ret) {
      LOGE(VideoEncoderMlu) << "RequestFrameBuffer() cnjpegEncWaitAvailInputBuf timeout";
      return cnstream::VideoEncoder::ERROR_TIMEOUT;
    } else if (CNCODEC_SUCCESS != ret) {
      LOGE(VideoEncoderMlu) << "RequestFrameBuffer() cnjpegEncWaitAvailInputBuf failed, ret=" << ret;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
  } else {
    memset(&ve_input, 0, sizeof(cnvideoEncInput));
    input_frame = &ve_input.frame;
    i32_t ret = cnvideoEncWaitAvailInputBuf(reinterpret_cast<cnvideoEncoder>(cn_encoder_), input_frame, timeout_ms);
    if (-CNCODEC_TIMEOUT == ret) {
      LOGE(VideoEncoderMlu) << "RequestFrameBuffer() cnvideoEncWaitAvailInputBuf timeout";
      return cnstream::VideoEncoder::ERROR_TIMEOUT;
    } else if (CNCODEC_SUCCESS != ret) {
      LOGE(VideoEncoderMlu) << "RequestFrameBuffer() cnvideoEncWaitAvailInputBuf failed, ret=" << ret;
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
  }

  memset(frame, 0, sizeof(VideoFrame));
  frame->width = input_frame->width;
  frame->height = input_frame->height;
  frame->data[0] = reinterpret_cast<uint8_t *>(input_frame->plane[0].addr);
  frame->stride[0] = input_frame->stride[0];
  frame->data[1] = reinterpret_cast<uint8_t *>(input_frame->plane[1].addr);
  frame->stride[1] = input_frame->stride[1];
  if (param_.pixel_format == VideoPixelFormat::I420) {
    frame->data[2] = reinterpret_cast<uint8_t *>(input_frame->plane[2].addr);
    frame->stride[2] = input_frame->stride[2];
  }
  frame->pixel_format = param_.pixel_format;
  frame->SetMluDeviceId(param_.mlu_device_id);
  frame->SetMluMemoryChannel(input_frame->channel);

  std::lock_guard<std::mutex> llk(list_mtx_);
  if (param_.codec_type == VideoCodecType::JPEG) {
    ji_list_.push_back(je_input);
  } else {
    vi_list_.push_back(ve_input);
  }

  return cnstream::VideoEncoder::SUCCESS;
}

int VideoEncoderMlu::SendFrame(const VideoFrame *frame, int timeout_ms) {
  UniqueReadLock slk(state_mtx_);
  if (state_ != RUNNING && !(state_ >= RUNNING && ((frame->HasEOS() && !frame->data[0]) || error_))) {
    LOGW(VideoEncoderMlu) << "SendFrame() not running";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  if (eos_sent_) {
    LOGE(VideoEncoderMlu) << "SendFrame() got EOS already";
    return cnstream::VideoEncoder::ERROR_FAILED;
  }
  if (!frame) return cnstream::VideoEncoder::ERROR_PARAMETERS;

  // if (!(frame->HasEOS()) && frame->data[0] != nullptr)
  //   LOGI(VideoEncoderMlu) << "SendFrame() pts=" << frame->pts;

  cnjpegEncInput je_input;
  cnvideoEncInput ve_input;
  bool is_back_frame = false;
  if (frame->IsMluMemory()) {
    std::unique_lock<std::mutex> lk(list_mtx_);
    if (param_.codec_type == VideoCodecType::JPEG) {
      if (!ji_list_.empty()) {
        auto input = std::find_if(ji_list_.begin(), ji_list_.end(), [frame, this](const cnjpegEncInput &input) {
          return ((param_.pixel_format == VideoPixelFormat::I420 &&
                   frame->data[0] == reinterpret_cast<uint8_t *>(input.frame.plane[0].addr) &&
                   frame->data[1] == reinterpret_cast<uint8_t *>(input.frame.plane[1].addr) &&
                   frame->data[2] == reinterpret_cast<uint8_t *>(input.frame.plane[2].addr)) ||
                  ((param_.pixel_format == VideoPixelFormat::NV12 || param_.pixel_format == VideoPixelFormat::NV21) &&
                   frame->data[0] == reinterpret_cast<uint8_t *>(input.frame.plane[0].addr) &&
                   frame->data[1] == reinterpret_cast<uint8_t *>(input.frame.plane[1].addr)));
        });
        if (input != ji_list_.end()) {
          je_input = *input;
          ji_list_.erase(input);
          is_back_frame = true;
          if (error_) {
            if (ji_list_.empty() && state_ == RUNNING) {
              lk.unlock();
              slk.Unlock();
              LOGE(VideoEncoderMlu) << "SendFrame() stop for error";
              return Stop();
            } else {
              lk.unlock();
              list_cv_.notify_all();
              return cnstream::VideoEncoder::ERROR_FAILED;
            }
          } else {
            list_cv_.notify_one();
          }
        }
      }
    } else {
      if (!vi_list_.empty()) {
        auto input = std::find_if(vi_list_.begin(), vi_list_.end(), [frame, this](const cnvideoEncInput &input) {
          return ((param_.pixel_format == VideoPixelFormat::I420 &&
                   frame->data[0] == reinterpret_cast<uint8_t *>(input.frame.plane[0].addr) &&
                   frame->data[1] == reinterpret_cast<uint8_t *>(input.frame.plane[1].addr) &&
                   frame->data[2] == reinterpret_cast<uint8_t *>(input.frame.plane[2].addr)) ||
                  ((param_.pixel_format == VideoPixelFormat::NV12 || param_.pixel_format == VideoPixelFormat::NV21) &&
                   frame->data[0] == reinterpret_cast<uint8_t *>(input.frame.plane[0].addr) &&
                   frame->data[1] == reinterpret_cast<uint8_t *>(input.frame.plane[1].addr)));
        });
        if (input != vi_list_.end()) {
          ve_input = *input;
          vi_list_.erase(input);
          is_back_frame = true;
          if (error_) {
            if (vi_list_.empty() && state_ == RUNNING) {
              lk.unlock();
              slk.Unlock();
              LOGE(VideoEncoderMlu) << "SendFrame() stop for error";
              return Stop();
            } else {
              lk.unlock();
              list_cv_.notify_all();
              return cnstream::VideoEncoder::ERROR_FAILED;
            }
          } else {
            list_cv_.notify_one();
          }
        }
      }
    }
    if (!is_back_frame) {
      LOGE(VideoEncoderMlu) << "SendFrame() frame in MLU memory is not requested from encoder";
      return cnstream::VideoEncoder::ERROR_FAILED;
    }
  } else {
    if (error_) {
      slk.Unlock();
      LOGE(VideoEncoderMlu) << "SendFrame() stop for error";
      return Stop();
    }
  }

  if ((!is_back_frame && frame->data[0] != nullptr) || (frame->HasEOS() && frame->data[0] == nullptr)) {
    cncodecFrame *input_frame = nullptr;
    if (param_.codec_type == VideoCodecType::JPEG) {
      memset(&je_input, 0, sizeof(cnjpegEncInput));
      input_frame = &je_input.frame;
      i32_t ret = cnjpegEncWaitAvailInputBuf(reinterpret_cast<cnjpegEncoder>(cn_encoder_), input_frame, timeout_ms);
      if (CNCODEC_TIMEOUT == ret) {
        LOGE(VideoEncoderMlu) << "SendFrame() cnjpegEncWaitAvailInputBuf timeout";
        return cnstream::VideoEncoder::ERROR_TIMEOUT;
      } else if (CNCODEC_SUCCESS != ret) {
        LOGE(VideoEncoderMlu) << "SendFrame() cnjpegEncWaitAvailInputBuf failed, ret=" << ret;
        return cnstream::VideoEncoder::ERROR_FAILED;
      }
    } else {
      memset(&ve_input, 0, sizeof(cnvideoEncInput));
      input_frame = &ve_input.frame;
      i32_t ret = cnvideoEncWaitAvailInputBuf(reinterpret_cast<cnvideoEncoder>(cn_encoder_), input_frame, timeout_ms);
      if (-CNCODEC_TIMEOUT == ret) {
        LOGE(VideoEncoderMlu) << "SendFrame() cnvideoEncWaitAvailInputBuf timeout";
        return cnstream::VideoEncoder::ERROR_TIMEOUT;
      } else if (CNCODEC_SUCCESS != ret) {
        LOGE(VideoEncoderMlu) << "SendFrame() cnvideoEncWaitAvailInputBuf failed, ret=" << ret;
        return cnstream::VideoEncoder::ERROR_FAILED;
      }
    }

    if (frame->data[0] != nullptr) {
      size_t copy_size;
      switch (param_.pixel_format) {
        case VideoPixelFormat::NV12:
        case VideoPixelFormat::NV21:
          input_frame->stride[0] = frame->stride[0];
          copy_size = frame->stride[0] * frame->height;
          CALL_CNRT_BY_CONTEXT(cnrtMemcpy(reinterpret_cast<void *>(input_frame->plane[0].addr), frame->data[0],
                                          copy_size, CNRT_MEM_TRANS_DIR_HOST2DEV),
                               param_.mlu_device_id, -1);
          input_frame->stride[1] = frame->stride[1];
          copy_size = frame->stride[1] * frame->height / 2;
          CALL_CNRT_BY_CONTEXT(cnrtMemcpy(reinterpret_cast<void *>(input_frame->plane[1].addr), frame->data[1],
                                          copy_size, CNRT_MEM_TRANS_DIR_HOST2DEV),
                               param_.mlu_device_id, -1);
          break;
        case VideoPixelFormat::I420:
          input_frame->stride[0] = frame->stride[0];
          copy_size = frame->stride[0] * frame->height;
          CALL_CNRT_BY_CONTEXT(cnrtMemcpy(reinterpret_cast<void *>(input_frame->plane[0].addr), frame->data[0],
                                          copy_size, CNRT_MEM_TRANS_DIR_HOST2DEV),
                               param_.mlu_device_id, -1);
          input_frame->stride[1] = frame->stride[1];
          copy_size = frame->stride[1] * frame->height / 2;
          CALL_CNRT_BY_CONTEXT(cnrtMemcpy(reinterpret_cast<void *>(input_frame->plane[1].addr), frame->data[1],
                                          copy_size, CNRT_MEM_TRANS_DIR_HOST2DEV),
                               param_.mlu_device_id, -1);
          input_frame->stride[2] = frame->stride[2];
          copy_size = frame->stride[2] * frame->height / 2;
          CALL_CNRT_BY_CONTEXT(cnrtMemcpy(reinterpret_cast<void *>(input_frame->plane[2].addr), frame->data[2],
                                          copy_size, CNRT_MEM_TRANS_DIR_HOST2DEV),
                               param_.mlu_device_id, -1);
          break;
        default:
          LOGE(VideoEncoderMlu) << "SendFrame() unsupported pixel format: " << param_.pixel_format;
          return cnstream::VideoEncoder::ERROR_FAILED;
      }
    }
  }

  u64_t pts = data_index_++ % CNCODEC_PTS_MAX_VALUE;
  if (frame->data[0] != nullptr) {
    std::lock_guard<std::mutex> lk(info_mtx_);
    int64_t frame_pts = frame->pts;
    if (frame_pts == INVALID_TIMESTAMP) {
      frame_pts = frame_count_ * param_.time_base / param_.frame_rate;
    }
    encoding_info_[pts] = (EncodingInfo){ frame_pts, frame->dts, CurrentTick(), 0 };
  }

  int ret = cnstream::VideoEncoder::SUCCESS;

  if (param_.codec_type == VideoCodecType::JPEG) {
    je_input.pts = pts;
    if (frame->HasEOS()) {
      je_input.flags |= CNJPEGENC_FLAG_EOS;
      if (!frame->data[0]) {
        je_input.flags |= CNJPEGENC_FLAG_INVALID;
        LOGI(VideoEncoderMlu) << "SendFrame() Send JPEG EOS Individually";
      } else {
        LOGI(VideoEncoderMlu) << "SendFrame() Send JPEG EOS with data";
      }
    } else {
      je_input.flags &= (~CNJPEGENC_FLAG_EOS);
    }
    cnjpegEncParameters params;
    memset(&params, 0, sizeof(cnjpegEncParameters));
    params.quality = param_.jpeg_quality;
    params.restartInterval = 0;
    i32_t cnret = cnjpegEncFeedFrame(reinterpret_cast<cnjpegEncoder>(cn_encoder_), &je_input, &params, timeout_ms);
    if (CNCODEC_TIMEOUT == cnret) {
      LOGE(VideoEncoderMlu) << "SendFrame() cnjpegEncFeedFrame timeout";
      ret = cnstream::VideoEncoder::ERROR_TIMEOUT;
    } else if (CNCODEC_SUCCESS != cnret) {
      LOGE(VideoEncoderMlu) << "SendFrame() cnjpegEncFeedFrame failed, ret=" << ret;
      ret = cnstream::VideoEncoder::ERROR_FAILED;
    }
  } else {
    ve_input.pts = pts;
    if (frame->HasEOS()) {
      ve_input.flags |= CNVIDEOENC_FLAG_EOS;
      if (!frame->data[0]) {
        ve_input.flags |= CNVIDEOENC_FLAG_INVALID_FRAME;
        LOGI(VideoEncoderMlu) << "SendFrame() Send Video EOS Individually";
      } else {
        LOGI(VideoEncoderMlu) << "SendFrame() Send Video EOS with data";
      }
    } else {
      ve_input.flags &= (~CNVIDEOENC_FLAG_EOS);
    }
    i32_t cnret = cnvideoEncFeedFrame(reinterpret_cast<cnvideoEncoder>(cn_encoder_), &ve_input, timeout_ms);
    if (-CNCODEC_TIMEOUT == cnret) {
      LOGE(VideoEncoderMlu) << "SendFrame() cnvideoEncFeedFrame timeout";
      ret = cnstream::VideoEncoder::ERROR_TIMEOUT;
    } else if (CNCODEC_SUCCESS != cnret) {
      LOGE(VideoEncoderMlu) << "SendFrame() cnvideoEncFeedFrame failed, ret=" << ret;
      ret = cnstream::VideoEncoder::ERROR_FAILED;
    }
  }

  if (ret == cnstream::VideoEncoder::SUCCESS) {
    if (frame->HasEOS()) eos_sent_ = true;
    if (frame->data[0] != nullptr) frame_count_++;
  } else {
    std::lock_guard<std::mutex> lk(info_mtx_);
    if (frame->data[0] != nullptr) encoding_info_.erase(pts);
  }

  return ret;
}

int VideoEncoderMlu::GetPacket(VideoPacket *packet, PacketInfo *info) {
  UniqueReadLock slk(state_mtx_);
  if (state_ != RUNNING) {
    LOGW(VideoEncoderMlu) << "GetPacket() not running";
    return cnstream::VideoEncoder::ERROR_STATE;
  }
  if (error_) {
    slk.Unlock();
    LOGE(VideoEncoderMlu) << "GetPacket() stop for error";
    return Stop();
  }
  return VideoEncoderBase::GetPacket(packet, info);
}

bool VideoEncoderMlu::GetPacketInfo(int64_t pts, PacketInfo *info) {
  if (!info) return false;

  std::lock_guard<std::mutex> lk(info_mtx_);
  for (auto info_it = encoding_info_.begin(); info_it != encoding_info_.end(); ++info_it) {
    if (info_it->second.pts == pts) {
      info->start_tick = info_it->second.start_tick;
      info->end_tick = info_it->second.end_tick;
      encoding_info_.erase(info_it);
      return true;
    }
  }
  return false;
}

i32_t VideoEncoderMlu::EventHandler(cncodecCbEventType event, void *info) {
  switch (event) {
    case CNCODEC_CB_EVENT_NEW_FRAME:
      ReceivePacket(info);
      break;
    case CNCODEC_CB_EVENT_EOS:
      ReceiveEOS();
      break;
    default:
      return ErrorHandler(event);
  }
  return 0;
}

void VideoEncoderMlu::ReceivePacket(void *info) {
  ReadLockGuard slk(state_mtx_);
  if (state_ != RUNNING) {
    LOGW(VideoEncoderMlu) << "ReceivePacket() not running";
    return;
  }

  VideoPacket packet;
  bool eos = false;
  memset(&packet, 0, sizeof(VideoPacket));
  if (param_.codec_type == VideoCodecType::JPEG) {
    cnjpegEncOutput *output = reinterpret_cast<cnjpegEncOutput *>(info);
    // LOGI(VideoEncoderMlu) << "ReceiveJPEGPacket size=" << output->streamLength << ", pts=" << output->pts;
    if (stream_buffer_size_ < output->streamLength) {
      if (stream_buffer_) delete[] stream_buffer_;
      stream_buffer_ = new (std::nothrow) uint8_t[output->streamLength];
      stream_buffer_size_ = output->streamLength;
    }
    CALL_CNRT_BY_CONTEXT(
        cnrtMemcpy(stream_buffer_, reinterpret_cast<void *>(output->streamBuffer.addr + output->dataOffset),
                   output->streamLength, CNRT_MEM_TRANS_DIR_DEV2HOST),
        param_.mlu_device_id, -1);
    packet.data = stream_buffer_;
    packet.size = output->streamLength;
    packet.pts = output->pts;
    packet.dts = INVALID_TIMESTAMP;
    eos = output->flags & CNJPEGENC_FLAG_EOS;
  } else {
    cnvideoEncOutput *output = reinterpret_cast<cnvideoEncOutput *>(info);
    // LOGI(VideoEncoderMlu) << "ReceiveVideoPacket size=" << output->streamLength << ", pts=" << output->pts <<
    //     ", type=" << output->sliceType;
    if (output->sliceType == CNCODEC_SLICE_H264_SPS_PPS || output->sliceType == CNCODEC_SLICE_HEVC_VPS_SPS_PPS) {
      LOGI(VideoEncoderMlu) << "ReceivePacket() Got parameter sets, size=" << output->streamLength;
      if (ps_buffer_) delete[] ps_buffer_;
      ps_buffer_ = new (std::nothrow) uint8_t[output->streamLength];
      ps_size_ = output->streamLength;
      CALL_CNRT_BY_CONTEXT(
          cnrtMemcpy(ps_buffer_, reinterpret_cast<void *>(output->streamBuffer.addr + output->dataOffset),
                     output->streamLength, CNRT_MEM_TRANS_DIR_DEV2HOST),
          param_.mlu_device_id, -1);
      return;
    } else if (output->sliceType == CNCODEC_SLICE_NALU_IDR || output->sliceType == CNCODEC_SLICE_NALU_I) {
      // LOGI(VideoEncoderMlu) << "ReceivePacket() Got key frame";
      packet.SetKey();
    }
    if (stream_buffer_size_ < output->streamLength) {
      if (output->streamLength) delete[] stream_buffer_;
      stream_buffer_ = new (std::nothrow) uint8_t[output->streamLength];
      stream_buffer_size_ = output->streamLength;
    }
    CALL_CNRT_BY_CONTEXT(
        cnrtMemcpy(stream_buffer_, reinterpret_cast<void *>(output->streamBuffer.addr + output->dataOffset),
                   output->streamLength, CNRT_MEM_TRANS_DIR_DEV2HOST),
        param_.mlu_device_id, -1);
    packet.data = stream_buffer_;
    packet.size = output->streamLength;
    packet.pts = output->pts;
    packet.dts = INVALID_TIMESTAMP;
  }
  // find out packet and update encoding info
  std::unique_lock<std::mutex> ilk(info_mtx_);
  int64_t index = packet.pts;
  auto it_info = encoding_info_.find(index);
  if (it_info != encoding_info_.end()) {
    it_info->second.end_tick = CurrentTick();
    packet.pts = it_info->second.pts;
    if (it_info->second.dts == INVALID_TIMESTAMP) {
      packet.dts = (packet_count_ - 2) * param_.time_base / param_.frame_rate;
    } else {
      packet.dts = it_info->second.dts;
    }
  } else {
    if (eos) {
      return;
    } else {
      LOGW(VideoEncoderMlu) << "ReceivePacket() restore encoding info failed, index=" << index;
    }
  }
  ilk.unlock();
  // LOGI(VideoEncoderMlu) << "ReceivePacket() got packet, size=" << packet.size << ", pts=" << packet.pts <<
  //     ", dts=" << packet.dts;
  PushBuffer(&packet);
  packet_count_++;
  std::lock_guard<std::mutex> lk(cb_mtx_);
  if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_DATA);
}

void VideoEncoderMlu::ReceiveEOS() {
  if (state_ < RUNNING) {
    LOGW(VideoEncoderMlu) << "ReceiveEOS() not running";
    return;
  }
  LOGI(VideoEncoderMlu) << "ReceiveEOS()";
  std::unique_lock<std::mutex> eos_lk(eos_mtx_);
  eos_got_ = true;
  eos_lk.unlock();
  eos_cv_.notify_one();
  std::lock_guard<std::mutex> lk(cb_mtx_);
  if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_EOS);
}

i32_t VideoEncoderMlu::ErrorHandler(cncodecCbEventType event) {
  std::lock_guard<std::mutex> lk(cb_mtx_);
  switch (event) {
    case CNCODEC_CB_EVENT_SW_RESET:
    case CNCODEC_CB_EVENT_HW_RESET:
      LOGE(VideoEncoderMlu) << "ErrorHandler() firmware crash event: " << event;
      error_ = true;
      if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_ERROR);
      break;
    case CNCODEC_CB_EVENT_OUT_OF_MEMORY:
      LOGE(VideoEncoderMlu) << "ErrorHandler() out of memory error thrown from cncodec";
      error_ = true;
      if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_ERROR);
      break;
    case CNCODEC_CB_EVENT_ABORT_ERROR:
      LOGE(VideoEncoderMlu) << "ErrorHandler() abort error thrown from cncodec";
      error_ = true;
      if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_ERROR);
      break;
    default:
      LOGE(VideoEncoderMlu) << "ErrorHandler() unknown event: " << event;
      error_ = true;
      if (event_callback_) event_callback_(cnstream::VideoEncoder::EVENT_ERROR);
      return -1;
  }
  return 0;
}

}  // namespace video

}  // namespace cnstream
