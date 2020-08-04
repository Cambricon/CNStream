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

#include <cn_codec_common.h>
#include <cn_jpeg_dec.h>
#include <cn_video_dec.h>
#include <cnrt.h>
#include <glog/logging.h>

#include <condition_variable>
#include <cstring>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "easycodec/easy_decode.h"
#include "format_info.h"

using std::mutex;
using std::string;
using std::to_string;
using std::unique_lock;

#define ALIGN(size, alignment) (((u32_t)(size) + (alignment)-1) & ~((alignment)-1))

#define CALL_CNRT_FUNC(func, msg)                                        \
  do {                                                                   \
    int ret = (func);                                                    \
    if (0 != ret) {                                                      \
      LOG(ERROR) << msg << " error code: " << ret;                       \
      throw EasyDecodeError(msg " error code : " + std::to_string(ret)); \
    }                                                                    \
  } while (0)

// cncodec add version macro since v1.6.0
#ifndef CNCODEC_VERSION
#define CNCODEC_VERSION 0
#endif

namespace edk {

static std::mutex g_vpu_instance_mutex;
/* static constexpr unsigned int g_decode_input_buffer_size = 4 << 20; */

static void PrintCreateAttr(cnvideoDecCreateInfo* p_attr) {
  printf("%-32s%s\n", "param", "value");
  printf("-------------------------------------\n");
  printf("%-32s%u\n", "Codectype", p_attr->codec);
  printf("%-32s%u\n", "Instance", p_attr->instance);
  printf("%-32s%u\n", "DeviceID", p_attr->deviceId);
  printf("%-32s%u\n", "MemoryAllocate", p_attr->allocType);
  printf("%-32s%u\n", "PixelFormat", p_attr->pixelFmt);
  printf("%-32s%u\n", "Progressive", p_attr->progressive);
  printf("%-32s%u\n", "Width", p_attr->width);
  printf("%-32s%u\n", "Height", p_attr->height);
  printf("%-32s%u\n", "BitDepthMinus8", p_attr->bitDepthMinus8);
  printf("%-32s%u\n", "InputBufferNum", p_attr->inputBufNum);
  printf("%-32s%u\n", "OutputBufferNum", p_attr->outputBufNum);
  printf("-------------------------------------\n");
}

static void PrintCreateAttr(cnjpegDecCreateInfo* p_attr) {
  printf("%-32s%s\n", "param", "value");
  printf("-------------------------------------\n");
  printf("%-32s%u\n", "Instance", p_attr->instance);
  printf("%-32s%u\n", "DeviceID", p_attr->deviceId);
  printf("%-32s%u\n", "MemoryAllocate", p_attr->allocType);
  printf("%-32s%u\n", "PixelFormat", p_attr->pixelFmt);
  printf("%-32s%u\n", "Width", p_attr->width);
  printf("%-32s%u\n", "Height", p_attr->height);
  printf("%-32s%u\n", "BitDepthMinus8", p_attr->bitDepthMinus8);
  printf("%-32s%u\n", "InputBufferNum", p_attr->inputBufNum);
  printf("%-32s%u\n", "OutputBufferNum", p_attr->outputBufNum);
  printf("%-32s%u\n", "InputBufferSize", p_attr->suggestedLibAllocBitStrmBufSize);
  printf("-------------------------------------\n");
}

class DecodeHandler {
 public:
  explicit DecodeHandler(EasyDecode* decoder);
  ~DecodeHandler();
  std::pair<bool, std::string> Init(const EasyDecode::Attr& attr);

  bool SendJpegData(const CnPacket& packet, bool eos);
  bool SendVideoData(const CnPacket& packet, bool eos, bool integral_frame);

  void AbortDecoder();

#ifdef ALLOC_BUFFER
  void AllocInputBuffer(cnvideoDecCreateInfo* params);
  void AllocOutputBuffer(cnvideoDecCreateInfo* params);
  void FreeInputBuffer(const cnvideoDecCreateInfo& params);
  void FreeOutputBuffer(const cnvideoDecCreateInfo& params);
#endif

  void ReceiveEvent(cncodecCbEventType type);
  void ReceiveFrame(void* out);
  int ReceiveSequence(cnvideoDecSequenceInfo* info);
  void ReceiveEOS();

  friend class EasyDecode;

 private:
  void EventTaskRunner();

  std::queue<cncodecCbEventType> event_queue_;
  std::mutex event_mtx_;
  std::condition_variable event_cond_;
  std::thread event_loop_;

  EasyDecode* decoder_ = nullptr;
  // cncodec handle
  void* handle_ = nullptr;

  EasyDecode::Attr attr_;
  cnvideoDecCreateInfo vparams_;
  cnjpegDecCreateInfo jparams_;
  const FormatInfo* pixel_fmt_info_;

  uint32_t packets_count_ = 0;
  uint32_t frames_count_ = 0;
  int minimum_buf_cnt_ = 0;

  EasyDecode::Status status_ = EasyDecode::Status::RUNNING;
  std::mutex status_mtx_;
  std::condition_variable status_cond_;

  /// eos workarround
  std::mutex eos_mtx_;
  std::condition_variable eos_cond_;
  bool send_eos_ = false;
  bool got_eos_ = false;
  bool jpeg_decode_ = false;
};  // class DecodeHandler

static i32_t EventHandler(cncodecCbEventType type, void* user_data, void* package) {
  auto handler = reinterpret_cast<DecodeHandler*>(user_data);
  // [ACQUIRED BY CNCODEC]
  // NEW_FRAME and SEQUENCE event must handled in callback thread,
  // The other events must handled in a different thread.
  if (handler != nullptr) {
    switch (type) {
      case CNCODEC_CB_EVENT_NEW_FRAME:
        handler->ReceiveFrame(package);
        break;
      case CNCODEC_CB_EVENT_SEQUENCE:
        handler->ReceiveSequence(reinterpret_cast<cnvideoDecSequenceInfo*>(package));
        break;
      default:
        handler->ReceiveEvent(type);
        break;
    }
  }
  return 0;
}

DecodeHandler::DecodeHandler(EasyDecode* decoder) : decoder_(decoder) {
  event_loop_ = std::thread(&DecodeHandler::EventTaskRunner, this);
}

DecodeHandler::~DecodeHandler() {
  /**
   * Decode destroied. status set to STOP.
   */
  unique_lock<mutex> status_lk(status_mtx_);
  status_ = EasyDecode::Status::STOP;
  status_lk.unlock();
  /**
   * Release resources.
   */
  unique_lock<mutex> eos_lk(eos_mtx_);
  if (!got_eos_) {
    if (!send_eos_ && handle_) {
      eos_mtx_.unlock();
      LOG(INFO) << "Send EOS in destruct";
      CnPacket packet;
      memset(&packet, 0, sizeof(CnPacket));
      decoder_->SendData(packet, true);
    } else {
      if (!handle_) got_eos_ = true;
    }
  }

  if (!eos_lk.owns_lock()) {
    eos_lk.lock();
  }

  if (!got_eos_) {
    LOG(INFO) << "Wait EOS in destruct";
    eos_cond_.wait(eos_lk, [this]() -> bool { return got_eos_; });
  }

  event_cond_.notify_all();
  event_loop_.join();

  if (handle_) {
    if (jpeg_decode_) {
      // Destroy jpu decoder
      LOG(INFO) << "Destroy jpeg decoder channel";
      auto ecode = cnjpegDecDestroy(handle_);
      if (CNCODEC_SUCCESS != ecode) {
        LOG(ERROR) << "Decoder destroy failed Error Code: " << ecode;
      }
    } else {
      // destroy vpu decoder
      LOG(INFO) << "Stop video decoder channel";
      auto ecode = cnvideoDecStop(handle_);
      if (CNCODEC_SUCCESS != ecode) {
        LOG(ERROR) << "Decoder stop failed Error Code: " << ecode;
      }

      LOG(INFO) << "Destroy video decoder channel";
      ecode = cnvideoDecDestroy(handle_);
      if (CNCODEC_SUCCESS != ecode) {
        LOG(ERROR) << "Decoder destroy failed Error Code: " << ecode;
      }
    }
    handle_ = nullptr;
  }

#ifdef ALLOC_BUFFER
  if (attr_.buf_strategy == BufferStrategy::EDK) {
    FreeInputBuffer(vparams_);
    FreeOutputBuffer(vparams_);
    delete[] vparams_.inputBuf;
    delete[] vparams_.outputBuf;
  }
#endif
}

void DecodeHandler::ReceiveEvent(cncodecCbEventType type) {
  std::lock_guard<std::mutex> lock(event_mtx_);
  event_queue_.push(type);
  event_cond_.notify_one();
}

void DecodeHandler::EventTaskRunner() {
  unique_lock<std::mutex> lock(event_mtx_);
  while (!event_queue_.empty() || !got_eos_) {
    event_cond_.wait(lock, [this] { return !event_queue_.empty() || got_eos_; });

    if (event_queue_.empty()) {
      // notified by eos
      continue;
    }

    cncodecCbEventType type = event_queue_.front();
    event_queue_.pop();
    lock.unlock();

    switch (type) {
      case CNCODEC_CB_EVENT_EOS:
        ReceiveEOS();
        break;
      case CNCODEC_CB_EVENT_SW_RESET:
      case CNCODEC_CB_EVENT_HW_RESET:
        LOG(ERROR) << "Decode firmware crash event: " << type;
        AbortDecoder();
        break;
      case CNCODEC_CB_EVENT_OUT_OF_MEMORY:
        LOG(ERROR) << "Out of memory error thrown from cncodec";
        AbortDecoder();
        break;
      case CNCODEC_CB_EVENT_ABORT_ERROR:
        LOG(ERROR) << "Abort error thrown from cncodec";
        AbortDecoder();
        break;
#if CNCODEC_VERSION >= 10600
      case CNCODEC_CB_EVENT_STREAM_CORRUPT:
        LOG(WARNING) << "Stream corrupt, discard frame";
        break;
#endif
      default:
        LOG(ERROR) << "Unknown event type";
        AbortDecoder();
        break;
    }

    lock.lock();
  }
}

void DecodeHandler::AbortDecoder() {
  LOG(WARNING) << "Abort decoder";
  if (handle_) {
    if (jpeg_decode_) {
      cnjpegDecAbort(handle_);
    } else {
      cnvideoDecAbort(handle_);
    }
    handle_ = nullptr;
    if (attr_.eos_callback) {
      attr_.eos_callback();
    }
    unique_lock<mutex> status_lk(status_mtx_);
    status_ = EasyDecode::Status::EOS;

    unique_lock<mutex> eos_lk(eos_mtx_);
    got_eos_ = true;
    eos_cond_.notify_one();
  } else {
    LOG(ERROR) << "Won't do abort, since cndecode handler has not been initialized";
  }
}

std::pair<bool, std::string> DecodeHandler::Init(const EasyDecode::Attr& attr) {
  attr_ = attr;
  // 1. decoder create parameters.
  jpeg_decode_ = attr.codec_type == CodecType::JPEG || attr.codec_type == CodecType::MJPEG;
  pixel_fmt_info_ = FormatInfo::GetFormatInfo(attr.pixel_format);
  if (jpeg_decode_) {
    memset(&jparams_, 0, sizeof(cnjpegDecCreateInfo));
    jparams_.deviceId = attr.dev_id;
    jparams_.instance = CNJPEGDEC_INSTANCE_AUTO;
    jparams_.pixelFmt = pixel_fmt_info_->cncodec_fmt;
    jparams_.colorSpace = CNCODEC_COLOR_SPACE_BT_709;
    jparams_.width = attr.frame_geometry.w;
    jparams_.height = attr.frame_geometry.h;
    jparams_.inputBufNum = attr.input_buffer_num;
    jparams_.outputBufNum = attr.output_buffer_num;
    jparams_.bitDepthMinus8 = 0;
    jparams_.allocType = CNCODEC_BUF_ALLOC_LIB;
    jparams_.userContext = reinterpret_cast<void*>(this);
    jparams_.suggestedLibAllocBitStrmBufSize = (4U << 20);
    jparams_.enablePreparse = 0;
    if (!attr.silent) {
      PrintCreateAttr(&jparams_);
    }
    int ecode = cnjpegDecCreate(&handle_, CNJPEGDEC_RUN_MODE_ASYNC, &EventHandler, &jparams_);
    if (0 != ecode) {
      return std::make_pair(false, "Create jpeg decode failed: " + to_string(ecode));
    }
  } else {
    memset(&vparams_, 0, sizeof(cnvideoDecCreateInfo));
    vparams_.deviceId = attr.dev_id;
    if (const char* turbo_env_p = std::getenv("VPU_TURBO_MODE")) {
      LOG(INFO) << "VPU Turbo mode : " << turbo_env_p;
      std::unique_lock<std::mutex> lk(g_vpu_instance_mutex);
      static int _vpu_inst_cnt = 0;
      static cnvideoDecInstance _instances[] = {
          // 100 channels:20+14+15+15+14+22
          CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_1, CNVIDEODEC_INSTANCE_2, CNVIDEODEC_INSTANCE_3,
          CNVIDEODEC_INSTANCE_4, CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_1,
          CNVIDEODEC_INSTANCE_2, CNVIDEODEC_INSTANCE_3, CNVIDEODEC_INSTANCE_4, CNVIDEODEC_INSTANCE_5,
          CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_1, CNVIDEODEC_INSTANCE_2, CNVIDEODEC_INSTANCE_3,
          CNVIDEODEC_INSTANCE_4, CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_1,
          CNVIDEODEC_INSTANCE_2, CNVIDEODEC_INSTANCE_3, CNVIDEODEC_INSTANCE_4, CNVIDEODEC_INSTANCE_5,
          CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_1, CNVIDEODEC_INSTANCE_2, CNVIDEODEC_INSTANCE_3,
          CNVIDEODEC_INSTANCE_4, CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_1,
          CNVIDEODEC_INSTANCE_2, CNVIDEODEC_INSTANCE_3, CNVIDEODEC_INSTANCE_4, CNVIDEODEC_INSTANCE_5,
          CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_1, CNVIDEODEC_INSTANCE_2, CNVIDEODEC_INSTANCE_3,
          CNVIDEODEC_INSTANCE_4, CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_1,
          CNVIDEODEC_INSTANCE_2, CNVIDEODEC_INSTANCE_3, CNVIDEODEC_INSTANCE_4, CNVIDEODEC_INSTANCE_5,
          CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_1, CNVIDEODEC_INSTANCE_2, CNVIDEODEC_INSTANCE_3,
          CNVIDEODEC_INSTANCE_4, CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_1,
          CNVIDEODEC_INSTANCE_2, CNVIDEODEC_INSTANCE_3, CNVIDEODEC_INSTANCE_4, CNVIDEODEC_INSTANCE_5,
          CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_1, CNVIDEODEC_INSTANCE_2, CNVIDEODEC_INSTANCE_3,
          CNVIDEODEC_INSTANCE_4, CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_1,
          CNVIDEODEC_INSTANCE_2, CNVIDEODEC_INSTANCE_3, CNVIDEODEC_INSTANCE_4, CNVIDEODEC_INSTANCE_5,
          CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_1, CNVIDEODEC_INSTANCE_2, CNVIDEODEC_INSTANCE_3,
          CNVIDEODEC_INSTANCE_4, CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_1,
          CNVIDEODEC_INSTANCE_3, CNVIDEODEC_INSTANCE_4, CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_0,
          CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_0,
          CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_0,
          CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_0, CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_3,
          CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_5, CNVIDEODEC_INSTANCE_2, CNVIDEODEC_INSTANCE_2};
      vparams_.instance = _instances[_vpu_inst_cnt++ % 100];
    } else {
      vparams_.instance = CNVIDEODEC_INSTANCE_AUTO;
    }
    vparams_.codec = CodecTypeCast(attr.codec_type);
    vparams_.pixelFmt = pixel_fmt_info_->cncodec_fmt;
    vparams_.colorSpace = ColorStdCast(attr.color_std);
    vparams_.width = attr.frame_geometry.w;
    vparams_.height = attr.frame_geometry.h;
    vparams_.bitDepthMinus8 = attr.pixel_format == PixelFmt::P010 ? 2 : 0;
    vparams_.progressive = attr.interlaced ? 0 : 1;
    vparams_.inputBufNum = attr.input_buffer_num;
    vparams_.outputBufNum = attr.output_buffer_num;
    vparams_.allocType = CNCODEC_BUF_ALLOC_LIB;
    vparams_.userContext = reinterpret_cast<void*>(this);

#ifdef ALLOC_BUFFER
    if (attr.buf_strategy == BufferStrategy::EDK) {
      vparams_.allocType = CNCODEC_BUF_ALLOC_APP;
      vparams_.inputBuf = new cncodecDevMemory[vparams_.inputBufNum];
      vparams_.outputBuf = new cncodecFrame[vparams_.outputBufNum];
      AllocInputBuffer(&vparams_);
      AllocOutputBuffer(&vparams_);
    }
#endif

    if (!attr.silent) {
      PrintCreateAttr(&vparams_);
    }

    int ecode = cnvideoDecCreate(&handle_, &EventHandler, &vparams_);
    if (0 != ecode) {
      return std::make_pair(false, "Create video decode failed: " + to_string(ecode));
    }

    int ret = cnvideoDecSetAttributes(handle_, CNVIDEO_DEC_ATTR_OUT_BUF_ALIGNMENT, &(attr_.stride_align));
    if (CNCODEC_SUCCESS != ret) {
      return std::make_pair(false, "cnvideo decode set attributes faild: " + to_string(ret));
    }
  }

  status_ = EasyDecode::Status::RUNNING;

  return std::make_pair(true, "Init succeed");
}

void DecodeHandler::ReceiveFrame(void* out) {
  // 1. handle decoder status
  if (EasyDecode::Status::PAUSED == status_) {
    unique_lock<mutex> status_lk(status_mtx_);
    status_cond_.wait(status_lk, [this]() -> bool { return EasyDecode::Status::RUNNING == status_; });
  }

  // 2. config CnFrame for user callback.
  CnFrame finfo;
  cncodecFrame* frame = nullptr;
  if (jpeg_decode_) {
    auto o = reinterpret_cast<cnjpegDecOutput*>(out);
    finfo.pts = o->pts;
    frame = &o->frame;
    VLOG(5) << "Receive one jpeg frame, " << frame;
  } else {
    auto o = reinterpret_cast<cnvideoDecOutput*>(out);
    finfo.pts = o->pts;
    frame = &o->frame;
    VLOG(5) << "Receive one video frame, " << frame;
  }
  if (frame->width == 0 || frame->height == 0 || frame->planeNum == 0) {
    LOG(WARNING) << "Receive empty frame";
    return;
  }
  finfo.device_id = attr_.dev_id;
  finfo.channel_id = frame->channel;
  finfo.buf_id = reinterpret_cast<uint64_t>(frame);
  finfo.width = frame->width;
  finfo.height = frame->height;
  finfo.n_planes = frame->planeNum;
  finfo.frame_size = 0;
  for (uint32_t pi = 0; pi < frame->planeNum; ++pi) {
    finfo.strides[pi] = frame->stride[pi];
    finfo.ptrs[pi] = reinterpret_cast<void*>(frame->plane[pi].addr);
    finfo.frame_size += pixel_fmt_info_->GetPlaneSize(frame->stride[pi], frame->height, pi);
  }
  finfo.pformat = attr_.pixel_format;
  finfo.color_std = attr_.color_std;

  VLOG(5) << "Frame: width " << finfo.width << " height " << finfo.height << " planes " << finfo.n_planes
          << " frame size " << finfo.frame_size;

  if (NULL != attr_.frame_callback) {
    VLOG(4) << "Add decode buffer Reference " << finfo.buf_id;
    if (jpeg_decode_) {
      cnjpegDecAddReference(handle_, frame);
    } else {
      cnvideoDecAddReference(handle_, frame);
    }
    attr_.frame_callback(finfo);
    frames_count_++;
  }
}

int DecodeHandler::ReceiveSequence(cnvideoDecSequenceInfo* info) {
  LOG(INFO) << "Receive sequence";

  vparams_.codec = info->codec;
  vparams_.pixelFmt = pixel_fmt_info_->cncodec_fmt;
  vparams_.width = info->width;
  vparams_.height = info->height;
  minimum_buf_cnt_ = info->minOutputBufNum;

  if (info->minInputBufNum > vparams_.inputBufNum) {
#ifdef ALLOC_BUFFER
    if (attr_.buf_strategy == BufferStrategy::EDK) {
      // release output buffer
      FreeInputBuffer(vparams_);
      delete[] vparams_.inputBuf;
      vparams_.inputBuf = new cncodecDevMemory[info->minInputBufNum];
      AllocInputBuffer(&vparams_);
    }
#endif
    vparams_.inputBufNum = info->minInputBufNum;
  }
  if (info->minOutputBufNum > vparams_.outputBufNum) {
#ifdef ALLOC_BUFFER
    if (attr_.buf_strategy == BufferStrategy::EDK) {
      FreeOutputBuffer(vparams_);
      delete[] vparams_.outputBuf;
      vparams_.outputBuf = new cncodecFrame[info->minOutputBufNum];
      AllocOutputBuffer(&vparams_);
    }
#endif
    vparams_.outputBufNum = info->minOutputBufNum;
  }

  vparams_.userContext = reinterpret_cast<void*>(this);

  int ecode = cnvideoDecStart(handle_, &vparams_);
  if (ecode != CNCODEC_SUCCESS) {
    LOG(ERROR) << "Start Decoder failed.";
    return -1;
  }
  return 0;
}

void DecodeHandler::ReceiveEOS() {
  LOG(INFO) << "Thread id: " << std::this_thread::get_id() << ",Received EOS from cncodec";

  if (attr_.eos_callback) {
    attr_.eos_callback();
  }
  unique_lock<mutex> status_lk(status_mtx_);
  status_ = EasyDecode::Status::EOS;

  unique_lock<mutex> eos_lk(eos_mtx_);
  got_eos_ = true;
  eos_cond_.notify_one();
}

bool DecodeHandler::SendJpegData(const CnPacket& packet, bool eos) {
  cnjpegDecInput input;
  if (packet.data != NULL && packet.length > 0) {
    memset(&input, 0, sizeof(cnjpegDecInput));
    input.streamBuffer = reinterpret_cast<u8_t*>(packet.data);
    input.streamLength = packet.length;
    input.pts = packet.pts;
    input.flags = CNJPEGDEC_FLAG_TIMESTAMP;
    VLOG(5) << "Feed stream info, data: " << input.streamBuffer << " ,length: " << input.streamLength
            << " ,pts: " << input.pts;

    auto ecode = cnjpegDecFeedData(handle_, &input, 10000);
    if (-CNCODEC_TIMEOUT == ecode) {
      LOG(ERROR) << "cnjpegDecFeedData timeout";
      return false;
    } else if (CNCODEC_SUCCESS != ecode) {
      throw EasyDecodeError("Send data failed. Error code: " + to_string(ecode));
    }

    packets_count_++;
  }

  if (eos) {
    unique_lock<mutex> eos_lk(eos_mtx_);
    input.streamBuffer = nullptr;
    input.streamLength = 0;
    input.pts = 0;
    input.flags = CNJPEGDEC_FLAG_EOS;
    LOG(INFO) << "Thread id: " << std::this_thread::get_id() << ",Feed EOS data";
    auto ecode = cnjpegDecFeedData(handle_, &input, 10000);
    if (-CNCODEC_TIMEOUT == ecode) {
      LOG(ERROR) << "cnjpegDecFeedData send EOS timeout";
      return false;
    } else if (CNCODEC_SUCCESS != ecode) {
      throw EasyDecodeError("Send EOS failed. Error code: " + to_string(ecode));
    }

    send_eos_ = true;
  }

  return true;
}

bool DecodeHandler::SendVideoData(const CnPacket& packet, bool eos, bool integral_frame) {
  cnvideoDecInput input;
  if (packet.data != NULL && packet.length > 0) {
    memset(&input, 0, sizeof(cnvideoDecInput));
    input.streamBuf = reinterpret_cast<u8_t*>(packet.data);
    input.streamLength = packet.length;
    input.pts = packet.pts;
    input.flags = CNVIDEODEC_FLAG_TIMESTAMP;
#if CNCODEC_VERSION >= 10600
    if (integral_frame) {
      input.flags |= CNVIDEODEC_FLAG_END_OF_FRAME;
    }
#endif
    VLOG(5) << "Feed stream info, data: " << input.streamBuf << " ,length: " << input.streamLength
            << " ,pts: " << input.pts;

    auto ecode = cnvideoDecFeedData(handle_, &input, 10000);
    if (-CNCODEC_TIMEOUT == ecode) {
      LOG(ERROR) << "cnvideoDecFeedData timeout";
      return false;
    } else if (CNCODEC_SUCCESS != ecode) {
      throw EasyDecodeError("Send data failed. Error code: " + to_string(ecode));
    }

    packets_count_++;
  }

  if (eos) {
    unique_lock<mutex> eos_lk(eos_mtx_);
    input.streamBuf = nullptr;
    input.streamLength = 0;
    input.pts = 0;
    input.flags = CNVIDEODEC_FLAG_EOS;
    LOG(INFO) << "Thread id: " << std::this_thread::get_id() << ",Feed EOS data";
    auto ecode = cnvideoDecFeedData(handle_, &input, 10000);
    if (-CNCODEC_TIMEOUT == ecode) {
      LOG(ERROR) << "cnvideoDecFeedData send EOS timeout";
      return false;
    } else if (CNCODEC_SUCCESS != ecode) {
      throw EasyDecodeError("Send EOS failed. Error code: " + to_string(ecode));
    }

    send_eos_ = true;
  }

  return true;
}

#ifdef ALLOC_BUFFER
void DecodeHandler::AllocInputBuffer(cnvideoDecCreateInfo* params) {
  LOG(INFO) << "Alloc Input Buffer";
  for (unsigned int i = 0; i < params->inputBufNum; i++) {
    CALL_CNRT_FUNC(cnrtMalloc(reinterpret_cast<void**>(&params->inputBuf[i].addr), g_decode_input_buffer_size),
                   "Malloc decode input buffer failed");
    params->inputBuf[i].size = g_decode_input_buffer_size;
  }
}

void DecodeHandler::AllocOutputBuffer(cnvideoDecCreateInfo* params) {
  LOG(INFO) << "Alloc Output Buffer";
  uint64_t size = 0;
  const unsigned int width = params->width;
  const unsigned int stride = ALIGN(width, 128);
  const unsigned int height = params->height;
  const unsigned int plane_num = pixel_fmt_info_->plane_num;

  for (unsigned int i = 0; i < params->outputBufNum; ++i) {
    for (unsigned int j = 0; j < plane_num; ++j) {
      // I420 Y plane align to 256
      if (params->pixelFmt == CNCODEC_PIX_FMT_I420 && plane_num == 0) {
        size = pixel_fmt_info_->GetPlaneSize(ALIGN(width, 256), height, j);
        params->outputBuf[i].stride[j] = ALIGN(width, 256);
      } else {
        size = pixel_fmt_info_->GetPlaneSize(stride, height, j);
        params->outputBuf[i].stride[j] = stride;
      }
      CALL_CNRT_FUNC(cnrtMalloc(reinterpret_cast<void**>(&params->outputBuf[i].plane[j].addr), size),
                     "Malloc decode output buffer failed");
      params->outputBuf[i].plane[j].size = size;
    }

    params->outputBuf[i].height = height;
    params->outputBuf[i].width = width;
    params->outputBuf[i].pixelFmt = params->pixelFmt;
    params->outputBuf[i].planeNum = plane_num;
    params->colorSpace = params->colorSpace;
  }
}

void DecodeHandler::FreeInputBuffer(const cnvideoDecCreateInfo& params) {
  LOG(INFO) << "Free Input Buffer";
  for (unsigned int i = 0; i < params.inputBufNum; ++i) {
    CALL_CNRT_FUNC(cnrtFree(reinterpret_cast<void*>(params.inputBuf[i].addr)), "Free decode input buffer failed");
  }
}

void DecodeHandler::FreeOutputBuffer(const cnvideoDecCreateInfo& params) {
  LOG(INFO) << "Free Output Buffer";
  for (unsigned int i = 0; i < params.outputBufNum; ++i) {
    for (unsigned int j = 0; j < params.outputBuf[i].planeNum; ++j) {
      CALL_CNRT_FUNC(cnrtFree(reinterpret_cast<void*>(params.outputBuf[i].plane[j].addr)),
                     "Free decode output buffer failed");
    }
  }
}
#endif

EasyDecode* EasyDecode::Create(const Attr& attr) {
  auto decoder = new EasyDecode();
  // init members
  decoder->handler_ = new DecodeHandler(decoder);

  // TODO
  /* CNCodecVersion version; */
  /* CN_Decode_GetVersion(&version); */
  /* printf("%-32s%d.%d\n", "CNCodec Version is", version.Major, version.Minor); */

  std::pair<bool, std::string> ret = decoder->handler_->Init(attr);
  if (!ret.first) {
    delete decoder->handler_;
    decoder->handler_ = nullptr;
    delete decoder;
    throw EasyDecodeError(ret.second);
  }

  return decoder;
}  // EasyDecode::Create

EasyDecode::EasyDecode() {}

EasyDecode::~EasyDecode() {
  if (handler_) {
    delete handler_;
    handler_ = nullptr;
  }
}

bool EasyDecode::Pause() {
  unique_lock<mutex> lock(handler_->status_mtx_);
  if (Status::RUNNING == handler_->status_) {
    handler_->status_ = Status::PAUSED;
    return true;
  }
  return false;
}

bool EasyDecode::Resume() {
  unique_lock<mutex> lock(handler_->status_mtx_);
  if (Status::PAUSED == handler_->status_) {
    handler_->status_ = Status::RUNNING;
    handler_->status_cond_.notify_all();
    return true;
  }
  return false;
}

void EasyDecode::AbortDecoder() { handler_->AbortDecoder(); }

EasyDecode::Status EasyDecode::GetStatus() const {
  unique_lock<mutex> lock(handler_->status_mtx_);
  return handler_->status_;
}

bool EasyDecode::SendData(const CnPacket& packet, bool eos, bool integral_frame) {
  if (!handler_->handle_) {
    LOG(ERROR) << "Decoder has not been init";
    return false;
  }
  if (handler_->send_eos_) {
    LOG(WARNING) << "EOS had been sent, won't feed data or EOS";
    return false;
  }
  // check status
  unique_lock<mutex> lock(handler_->status_mtx_);

  if (Status::PAUSED == handler_->status_) {
    handler_->status_cond_.wait(lock, [this]() -> bool { return Status::RUNNING == handler_->status_; });
  }

  if (packet.length == 0 && !eos) {
    LOG(ERROR) << "Packet length is equal to 0. The packet will not be sent.";
    return true;
  }

  bool ret = false;

  if (handler_->jpeg_decode_) {
    ret = handler_->SendJpegData(packet, eos);
  } else {
    ret = handler_->SendVideoData(packet, eos, integral_frame);
  }

  // timeout
  if (!ret) {
    lock.unlock();
    handler_->AbortDecoder();
    throw EasyDecodeError("cndecode timeout");
  }

  return ret;
}

void EasyDecode::ReleaseBuffer(uint64_t buf_id) {
  VLOG(4) << "Release decode buffer reference " << buf_id;
  if (handler_->jpeg_decode_) {
    cnjpegDecReleaseReference(handler_->handle_, reinterpret_cast<cncodecFrame*>(buf_id));
  } else {
    cnvideoDecReleaseReference(handler_->handle_, reinterpret_cast<cncodecFrame*>(buf_id));
  }
}

bool EasyDecode::CopyFrameD2H(void* dst, const CnFrame& frame) {
  if (!dst) {
    throw EasyDecodeError("CopyFrameD2H: destination is nullptr");
    return false;
  }
  auto odata = reinterpret_cast<uint8_t*>(dst);
  cncodecPixelFormat pixel_fmt;
  if (handler_->jpeg_decode_) {
    pixel_fmt = handler_->jparams_.pixelFmt;
  } else {
    pixel_fmt = handler_->vparams_.pixelFmt;
  }

  VLOG(5) << "Copy codec frame from device to host";
  VLOG(5) << "device address: (plane 0) " << frame.ptrs[0] << ", (plane 1) " << frame.ptrs[1];
  VLOG(5) << "host address: " << reinterpret_cast<int64_t>(odata);

  switch (pixel_fmt) {
    case CNCODEC_PIX_FMT_NV21:
    case CNCODEC_PIX_FMT_NV12: {
      size_t len_y = frame.strides[0] * frame.height;
      size_t len_uv = frame.strides[1] * frame.height / 2;
      CALL_CNRT_FUNC(cnrtMemcpy(reinterpret_cast<void*>(odata), frame.ptrs[0], len_y, CNRT_MEM_TRANS_DIR_DEV2HOST),
                     "Decode copy frame plane luminance failed.");
      CALL_CNRT_FUNC(
          cnrtMemcpy(reinterpret_cast<void*>(odata + len_y), frame.ptrs[1], len_uv, CNRT_MEM_TRANS_DIR_DEV2HOST),
          "Decode copy frame plane chroma failed.");
      break;
    }
    case CNCODEC_PIX_FMT_I420: {
      size_t len_y = frame.strides[0] * frame.height;
      size_t len_u = frame.strides[1] * frame.height / 2;
      size_t len_v = frame.strides[2] * frame.height / 2;
      CALL_CNRT_FUNC(cnrtMemcpy(reinterpret_cast<void*>(odata), frame.ptrs[0], len_y, CNRT_MEM_TRANS_DIR_DEV2HOST),
                     "Decode copy frame plane y failed.");
      CALL_CNRT_FUNC(
          cnrtMemcpy(reinterpret_cast<void*>(odata + len_y), frame.ptrs[1], len_u, CNRT_MEM_TRANS_DIR_DEV2HOST),
          "Decode copy frame plane u failed.");
      CALL_CNRT_FUNC(
          cnrtMemcpy(reinterpret_cast<void*>(odata + len_y + len_u), frame.ptrs[2], len_v, CNRT_MEM_TRANS_DIR_DEV2HOST),
          "Decode copy frame plane v failed.");
      break;
    }
    default:
      LOG(ERROR) << "don't support format: " << pixel_fmt;
      break;
  }

  return true;
}

EasyDecode::Attr EasyDecode::GetAttr() const { return handler_->attr_; }

int EasyDecode::GetMinimumOutputBufferCount() const { return handler_->minimum_buf_cnt_; }

}  // namespace edk
