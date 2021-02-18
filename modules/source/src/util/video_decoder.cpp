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

#include "video_decoder.hpp"

#include "cnrt.h"
#include "cn_jpeg_dec.h"
#include "cn_video_dec.h"

#include "cnstream_common.hpp"
#include "cnstream_logging.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <utility>

namespace cnstream {

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

// FFMPEG use AVCodecParameters instead of AVCodecContext
// since from version 3.1(libavformat/version:57.40.100)
#define FFMPEG_VERSION_3_1 AV_VERSION_INT(57, 40, 100)

namespace detail {
class SpinLock {
 public:
  void lock() {
    while (lock_.test_and_set(std::memory_order_acquire)) {
    }  // spin
  }
  void unlock() { lock_.clear(std::memory_order_release); }

 private:
  std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
};

class SpinLockGuard {
 public:
  explicit SpinLockGuard(SpinLock& lock) : lock_(lock) { lock_.lock(); }
  ~SpinLockGuard() { lock_.unlock(); }

 private:
  SpinLock& lock_;
};
}  // namespace detail

class MluDecoderImpl {
 public:
  explicit MluDecoderImpl(IDecodeResult *cb) :result_ (cb) {}
  ~MluDecoderImpl() = default;  
  bool Create(VideoInfo *info, ExtraDecoderInfo *extra);
  void Destroy();
  bool Process(VideoEsPacket *pkt);  

 public:
  bool CreateVideoDecoder(VideoInfo *info, ExtraDecoderInfo *extra);
  void DestroyVideoDecoder();
  void SequenceCallback(cnvideoDecSequenceInfo *pFormat);
  void VideoFrameCallback(cnvideoDecOutput *dec_output);
  void CorruptCallback(const cnvideoDecStreamCorruptInfo &streamcorruptinfo);
  void VideoEosCallback();
  void VideoResetCallback();

  bool CreateJpegDecoder(VideoInfo *info, ExtraDecoderInfo *extra);
  void DestroyJpegDecoder();
  void JpegFrameCallback(cnjpegDecOutput *dec_output);
  void JpegEosCallback();
  void JpegResetCallback();

 private:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  int ProcessFrame(cnvideoDecOutput *output);
  int ProcessJpegFrame(cnjpegDecOutput *output);
  
 private:
  std::atomic<int> cndec_start_flag_{0};
  std::atomic<int> cndec_error_flag_{0};
  std::atomic<int> cndec_abort_flag_{0};
  std::atomic<int> eos_got_{0};
  std::atomic<int> cndec_buf_ref_count_{0};
  std::atomic<int> eos_sent_{0};  // flag for cndec-eos has been sent to decoder
  IDecodeResult *result_;
  // cnvideo
  cnvideoDecCreateInfo create_info_;
  cnvideoDecoder instance_ = nullptr;
  class CNDeallocator : public IDecBufRef {
   public:
    explicit CNDeallocator(MluDecoderImpl *decoder, cncodecFrame *frame) : decoder_(decoder), frame_(frame) {
      ++decoder_->cndec_buf_ref_count_;
    }
    ~CNDeallocator() {
      if (decoder_->instance_) {
        cnvideoDecReleaseReference(decoder_->instance_, frame_);
        --decoder_->cndec_buf_ref_count_;
      }
    }

   private:
    MluDecoderImpl *decoder_;
    cncodecFrame *frame_;
  };

  // For m200 vpu-decoder, m200 vpu-codec does not 64bits timestamp, we have to implement it.
  uint32_t pts_key_ = 0;
  std::unordered_map<uint32_t, uint64_t> vpu_pts_map_;
  detail::SpinLock map_lock_;
  uint32_t SetVpuTimestamp(uint64_t pts) {
    detail::SpinLockGuard guard(map_lock_);
    uint32_t key = pts_key_++;
    vpu_pts_map_[key] = pts;
    return key;
  }
  bool GetVpuTimestamp(uint32_t key, uint64_t *pts) {
    if (!pts) return false;
    detail::SpinLockGuard guard(map_lock_);
    auto iter = vpu_pts_map_.find(key);
    if (iter != vpu_pts_map_.end()) {
      *pts = iter->second;
      vpu_pts_map_.erase(iter);
      return true;
    }
    return false;
  }

  // cnjpeg
  cnjpegDecCreateInfo create_jpg_info_;
  cnjpegDecoder jpg_instance_ = nullptr;
  class CNDeallocatorJpg : public IDecBufRef {
   public:
    explicit CNDeallocatorJpg(MluDecoderImpl *decoder, cncodecFrame *frame) : decoder_(decoder), frame_(frame) {
      ++decoder_->cndec_buf_ref_count_;
    }
    ~CNDeallocatorJpg() {
      if (decoder_->jpg_instance_) {
        cnjpegDecReleaseReference(decoder_->jpg_instance_, frame_);
        --decoder_->cndec_buf_ref_count_;
      }
    }

   private:
    MluDecoderImpl *decoder_;
    cncodecFrame *frame_;
  };
  std::mutex instance_mutex_;
  VideoInfo info_;
  ExtraDecoderInfo extra_;
};

MluDecoder::MluDecoder(IDecodeResult *cb) : Decoder(cb) {
  impl_ = new MluDecoderImpl(cb);
}

MluDecoder::~MluDecoder() {
  if (impl_) delete impl_, impl_ = nullptr;
}

bool MluDecoder:: Create(VideoInfo *info, ExtraDecoderInfo *extra) {
  if (impl_) {
    return impl_->Create(info, extra);
  }
  return false;
}

void MluDecoder::Destroy() {
  if (impl_) {
    impl_->Destroy();
  }
}

bool MluDecoder::Process(VideoEsPacket *pkt) {
  if (impl_) {
    return impl_->Process(pkt);
  }
  return false;
}

bool MluDecoderImpl::Create(VideoInfo *info, ExtraDecoderInfo *extra) {
  // create decoder
  if (info->codec_id == AV_CODEC_ID_MJPEG) {
    if (CreateJpegDecoder(info, extra) != true) {
      return false;
    }
  } else {
    if (CreateVideoDecoder(info, extra) != true) {
      return false;
    }
  }
  info_ = *info;
  if (extra) extra_ = *extra;
  return true;
}

void MluDecoderImpl::Destroy() {
  if (instance_) {
    if (!cndec_abort_flag_.load()) {
      DestroyVideoDecoder();
    } else {
      // make sure all cndec buffers released before abort
      while (this->cndec_buf_ref_count_.load()) {
        std::this_thread::yield();
      }
      std::lock_guard<std::mutex> lk(instance_mutex_);
      cnvideoDecAbort(instance_);
      instance_ = nullptr;
      if (result_) {
        result_->OnDecodeEos();
      }
    }
  }

  if (jpg_instance_) {
    if (!cndec_abort_flag_.load()) {
      DestroyJpegDecoder();
    } else {
      // make sure all cndec buffers released before abort
      while (this->cndec_buf_ref_count_.load()) {
        std::this_thread::yield();
      }
      std::lock_guard<std::mutex> lk(instance_mutex_);
      cnjpegDecAbort(jpg_instance_);
      jpg_instance_ = nullptr;
      if (result_) {
        result_->OnDecodeEos();
      }
    }
  }
}

bool MluDecoderImpl::Process(VideoEsPacket *pkt) {
  if (cndec_abort_flag_.load() || cndec_error_flag_.load()) {
    LOGE(SOURCE) << "cndec_abort_flag_.load() || cndec_error_flag_.load()";
    return false;
  }
  if (instance_) {
    cnvideoDecInput input;
    memset(&input, 0, sizeof(cnvideoDecInput));
    if (pkt && pkt->data && pkt->len) {
      input.streamBuf = pkt->data;
      input.streamLength = pkt->len;
      input.pts = SetVpuTimestamp(pkt->pts);
      input.flags |= CNVIDEODEC_FLAG_TIMESTAMP;
      input.flags |= CNVIDEODEC_FLAG_END_OF_FRAME;
      if (input.streamLength > create_info_.suggestedLibAllocBitStrmBufSize) {
        LOGW(SOURCE) << "cnvideoDecFeedData- truncate " << input.streamLength << " to "
                      << create_info_.suggestedLibAllocBitStrmBufSize;
        input.streamLength = create_info_.suggestedLibAllocBitStrmBufSize;
      }
    } else {
      input.flags |= CNVIDEODEC_FLAG_EOS;
      eos_sent_.store(1);
    }

    /*
     * eos frame don't retry if feed timeout
     */
    if (input.flags & CNVIDEODEC_FLAG_EOS) {
      int ret = cnvideoDecFeedData(instance_, &input, 10000);  // FIXME
      if (-CNCODEC_TIMEOUT == ret) {
        LOGE(SOURCE) << "cnvideoDecFeedData(eos) timeout happened";
        cndec_abort_flag_ = 1;
        return false;
      } else if (CNCODEC_SUCCESS != ret) {
        LOGE(SOURCE) << "Call cnvideoDecFeedData failed, ret = " << ret;
        cndec_error_flag_ = 1;
        return false;
      } else {
        return true;
      }
    } else {
      int retry_time = 3;  // FIXME
      while (retry_time) {
        int ret = cnvideoDecFeedData(instance_, &input, 10000);  // FIXME
        if (-CNCODEC_TIMEOUT == ret) {
          retry_time--;
          LOGI(SOURCE) << "cnvideoDecFeedData(data) timeout happened, retry feed data, time: " << 3 - retry_time;
          continue;
        } else if (CNCODEC_SUCCESS != ret) {
          LOGE(SOURCE) << "Call cnvideoDecFeedData(data) failed, ret = " << ret;
          GetVpuTimestamp(input.pts, nullptr);  // Failed to feeddata, erase record
          cndec_error_flag_ = 1;
          return false;
        } else {
          return true;
        }
      }
      GetVpuTimestamp(input.pts, nullptr);  // Failed to feeddata, erase record

      if (0 == retry_time) {
        LOGI(SOURCE) << "cnvideoDecFeedData(data) timeout 3 times, prepare abort decoder.";
        // don't processframe
        cndec_abort_flag_ = 1;
        return false;
      }
    }
  }

  if (jpg_instance_) {
    cnjpegDecInput input;
    memset(&input, 0, sizeof(cnjpegDecInput));
    if (pkt && pkt->data && pkt->len) {
      input.streamBuffer = pkt->data;
      input.streamLength = pkt->len;
      input.pts = pkt->pts;
      input.flags |= CNJPEGDEC_FLAG_TIMESTAMP;
      if (input.streamLength > create_jpg_info_.suggestedLibAllocBitStrmBufSize) {
        LOGW(SOURCE) << "cnjpegDecFeedData- truncate " << input.streamLength << " to "
                      << create_jpg_info_.suggestedLibAllocBitStrmBufSize;
        input.streamLength = create_jpg_info_.suggestedLibAllocBitStrmBufSize;
      }
    } else {
      input.flags |= CNJPEGDEC_FLAG_EOS;
      eos_sent_.store(1);
    }

    /*
     * eos frame don't retry if feed timeout
     */
    if (input.flags & CNJPEGDEC_FLAG_EOS) {
      int ret = cnjpegDecFeedData(jpg_instance_, &input, 10000);  // FIXME
      if (CNCODEC_TIMEOUT == ret) {
        LOGE(SOURCE) << "cnjpegDecFeedData(eos) timeout happened";
        cndec_abort_flag_ = 1;
        return false;
      } else if (CNCODEC_SUCCESS != ret) {
        LOGE(SOURCE) << "Call cnjpegDecFeedData(eos) failed, ret = " << ret;
        cndec_error_flag_ = 1;
        return false;
      } else {
        return true;
      }
    } else {
      int retry_time = 3;  // FIXME
      while (retry_time) {
        int ret = cnjpegDecFeedData(jpg_instance_, &input, 10000);  // FIXME
        if (CNCODEC_TIMEOUT == ret) {
          retry_time--;
          LOGI(SOURCE) << "cnjpegDecFeedData(data) timeout happened, retry feed data, time: " << 3 - retry_time;
          continue;
        } else if (CNCODEC_SUCCESS != ret) {
          LOGE(SOURCE) << "Call cnjpegDecFeedData(data) failed, ret = " << ret;
          cndec_error_flag_ = 1;
          return false;
        } else {
          return true;
        }
      }
      if (0 == retry_time) {
        LOGI(SOURCE) << "cnjpegDecFeedData(data) timeout 3 times, prepare abort decoder.";
        // don't processframe
        cndec_abort_flag_ = 1;
        return false;
      }
    }
  }

  // should not come here...
  return false;
}

//
// video decoder implementation
//
static int VideoDecodeCallback(cncodecCbEventType EventType, void *pData, void *pdata1) {
  MluDecoderImpl *pThis = reinterpret_cast<MluDecoderImpl *>(pData);
  switch (EventType) {
    case CNCODEC_CB_EVENT_NEW_FRAME:
      pThis->VideoFrameCallback(reinterpret_cast<cnvideoDecOutput *>(pdata1));
      break;
    case CNCODEC_CB_EVENT_SEQUENCE:
      pThis->SequenceCallback(reinterpret_cast<cnvideoDecSequenceInfo *>(pdata1));
      break;
    case CNCODEC_CB_EVENT_EOS:
      pThis->VideoEosCallback();
      break;
    case CNCODEC_CB_EVENT_SW_RESET:
    case CNCODEC_CB_EVENT_HW_RESET:
      // todo....
      LOGE(SOURCE) << "Decode Firmware crash Event Event: " << EventType;
      pThis->VideoResetCallback();
      break;
    case CNCODEC_CB_EVENT_OUT_OF_MEMORY:
      LOGE(SOURCE) << "Decode out of memory, force stop";
      pThis->VideoEosCallback();
      break;
    case CNCODEC_CB_EVENT_ABORT_ERROR:
      LOGE(SOURCE) << "Decode abort error occured, force stop";
      pThis->VideoEosCallback();
      break;
    case CNCODEC_CB_EVENT_STREAM_CORRUPT:
      LOGW(SOURCE) << "Stream corrupt, discard frame";
      pThis->CorruptCallback(*reinterpret_cast<cnvideoDecStreamCorruptInfo *>(pdata1));
      break;
    default:
      LOGE(SOURCE) << "Unsupported Decode Event: " << EventType;
      break;
  }
  return 0;
}

void MluDecoderImpl::SequenceCallback(cnvideoDecSequenceInfo *pFormat) {
  /*update decode info*/

  // FIXME, check to reset decoder ...
  create_info_.codec = pFormat->codec;
  create_info_.height = pFormat->height;
  create_info_.width = pFormat->width;

  uint32_t out_buf_num = extra_.output_buf_num;  
  if (out_buf_num > pFormat->minOutputBufNum) {
    create_info_.outputBufNum = out_buf_num;
  } else {
    create_info_.outputBufNum = pFormat->minOutputBufNum + 1;
  }
  if (create_info_.outputBufNum > 32) {
    create_info_.outputBufNum = 32;
  }
  /*start decode*/
  // LOGI(SOURCE) << " cnvideoDecStart called, " << instance_  << "\n";
  int ret = cnvideoDecStart(instance_, &create_info_);
  if (CNCODEC_SUCCESS != ret) {
    LOGE(SOURCE) << "Call cnvideoDecStart failed, ret = " << ret;
    if (result_) {
      result_->OnDecodeError(ERROR_FAILED_TO_START);
    }
    return;
  }
  // LOGI(SOURCE) << " cnvideoDecStart called done, " << instance_  << "\n";
  cndec_start_flag_.store(1);
}


void MluDecoderImpl::CorruptCallback(const cnvideoDecStreamCorruptInfo &streamcorruptinfo) {
  LOGW(SOURCE) << "Skip frame number: " << streamcorruptinfo.frameNumber
               << ", frame count: " << streamcorruptinfo.frameCount
               << ", " << instance_  << "\n";
  #define CORRUPT_PTS_CNCODEC_VERSION 10800
  #if CNCODEC_VERSION >= CORRUPT_PTS_CNCODEC_VERSION
    GetVpuTimestamp(streamcorruptinfo.pts, nullptr);
  #endif
  if (result_) {
    result_->OnDecodeError(ERROR_CORRUPT_DATA);
  }
}

void MluDecoderImpl::VideoFrameCallback(cnvideoDecOutput *output) {
  if (cndec_abort_flag_.load() || cndec_error_flag_.load()) {
    return;
  }

  if (output->frame.width == 0 || output->frame.height == 0) {
    LOGW(SOURCE) << "Skip frame! " << (int64_t)this << " width x height:" << output->frame.width << " x "
                  << output->frame.height << " timestamp:" << output->pts << std::endl;
    return;
  }

  // query and set full 64bits timestamp
  uint64_t usr_pts;
  if (GetVpuTimestamp(output->pts, &usr_pts) == true) {
    // LOGI(SOURCE) << "Query timetamp," << output->pts << ":" << usr_pts << std::endl;
    // LOGI(SOURCE) << "Query timetamp, map_size = " << vpu_pts_map_.size() << std::endl;
    output->pts = usr_pts;
  } else {
    LOGD(SOURCE) << "Failed to query timetamp," << (int64_t)this
                 << ", use timestamp from vpu-decoder:" << output->pts << std::endl;
  }

  std::lock_guard<std::mutex> lk(instance_mutex_);
  cnvideoDecAddReference(instance_, &output->frame);
  //double start = TimeStamp::Current();
  ProcessFrame(output);
  //double end = TimeStamp::Current();
  //LOGD_IF(SOURCE, end - start > 5000000) << "processvideoFrame takes: " << end - start << "us.";
}

int MluDecoderImpl::ProcessFrame(cnvideoDecOutput *output) {
  if (cndec_abort_flag_.load() || cndec_error_flag_.load()) {
    cnvideoDecReleaseReference(instance_, &output->frame);
    return -1;
  }

  DecodeFrame cn_frame;
  cn_frame.valid = true;
  cn_frame.width =  output->frame.width;
  cn_frame.height =  output->frame.height;
  cn_frame.pts = output->pts;  
  switch (output->frame.pixelFmt) {
  case CNCODEC_PIX_FMT_NV12: 
    {
      cn_frame.fmt = DecodeFrame::FMT_NV12; 
      cn_frame.planeNum = 2;
      break;
    }
  case CNCODEC_PIX_FMT_NV21: 
    {
      cn_frame.fmt = DecodeFrame::FMT_NV21; 
      cn_frame.planeNum = 2;
      break;
    }
  default:
    {
      cn_frame.fmt = DecodeFrame::FMT_INVALID;
      cn_frame.planeNum = 0;
      break;
    }
  }
  cn_frame.mlu_addr = true;
  cn_frame.device_id = output->frame.deviceId;
  for (int i = 0; i < cn_frame.planeNum; i++) {
    cn_frame.stride[i] = output->frame.stride[i];
    cn_frame.plane[i] = reinterpret_cast<void *>(output->frame.plane[i].addr);
  }
  std::unique_ptr<CNDeallocator> deAllocator(new CNDeallocator(this, &output->frame));
  cn_frame.buf_ref = std::move(deAllocator);
  if (!cn_frame.buf_ref) {
    cnvideoDecReleaseReference(instance_, &output->frame);
  }    
  if (result_) {
    result_->OnDecodeFrame(&cn_frame);
  }
  return 0;
}

void MluDecoderImpl::VideoEosCallback() {
  if (result_) {
    result_->OnDecodeEos();
  }
  eos_got_.store(1);
}

void MluDecoderImpl::VideoResetCallback() {
  cndec_abort_flag_.store(1);
  if (result_) {
    result_->OnDecodeError(ERROR_RESET);
  } 
}

bool MluDecoderImpl::CreateVideoDecoder(VideoInfo *info, ExtraDecoderInfo *extra) {
  if (instance_) {
    LOGE(SOURCE) << "MluDecoder::CreateVideoDecoder, duplicated";
    return false;
  }
  memset(&create_info_, 0, sizeof(cnvideoDecCreateInfo));
  create_info_.deviceId = extra ? extra->device_id : 0;
  create_info_.instance = CNVIDEODEC_INSTANCE_AUTO;
  switch (info->codec_id) {
    case AV_CODEC_ID_H264:
      create_info_.codec = CNCODEC_H264;
      break;
    case AV_CODEC_ID_HEVC:
      create_info_.codec = CNCODEC_HEVC;
      break;
    default: {
      LOGE(SOURCE) << "codec type not supported yet, codec_id = " << info->codec_id;
      return false;
    }
  }
  create_info_.pixelFmt = CNCODEC_PIX_FMT_NV12;  
  create_info_.progressive = info->progressive; // to be removed later
  if (extra) {
    create_info_.inputBufNum = extra->input_buf_num;
    create_info_.outputBufNum = extra->output_buf_num;  // must be non-zero, although it is not used.
  } else {
    create_info_.inputBufNum = 2;
    create_info_.outputBufNum = 4;
  }
  
  create_info_.allocType = CNCODEC_BUF_ALLOC_LIB;
  create_info_.suggestedLibAllocBitStrmBufSize = 2 * 1024 * 1024; // FIXME
  create_info_.userContext = reinterpret_cast<void *>(this);
  eos_got_.store(0);
  eos_sent_.store(0);
  cndec_abort_flag_.store(0);
  cndec_error_flag_.store(0);
  cndec_start_flag_.store(0);

  // LOGI(SOURCE) << this << " cnvideoDecCreate called " << &create_info_ << "\n";
  int ret = cnvideoDecCreate(&this->instance_, VideoDecodeCallback, &create_info_);
  if (CNCODEC_SUCCESS != ret) {
    LOGE(SOURCE) << "Call cnvideoDecCreate failed, ret = " << ret;
    return false;
  }
  // LOGI(SOURCE) << this << " cnvideoDecCreate called Done, " << &create_info_
  //              << ", instance = " << instance_ << "\n";

  int stride_align = 1;
  if (extra && extra->apply_stride_align_for_scaler) {
    stride_align = 128; //YUV420SP_STRIDE_ALIGN_FOR_SCALER;
  }  
  ret = cnvideoDecSetAttributes(this->instance_, CNVIDEO_DEC_ATTR_OUT_BUF_ALIGNMENT, &stride_align);
  if (CNCODEC_SUCCESS != ret) {
    LOGE(SOURCE) << "Failed to set output buffer stride alignment,error code: " << ret;
    return false;
  }
  return true;
}

void MluDecoderImpl::DestroyVideoDecoder() {
  if (this->instance_) {
    if (!cndec_start_flag_.load()) {
      cnvideoDecAbort(instance_);
      instance_ = nullptr;
      if (result_) {
        result_->OnDecodeEos();
      }
      return;
    }
    if (!eos_sent_.load()) {
      this->Process(nullptr);
    }
    /**
     * make sure got eos before, than check release cndec buffers
     */
    while (!eos_got_.load()) {
      std::this_thread::yield();
      if (cndec_abort_flag_.load()) {
        break;
      }
    }
    /**
     * make sure all cndec buffers released before destorying cndecoder
     */
    while (cndec_buf_ref_count_.load()) {
      std::this_thread::yield();
      if (cndec_abort_flag_.load()) {
        break;
      }
    }
    if (cndec_abort_flag_.load()) {
      cnvideoDecAbort(instance_);
      instance_ = nullptr;
      if (result_) {
        result_->OnDecodeEos();
      }      
      return;
    }
    int ret = cnvideoDecStop(instance_);
    if (ret == -CNCODEC_TIMEOUT) {
      LOGE(SOURCE) << "cnvideoDecStop timeout happened";
      cnvideoDecAbort(instance_);
      instance_ = nullptr;
      if (result_) {
        result_->OnDecodeEos();
      }
      return;
    } else if (CNCODEC_SUCCESS != ret) {
      LOGE(SOURCE) << "Call cnvideoDecStop failed, ret = " << ret;
    }
    ret = cnvideoDecDestroy(instance_);
    if (CNCODEC_SUCCESS != ret) {
      LOGE(SOURCE) << "Call cnvideoDecDestroy failed, ret = " << ret;
    }
    instance_ = nullptr;
  }
}

//
// cnjpeg decoder
//
void MluDecoderImpl::JpegEosCallback(void) {
  if (result_) {
    result_->OnDecodeEos();
  }
  eos_got_.store(1);
}

void MluDecoderImpl::JpegResetCallback() {
  cndec_abort_flag_.store(1);
  if (result_) {
    result_->OnDecodeError(ERROR_RESET);
  }
}

void MluDecoderImpl::JpegFrameCallback(cnjpegDecOutput *output) {
  if (cndec_abort_flag_.load() || cndec_error_flag_.load()) {
    return;
  }

  if (output->result != 0) {
    /*
     If JPU decode failed, create a empty FrameInfo which only including
     an Error Flag(or valid information);
     Now olny support pts
    */
    DecodeFrame cn_frame;
    cn_frame.valid = false;
    cn_frame.pts = output->pts;  
    if (result_) {
      result_->OnDecodeFrame(&cn_frame);
    }
    return;
  }
  std::lock_guard<std::mutex> lk(instance_mutex_);
  cnjpegDecAddReference(jpg_instance_, &output->frame);
  //double start = TimeStamp::Current();
  ProcessJpegFrame(output);
  //double end = TimeStamp::Current();
  //LOGD_IF(SOURCE, end - start > 5000000) << "processJpegFrame takes: " << end - start << "us.";  
}

int MluDecoderImpl::ProcessJpegFrame(cnjpegDecOutput *output) {  
  if (cndec_abort_flag_.load() || cndec_error_flag_.load()) {
    cnjpegDecReleaseReference(jpg_instance_, &output->frame);
    return -1;
  }

  DecodeFrame cn_frame;
  cn_frame.valid = true;
  cn_frame.width =  output->frame.width;
  cn_frame.height =  output->frame.height;
  cn_frame.pts = output->pts;  
  switch (output->frame.pixelFmt) {
  case CNCODEC_PIX_FMT_NV12: 
    {
      cn_frame.fmt = DecodeFrame::FMT_NV12; 
      cn_frame.planeNum = 2;
      break;
    }
  case CNCODEC_PIX_FMT_NV21: 
    {
      cn_frame.fmt = DecodeFrame::FMT_NV21; 
      cn_frame.planeNum = 2;
      break;
    }
  default:
    {
      cn_frame.fmt = DecodeFrame::FMT_INVALID;
      cn_frame.planeNum = 0;
      break;
    }
  }
  cn_frame.mlu_addr = true;
  cn_frame.device_id = output->frame.deviceId;
  for (int i = 0; i < cn_frame.planeNum; i++) {
    cn_frame.stride[i] = output->frame.stride[i];
    cn_frame.plane[i] = reinterpret_cast<void *>(output->frame.plane[i].addr);
  }
  std::unique_ptr<CNDeallocatorJpg> deAllocator(new CNDeallocatorJpg(this, &output->frame));
  cn_frame.buf_ref = std::move(deAllocator);
  if (!cn_frame.buf_ref) {
    cnjpegDecReleaseReference(jpg_instance_, &output->frame);
  }    
  if (result_) {
    result_->OnDecodeFrame(&cn_frame);
  }
  return 0;
}

static int JpegEventCallback(cncodecCbEventType event, void *context, void *data) {
  MluDecoderImpl *pThis = reinterpret_cast<MluDecoderImpl *>(context);
  switch (event) {
    case CNCODEC_CB_EVENT_EOS:
      pThis->JpegEosCallback();
      break;

    case CNCODEC_CB_EVENT_SW_RESET:
    case CNCODEC_CB_EVENT_HW_RESET:
      LOGE(SOURCE) << "RESET Event received type = " << event;
      pThis->JpegResetCallback();
      break;

    case CNCODEC_CB_EVENT_NEW_FRAME:
      if (data) {
        pThis->JpegFrameCallback(reinterpret_cast<cnjpegDecOutput *>(data));
      }
      break;

    default:
      LOGE(SOURCE) << "unexpected Event received = " << event;
      return -1;
  }
  return 0;
}

bool MluDecoderImpl::CreateJpegDecoder(VideoInfo *info, ExtraDecoderInfo *extra) {
  if (jpg_instance_) {
    return false;
  }
  // maximum resolution 8K
  memset(&create_jpg_info_, 0, sizeof(cnjpegDecCreateInfo));
  create_jpg_info_.deviceId = extra ? extra->device_id : 0;
  create_jpg_info_.instance = CNJPEGDEC_INSTANCE_AUTO;
  create_jpg_info_.pixelFmt = CNCODEC_PIX_FMT_NV12;
  create_jpg_info_.width = (extra && extra->max_width) ? extra->max_width : 7680;
  create_jpg_info_.height = (extra && extra->max_height) ? extra->max_height : 4320;
  create_jpg_info_.enablePreparse = 0;
  create_jpg_info_.userContext = reinterpret_cast<void *>(this);
  create_jpg_info_.allocType = CNCODEC_BUF_ALLOC_LIB;
  if (extra) {
    create_jpg_info_.inputBufNum = extra->input_buf_num;
    create_jpg_info_.outputBufNum = extra->output_buf_num;
  } else {
    create_jpg_info_.inputBufNum = 2;
    create_jpg_info_.outputBufNum = 4;
  }
  
  create_jpg_info_.suggestedLibAllocBitStrmBufSize = create_jpg_info_.width * create_jpg_info_.height * 3/2/2;
  eos_got_.store(0);
  eos_sent_.store(0);
  cndec_abort_flag_.store(0);
  cndec_error_flag_.store(0);
  int ret = cnjpegDecCreate(&this->jpg_instance_, CNJPEGDEC_RUN_MODE_ASYNC, JpegEventCallback, &create_jpg_info_);
  if (CNCODEC_SUCCESS != ret) {
    LOGE(SOURCE) << "Call cnjpegDecCreate failed, ret = " << ret;
    return false;
  }
  return true;
}

void MluDecoderImpl::DestroyJpegDecoder() {
  if (this->jpg_instance_) {
    if (!eos_sent_.load()) {
      this->Process(nullptr);
    }
    /**
     * make sure got eos before, than check release cndec buffers
     */
    while (!eos_got_.load()) {
      std::this_thread::yield();
      if (cndec_abort_flag_.load()) {
        break;
      }
    }
    /**
     * make sure all cndec buffers released before destorying cndecoder
     */
    while (cndec_buf_ref_count_.load()) {
      std::this_thread::yield();
      if (cndec_abort_flag_.load()) {
        break;
      }
    }
    if (cndec_abort_flag_.load()) {
      cnjpegDecAbort(jpg_instance_);
      jpg_instance_ = nullptr;
      if (result_) {
        result_->OnDecodeEos();
      }
      return;
    }
    int ret = cnjpegDecDestroy(jpg_instance_);
    if (CNCODEC_SUCCESS != ret) {
      LOGE(SOURCE) << "Call cnjpegDecDestroy failed, ret = " << ret;
    }
    jpg_instance_ = nullptr;
  }
}

//----------------------------------------------------------------------------
// CPU decoder
bool FFmpegCpuDecoder::Create(VideoInfo *info, ExtraDecoderInfo *extra) {
  AVCodec *dec = avcodec_find_decoder(info->codec_id);
  if (!dec) {
    LOGE(SOURCE) << "avcodec_find_decoder failed";
    return false;
  }
  instance_ = avcodec_alloc_context3(dec);
  if (!instance_) {
    LOGE(SOURCE) << "Failed to do avcodec_alloc_context3";
    return false;
  }
  // av_codec_set_pkt_timebase(instance_, st->time_base);

  if (!extra->extra_info.empty()) {
    instance_->extradata = extra->extra_info.data();
    instance_->extradata_size = extra->extra_info.size();
  }

  if (avcodec_open2(instance_, dec, NULL) < 0) {
    LOGE(SOURCE) << "Failed to open codec";
    return false;
  }
  av_frame_ = av_frame_alloc();
  if (!av_frame_) {
    LOGE(SOURCE) << "Could not alloc frame";
    return false;
  }
  eos_got_.store(0);
  eos_sent_.store(0);
  return true;
}

void FFmpegCpuDecoder::Destroy() {
  if (instance_ != nullptr) {
    if (!eos_sent_.load()) {
      while (this->Process(nullptr, true)) {
      }
    }
    while (!eos_got_.load()) {
      std::this_thread::yield();
    }
    avcodec_close(instance_), av_free(instance_);
    instance_ = nullptr;
  }
  if (av_frame_) {
    av_frame_free(&av_frame_);
    av_frame_ = nullptr;
  }
}

bool FFmpegCpuDecoder::Process(VideoEsPacket *pkt) {
  if (pkt && pkt->data && pkt->len) {
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = pkt->data;
    packet.size = pkt->len;
    packet.pts = pkt->pts;
    return Process(&packet, false);
  }
  return Process(nullptr, true);
}

bool FFmpegCpuDecoder::Process(AVPacket *pkt, bool eos) {
  LOGD_IF(SOURCE, eos) << "[FFmpegCpuDecoder]  " << (int64_t)this << " send eos.";
  if (eos) {
    AVPacket packet;
    av_init_packet(&packet);
    packet.size = 0;
    packet.data = NULL;

    eos_sent_.store(1);
    // flush all frames ...
    int got_frame = 0;
    do {
      avcodec_decode_video2(instance_, av_frame_, &got_frame, &packet);
      if (got_frame) ProcessFrame(av_frame_);
    } while (got_frame);

    if (result_) {
      result_->OnDecodeEos();
    }
    eos_got_.store(1);
    return false;
  }
  int got_frame = 0;
  int ret = avcodec_decode_video2(instance_, av_frame_, &got_frame, pkt);
  if (ret < 0) {
    LOGE(SOURCE) << "avcodec_decode_video2 failed, data ptr, size:" << pkt->data << ", " << pkt->size;
    return true;
  }
#if LIBAVFORMAT_VERSION_INT <= FFMPEG_VERSION_3_1
  av_frame_->pts = pkt->pts;
#endif
  if (got_frame) {
    ProcessFrame(av_frame_);
  }
  return true;
}


bool FFmpegCpuDecoder::ProcessFrame(AVFrame *frame) {  
  if (instance_->pix_fmt != AV_PIX_FMT_YUV420P && instance_->pix_fmt != AV_PIX_FMT_YUVJ420P &&
      instance_->pix_fmt != AV_PIX_FMT_YUYV422) {
    LOGE(SOURCE) << "FFmpegCpuDecoder only supports AV_PIX_FMT_YUV420P , AV_PIX_FMT_YUVJ420P and AV_PIX_FMT_YUYV422";
    return false;
  }  
  DecodeFrame cn_frame;
  cn_frame.valid = true;
  cn_frame.width =  frame->width;
  cn_frame.height =  frame->height;
  cn_frame.pts = frame->pts;  
  switch (instance_->pix_fmt) {
  case AV_PIX_FMT_YUV420P: 
    {
      cn_frame.fmt = DecodeFrame::FMT_I420; 
      cn_frame.planeNum = 3;
      break;
    }
  case AV_PIX_FMT_YUVJ420P: 
    {
      cn_frame.fmt = DecodeFrame::FMT_J420; 
      cn_frame.planeNum = 3;
      break;
    }    
  case AV_PIX_FMT_YUYV422: 
    {
      cn_frame.fmt = DecodeFrame::FMT_YUYV; 
      cn_frame.planeNum = 1;
      break;
    }    
  default:
    {
      cn_frame.fmt = DecodeFrame::FMT_INVALID;
      cn_frame.planeNum = 0;
      break;
    }
  }
  cn_frame.mlu_addr = false;
  for (int i = 0; i < cn_frame.planeNum; i++) {
    cn_frame.stride[i] = frame->linesize[i];
    cn_frame.plane[i] = frame->data[i];
  }
  cn_frame.buf_ref = nullptr;
  if (result_) {
    result_->OnDecodeFrame(&cn_frame);
  }
  return true;
}

}  // namespace cnstream
