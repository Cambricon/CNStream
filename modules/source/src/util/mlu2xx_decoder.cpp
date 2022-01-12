/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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

#include <cnrt.h>
#include <cn_jpeg_dec.h>
#include <cn_video_dec.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <map>
#include <utility>

#include "cnstream_common.hpp"
#include "cnstream_logging.hpp"
#include "video_decoder.hpp"

namespace cnstream {

class Mlu2xxDecoder : public Decoder {
 public:
  explicit Mlu2xxDecoder(const std::string& stream_id, IDecodeResult *cb) : Decoder(stream_id, cb) {}
  ~Mlu2xxDecoder() = default;
  bool Create(VideoInfo *info, ExtraDecoderInfo *extra) override;
  void Destroy() override;
  bool Process(VideoEsPacket *pkt) override;

 public:
  const std::string& GetStreamId() const;
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
  void WaitAllBuffersBack();

 private:
  std::atomic<int> cndec_start_flag_{0};
  std::atomic<int> cndec_error_flag_{0};
  std::atomic<int> cndec_abort_flag_{0};
  std::atomic<int> eos_got_{0};
  std::atomic<int> cndec_buf_ref_count_{0};
  std::atomic<int> eos_sent_{0};  // flag for cndec-eos has been sent to decoder
  // cnvideo
  cnvideoDecCreateInfo create_info_;
  cnvideoDecoder instance_ = nullptr;
  class CNDeallocator : public IDecBufRef {
   public:
    explicit CNDeallocator(Mlu2xxDecoder *decoder, cncodecFrame *frame) : decoder_(decoder), frame_(frame) {
      int origin_cnt = decoder_->cndec_buf_ref_count_.fetch_add(1);
      LOGT(SOURCE) << "[" << decoder_->stream_id_ << "]: Increase reference count [" << origin_cnt + 1 << "]";
    }
    ~CNDeallocator() {
      if (decoder_->instance_) {
        LOGT(SOURCE) << "[" << decoder_->stream_id_ << "]: Begin release reference, buffer[" << frame_ << "]";
        cnvideoDecReleaseReference(decoder_->instance_, frame_);
        LOGT(SOURCE) << "[" << decoder_->stream_id_ << "]: Finish release reference, buffer[" << frame_ << "]";
        int origin_cnt = decoder_->cndec_buf_ref_count_.fetch_sub(1);
        LOGT(SOURCE) << "[" << decoder_->stream_id_ << "]: Decrease reference count [" << origin_cnt - 1 << "]";
      }
    }

   private:
    Mlu2xxDecoder *decoder_;
    cncodecFrame *frame_;
  };

  // For m200 vpu-decoder, m200 vpu-codec does not 64bits timestamp, we have to implement it.
  uint32_t pts_key_ = 0;
  std::map<uint32_t, uint64_t> vpu_pts_map_;
  std::mutex map_lock_;
  uint32_t SetVpuTimestamp(uint64_t pts) {
    std::lock_guard<std::mutex> guard(map_lock_);
    uint32_t key = pts_key_++;
    vpu_pts_map_[key] = pts;
    return key;
  }
  bool GetVpuTimestamp(uint32_t key, uint64_t *pts) {
    if (!pts) return false;
    std::lock_guard<std::mutex>  guard(map_lock_);
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
    explicit CNDeallocatorJpg(Mlu2xxDecoder *decoder, cncodecFrame *frame) : decoder_(decoder), frame_(frame) {
      int origin_cnt = decoder_->cndec_buf_ref_count_.fetch_add(1);
      LOGT(SOURCE) << "[" << decoder_->stream_id_ << "]: Increase reference count [" << origin_cnt + 1 << "]";
    }
    ~CNDeallocatorJpg() {
      if (decoder_->jpg_instance_) {
        LOGT(SOURCE) << "[" << decoder_->stream_id_ << "]: Begin release reference, buffer[" << frame_ << "]";
        cnjpegDecReleaseReference(decoder_->jpg_instance_, frame_);
        LOGT(SOURCE) << "[" << decoder_->stream_id_ << "]: Finish release reference, buffer[" << frame_ << "]";
        int origin_cnt = decoder_->cndec_buf_ref_count_.fetch_sub(1);
        LOGT(SOURCE) << "[" << decoder_->stream_id_ << "]: Decrease reference count [" << origin_cnt - 1 << "]";
      }
    }

   private:
    Mlu2xxDecoder *decoder_;
    cncodecFrame *frame_;
  };
  std::mutex instance_mutex_;
  VideoInfo info_;
  ExtraDecoderInfo extra_;
};


bool Mlu2xxDecoder::Create(VideoInfo *info, ExtraDecoderInfo *extra) {
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

void Mlu2xxDecoder::Destroy() {
  LOGI(SOURCE) << "[" << stream_id_ << "]: Begin to destroy decoder";
  if (instance_) {
    if (!cndec_abort_flag_.load()) {
      DestroyVideoDecoder();
    } else {
      // make sure all cndec buffers released before abort
      WaitAllBuffersBack();
      std::lock_guard<std::mutex> lk(instance_mutex_);
      LOGI(SOURCE) << "[" << stream_id_ << "]: Begin aborting decoder";
      cnvideoDecAbort(instance_);
      LOGI(SOURCE) << "[" << stream_id_ << "]: Finish aborting decoder";
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
      WaitAllBuffersBack();
      std::lock_guard<std::mutex> lk(instance_mutex_);
      LOGI(SOURCE) << "[" << stream_id_ << "]: Begin abort decoder";
      cnjpegDecAbort(jpg_instance_);
      LOGI(SOURCE) << "[" << stream_id_ << "]: Finish abort decoder";
      jpg_instance_ = nullptr;
      if (result_) {
        result_->OnDecodeEos();
      }
    }
  }
  LOGI(SOURCE) << "[" << stream_id_ << "]: Finish destroy decoder";
}

bool Mlu2xxDecoder::Process(VideoEsPacket *pkt) {
  if (cndec_abort_flag_.load() || cndec_error_flag_.load()) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: abort flag or error flag is true, process failed";
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
        LOGW(SOURCE) << "[" << stream_id_ << "]: "
                     << "cnvideoDecFeedData- truncate " << input.streamLength << " to "
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
        LOGW(SOURCE) << "[" << stream_id_ << "]: cnvideoDecFeedData(eos) timeout happened";
        cndec_abort_flag_ = 1;
        return false;
      } else if (CNCODEC_SUCCESS != ret) {
        LOGE(SOURCE) << "[" << stream_id_ << "]: cnvideoDecFeedData(eos) failed, ret = " << ret;
        cndec_error_flag_ = 1;
        return false;
      } else {
        LOGI(SOURCE) << "[" << stream_id_ << "]: cnvideoDecFeedData(eos) succeed. ";
        return true;
      }
    } else {
      int retry_time = 3;
      while (retry_time) {
        int ret = cnvideoDecFeedData(instance_, &input, 10000);  // FIXME
        if (-CNCODEC_TIMEOUT == ret) {
          retry_time--;
          LOGW(SOURCE) << "[" << stream_id_ << "]: "
                       << "cnvideoDecFeedData(data) timeout happened, retry feed data, time: "
                       << 3 - retry_time;
          continue;
        } else if (CNCODEC_SUCCESS != ret) {
          LOGE(SOURCE) << "[" << stream_id_ << "]: "
                       << "Call cnvideoDecFeedData(data) failed, ret = " << ret;
          GetVpuTimestamp(input.pts, nullptr);  // Failed to feeddata, erase record
          cndec_error_flag_ = 1;
          return false;
        } else {
          cndec_start_flag_.store(1);
          return true;
        }
      }
      GetVpuTimestamp(input.pts, nullptr);  // Failed to feeddata, erase record

      if (0 == retry_time) {
        LOGW(SOURCE) << "[" << stream_id_ << "]: "
                     << "cnvideoDecFeedData(data) timeout 3 times, prepare abort decoder.";
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
        LOGW(SOURCE) << "[" << stream_id_ << "]: "
                     << "cnjpegDecFeedData- truncate " << input.streamLength << " to "
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
        LOGW(SOURCE) << "[" << stream_id_ << "]: "
                     << "cnjpegDecFeedData(eos) timeout happened";
        cndec_abort_flag_ = 1;
        return false;
      } else if (CNCODEC_SUCCESS != ret) {
        LOGE(SOURCE) << "[" << stream_id_ << "]: "
                     << "Call cnjpegDecFeedData(eos) failed, ret = " << ret;
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
          LOGW(SOURCE) << "[" << stream_id_ << "]: "
                       << "cnjpegDecFeedData(data) timeout happened, retry feed data, time: " << 3 - retry_time;
          continue;
        } else if (CNCODEC_SUCCESS != ret) {
          LOGE(SOURCE) << "[" << stream_id_ << "]: "
                       << "Call cnjpegDecFeedData(data) failed, ret = " << ret;
          cndec_error_flag_ = 1;
          return false;
        } else {
          return true;
        }
      }
      if (0 == retry_time) {
        LOGW(SOURCE) << "[" << stream_id_ << "]: "
                     << "cnjpegDecFeedData(data) timeout 3 times, prepare abort decoder.";
        // don't processframe
        cndec_abort_flag_ = 1;
        return false;
      }
    }
  }

  // should not come here...
  return false;
}

inline
const std::string& Mlu2xxDecoder::GetStreamId() const {
  return stream_id_;
}

void Mlu2xxDecoder::WaitAllBuffersBack() {
  LOGI(SOURCE) << "[" << stream_id_ << "]: Wait all buffers back...";
  while (this->cndec_buf_ref_count_.load()) {
    std::this_thread::yield();
  }
  LOGI(SOURCE) << "[" << stream_id_ << "]: All buffers back";
}

//
// video decoder implementation
//
static int VideoDecodeCallback(cncodecCbEventType EventType, void *pData, void *pdata1) {
  Mlu2xxDecoder *pThis = reinterpret_cast<Mlu2xxDecoder *>(pData);
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
      LOGE(SOURCE) << "[" << pThis->GetStreamId() << "]: "
                   << "Decode Firmware crash Event Event: " << EventType;
      pThis->VideoResetCallback();
      break;
    case CNCODEC_CB_EVENT_OUT_OF_MEMORY:
      LOGE(SOURCE) << "[" << pThis->GetStreamId() << "]: "
                   << "Decode out of memory, force stop";
      pThis->VideoEosCallback();
      break;
    case CNCODEC_CB_EVENT_ABORT_ERROR:
      LOGE(SOURCE) << "[" << pThis->GetStreamId() << "]: "
                   << "Decode abort error occured, force stop";
      pThis->VideoEosCallback();
      break;
    case CNCODEC_CB_EVENT_STREAM_CORRUPT:
      LOGW(SOURCE) << "[" << pThis->GetStreamId() << "]: "
                   << "Stream corrupt, discard frame";
      pThis->CorruptCallback(*reinterpret_cast<cnvideoDecStreamCorruptInfo *>(pdata1));
      break;
    default:
      LOGE(SOURCE) << "[" << pThis->GetStreamId() << "]: "
                   << "Unsupported Decode Event: " << EventType;
      break;
  }
  return 0;
}

void Mlu2xxDecoder::SequenceCallback(cnvideoDecSequenceInfo *pFormat) {
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
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "Call cnvideoDecStart failed, ret = " << ret;
    if (result_) {
      result_->OnDecodeError(DecodeErrorCode::ERROR_FAILED_TO_START);
    }
    return;
  }
  // LOGI(SOURCE) << " cnvideoDecStart called done, " << instance_  << "\n";
}

void Mlu2xxDecoder::CorruptCallback(const cnvideoDecStreamCorruptInfo &streamcorruptinfo) {
  LOGW(SOURCE) << "[" << stream_id_ << "]: "
               << "Skip frame number: " << streamcorruptinfo.frameNumber
               << ", frame count: " << streamcorruptinfo.frameCount
               << ", " << instance_;
  #define CORRUPT_PTS_CNCODEC_VERSION 10800
  #if CNCODEC_VERSION >= CORRUPT_PTS_CNCODEC_VERSION
    GetVpuTimestamp(streamcorruptinfo.pts, nullptr);
  #endif
}

void Mlu2xxDecoder::VideoFrameCallback(cnvideoDecOutput *output) {
  if (cndec_abort_flag_.load() || cndec_error_flag_.load()) {
    return;
  }

  if (output->frame.width == 0 || output->frame.height == 0) {
    LOGW(SOURCE) << "[" << stream_id_ << "]: "
                 << "Skip frame! " << (int64_t)this << " width x height:" << output->frame.width << " x "
                 << output->frame.height << " timestamp:" << output->pts << std::endl;
    return;
  }

  // query and set full 64bits timestamp
  uint64_t usr_pts;
  if (GetVpuTimestamp(output->pts, &usr_pts) == true) {
    output->pts = usr_pts;
  } else {
    LOGD(SOURCE) << "[" << stream_id_ << "]: "
                 << "Failed to query timetamp," << (int64_t)this
                 << ", use timestamp from vpu-decoder:" << output->pts << std::endl;
  }

  LOGT(SOURCE) << "[" << stream_id_ << "]: "
               << "Begin add reference, buffer[" << &output->frame << "]";
  std::lock_guard<std::mutex> lk(instance_mutex_);
  cnvideoDecAddReference(instance_, &output->frame);
  LOGT(SOURCE) << "[" << stream_id_ << "]: "
               << "Finish add reference, buffer[" << &output->frame << "]";
  ProcessFrame(output);
}

int Mlu2xxDecoder::ProcessFrame(cnvideoDecOutput *output) {
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
    case CNCODEC_PIX_FMT_NV12: {
      cn_frame.fmt = DecodeFrame::PixFmt::FMT_NV12;
      cn_frame.planeNum = 2;
      break;
    }
    case CNCODEC_PIX_FMT_NV21: {
      cn_frame.fmt = DecodeFrame::PixFmt::FMT_NV21;
      cn_frame.planeNum = 2;
      break;
    }
    default: {
      cn_frame.fmt = DecodeFrame::PixFmt::FMT_INVALID;
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

void Mlu2xxDecoder::VideoEosCallback() {
  if (result_) {
    result_->OnDecodeEos();
  }
  eos_got_.store(1);
}

void Mlu2xxDecoder::VideoResetCallback() {
  cndec_abort_flag_.store(1);
}

bool Mlu2xxDecoder::CreateVideoDecoder(VideoInfo *info, ExtraDecoderInfo *extra) {
  if (instance_) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "MluDecoder::CreateVideoDecoder, duplicated";
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
      LOGE(SOURCE) << "[" << stream_id_ << "]: "
                   << "codec type not supported yet, codec_id = " << info->codec_id;
      return false;
    }
  }
  create_info_.pixelFmt = CNCODEC_PIX_FMT_NV12;
  create_info_.progressive = info->progressive;  // to be removed later
  if (extra) {
    create_info_.inputBufNum = extra->input_buf_num;
    create_info_.outputBufNum = extra->output_buf_num;  // must be non-zero, although it is not used.
  } else {
    create_info_.inputBufNum = 2;
    create_info_.outputBufNum = 4;
  }

  create_info_.allocType = CNCODEC_BUF_ALLOC_LIB;
  create_info_.suggestedLibAllocBitStrmBufSize = 2 * 1024 * 1024;  // FIXME
  create_info_.userContext = reinterpret_cast<void *>(this);
  eos_got_.store(0);
  eos_sent_.store(0);
  cndec_abort_flag_.store(0);
  cndec_error_flag_.store(0);
  cndec_start_flag_.store(0);

  // LOGI(SOURCE) << this << " cnvideoDecCreate called " << &create_info_ << "\n";
  int ret = cnvideoDecCreate(&this->instance_, VideoDecodeCallback, &create_info_);
  if (CNCODEC_SUCCESS != ret) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "Call cnvideoDecCreate failed, ret = " << ret;
    return false;
  }
  // LOGI(SOURCE) << this << " cnvideoDecCreate called Done, " << &create_info_
  //              << ", instance = " << instance_ << "\n";

  int stride_align = 1;
  if (extra && extra->apply_stride_align_for_scaler) {
    stride_align = 128;  // YUV420SP_STRIDE_ALIGN_FOR_SCALER;
  }
  ret = cnvideoDecSetAttributes(this->instance_, CNVIDEO_DEC_ATTR_OUT_BUF_ALIGNMENT, &stride_align);
  if (CNCODEC_SUCCESS != ret) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "Failed to set output buffer stride alignment,error code: " << ret;
    return false;
  }
  return true;
}

void Mlu2xxDecoder::DestroyVideoDecoder() {
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
      LOGW(SOURCE) << "[" << stream_id_ << "]: "
                   << "cnvideoDecStop timeout happened";
      cnvideoDecAbort(instance_);
      instance_ = nullptr;
      if (result_) {
        result_->OnDecodeEos();
      }
      return;
    } else if (CNCODEC_SUCCESS != ret) {
      LOGE(SOURCE) << "[" << stream_id_ << "]: "
                   << "Call cnvideoDecStop failed, ret = " << ret;
    }
    ret = cnvideoDecDestroy(instance_);
    if (CNCODEC_SUCCESS != ret) {
      LOGE(SOURCE) << "[" << stream_id_ << "]: "
                   << "Call cnvideoDecDestroy failed, ret = " << ret;
    }
    instance_ = nullptr;
  }
}

//
// cnjpeg decoder
//
void Mlu2xxDecoder::JpegEosCallback(void) {
  if (result_) {
    result_->OnDecodeEos();
  }
  eos_got_.store(1);
}

void Mlu2xxDecoder::JpegResetCallback() {
  cndec_abort_flag_.store(1);
  if (result_) {
    result_->OnDecodeError(DecodeErrorCode::ERROR_RESET);
  }
}

void Mlu2xxDecoder::JpegFrameCallback(cnjpegDecOutput *output) {
  if (cndec_abort_flag_.load() || cndec_error_flag_.load()) {
    return;
  }

  if (output->result != 0) {
    /*
     If JPU decode failed, create a empty FrameInfo which only including
     an Error Flag(or valid information);
     Now only support pts
    */
    DecodeFrame cn_frame;
    cn_frame.valid = false;
    cn_frame.pts = output->pts;
    if (result_) {
      result_->OnDecodeFrame(&cn_frame);
    }
    return;
  }
  LOGT(SOURCE) << "[" << stream_id_ << "]: "
               << "Begin add reference, buffer[" << &output->frame << "]";
  std::lock_guard<std::mutex> lk(instance_mutex_);
  cnjpegDecAddReference(jpg_instance_, &output->frame);
  LOGT(SOURCE) << "[" << stream_id_ << "]: "
               << "Finish add reference, buffer[" << &output->frame << "]";
  ProcessJpegFrame(output);
}

int Mlu2xxDecoder::ProcessJpegFrame(cnjpegDecOutput *output) {
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
    case CNCODEC_PIX_FMT_NV12: {
      cn_frame.fmt = DecodeFrame::PixFmt::FMT_NV12;
      cn_frame.planeNum = 2;
      break;
    }
    case CNCODEC_PIX_FMT_NV21: {
      cn_frame.fmt = DecodeFrame::PixFmt::FMT_NV21;
      cn_frame.planeNum = 2;
      break;
    }
    default: {
      cn_frame.fmt = DecodeFrame::PixFmt::FMT_INVALID;
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
  Mlu2xxDecoder *pThis = reinterpret_cast<Mlu2xxDecoder *>(context);
  switch (event) {
    case CNCODEC_CB_EVENT_EOS:
      pThis->JpegEosCallback();
      break;

    case CNCODEC_CB_EVENT_SW_RESET:
    case CNCODEC_CB_EVENT_HW_RESET:
      LOGE(SOURCE) << "[" << pThis->GetStreamId() << "]: "
                   << "RESET Event received type = " << event;
      pThis->JpegResetCallback();
      break;

    case CNCODEC_CB_EVENT_NEW_FRAME:
      if (data) {
        pThis->JpegFrameCallback(reinterpret_cast<cnjpegDecOutput *>(data));
      }
      break;

    default:
      LOGE(SOURCE) << "[" << pThis->GetStreamId() << "]: "
                   << "unexpected Event received = " << event;
      return -1;
  }
  return 0;
}

bool Mlu2xxDecoder::CreateJpegDecoder(VideoInfo *info, ExtraDecoderInfo *extra) {
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
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "Call cnjpegDecCreate failed, ret = " << ret;
    return false;
  }
  return true;
}

void Mlu2xxDecoder::DestroyJpegDecoder() {
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
      LOGE(SOURCE) << "[" << stream_id_ << "]: "
                   << "Call cnjpegDecDestroy failed, ret = " << ret;
    }
    jpg_instance_ = nullptr;
  }
}

Decoder* CreateMlu2xxDecoder(const std::string& stream_id, IDecodeResult *cb) {
  return new Mlu2xxDecoder(stream_id, cb);
}

}  // namespace cnstream
