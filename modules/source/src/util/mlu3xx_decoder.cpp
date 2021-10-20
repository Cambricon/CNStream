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

#include <algorithm>
#include <atomic>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "cnstream_common.hpp"
#include "cnstream_logging.hpp"
#include "video_decoder.hpp"


#ifdef ENABLE_MLU300_CODEC
#include <cncodec_v3_common.h>
#include <cncodec_v3_dec.h>
#include <cnrt.h>

namespace cnstream {

class Mlu3xxDecoder : public Decoder {
 public:
  explicit Mlu3xxDecoder(const std::string& stream_id, IDecodeResult *cb) : Decoder(stream_id, cb) {}
  ~Mlu3xxDecoder() = default;
  bool Create(VideoInfo *info, ExtraDecoderInfo *extra) override;
  void Destroy() override;
  bool Process(VideoEsPacket *pkt) override;

  void ReceiveFrame(cncodecFrame_t *codec_frame);
  void ReceiveSequence(cncodecDecSequenceInfo_t *seq_info);
  void ReceiveEOS();
  void HandleStreamCorrupt();
  void HandleStreamNotSupport();
  void HandleUnknownEvent(cncodecEventType_t type);

 private:
  class CNDeallocator : public IDecBufRef {
   public:
    explicit CNDeallocator(Mlu3xxDecoder *decoder, cncodecFrame_t *frame) : decoder_(decoder), frame_(frame) {
      cncodecDecFrameRef(decoder_->instance_, frame);
      int origin_cnt = decoder_->cndec_buf_ref_count_.fetch_add(1);
      LOGT(SOURCE) << "[" << decoder_->stream_id_ << "]: Increase reference count [" << origin_cnt + 1 << "]";
    }
    ~CNDeallocator() {
      if (decoder_->created_) {
        LOGT(SOURCE) << "[" << decoder_->stream_id_ << "]: Begin release reference, buffer[" << frame_ << "]";
        cncodecDecFrameUnref(decoder_->instance_, frame_);
        LOGT(SOURCE) << "[" << decoder_->stream_id_ << "]: Finish release reference, buffer[" << frame_ << "]";
        int origin_cnt = decoder_->cndec_buf_ref_count_.fetch_sub(1);
        LOGT(SOURCE) << "[" << decoder_->stream_id_ << "]: Decrease reference count [" << origin_cnt - 1 << "]";
      }
    }

   private:
    Mlu3xxDecoder *decoder_;
    cncodecFrame_t *frame_;
  };  // class CNDeallocator
  void ResetFlags();
  bool SetDecParams();

 private:
  std::atomic<int> cndec_buf_ref_count_{0};
  std::atomic<bool> eos_sent_{false};  // flag for cndec-eos has been sent to decoder
  std::atomic<bool> timeout_{false};
  std::atomic<bool> error_flag_{false};
  std::atomic<bool> created_{false};
  std::unique_ptr<std::promise<void>> eos_promise_;
  cncodecDecCreateInfo_t create_info_;
  cncodecDecParams_t codec_params_;
  ExtraDecoderInfo extra_info_;
  int receive_seq_time_ = 0;
  cncodecHandle_t instance_ = 0;
};  // class Mlu3xxDecoder

static
i32_t Mlu3xxEventCallback(cncodecEventType_t type, void *ctx, void *output) {
  auto decoder = reinterpret_cast<Mlu3xxDecoder*>(ctx);
  switch (type) {
    case CNCODEC_EVENT_NEW_FRAME:
      decoder->ReceiveFrame(reinterpret_cast<cncodecFrame_t*>(output));
      break;
    case CNCODEC_EVENT_SEQUENCE:
      decoder->ReceiveSequence(reinterpret_cast<cncodecDecSequenceInfo_t*>(output));
      break;
    case CNCODEC_EVENT_EOS:
      decoder->ReceiveEOS();
      break;
    case CNCODEC_EVENT_STREAM_CORRUPT:
      decoder->HandleStreamCorrupt();
      break;
    case CNCODEC_EVENT_STREAM_NOT_SUPPORTED:
      decoder->HandleStreamNotSupport();
      break;
    default:
      decoder->HandleUnknownEvent(type);
      break;
  }
  return 0;
}

bool Mlu3xxDecoder::Create(VideoInfo *info, ExtraDecoderInfo *extra) {
  if (created_) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "Mlu3xxDecoder::Create, duplicated";
  }
  memset(&create_info_, 0, sizeof(create_info_));
  memset(&codec_params_, 0, sizeof(codec_params_));
  create_info_.device_id    = extra ? extra->device_id : 0;
  create_info_.send_mode    = CNCODEC_DEC_SEND_MODE_FRAME;
  create_info_.run_mode     = CNCODEC_RUN_MODE_ASYNC;
  switch (info->codec_id) {
    case AV_CODEC_ID_H264:
      create_info_.codec = CNCODEC_H264;
      break;
    case AV_CODEC_ID_HEVC:
      create_info_.codec = CNCODEC_HEVC;
      break;
    case AV_CODEC_ID_MJPEG:
      create_info_.codec = CNCODEC_JPEG;
      break;
    default: {
      LOGE(SOURCE) << "[" << stream_id_ << "]: "
                   << "Codec type not supported yet, codec_id = " << info->codec_id;
      return false;
    }
  }
  create_info_.stream_buf_size = 4 << 20;  // FIXME
  create_info_.user_context    = this;
  extra_info_ = *extra;

  ResetFlags();

  int codec_ret = cncodecDecCreate(&instance_, &Mlu3xxEventCallback, &create_info_);
  if (CNCODEC_SUCCESS != codec_ret) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "Call cncodecDecCreate failed, ret = " << codec_ret;
    return false;
  }

  created_ = true;

  codec_params_.output_buf_num    = extra ? extra->output_buf_num : 2;
  codec_params_.pixel_format      = CNCODEC_PIX_FMT_NV12;
  codec_params_.color_space       = CNCODEC_COLOR_SPACE_BT_709;
  codec_params_.output_buf_source = CNCODEC_BUF_SOURCE_LIB;
  codec_params_.output_order      = CNCODEC_DEC_OUTPUT_ORDER_DISPLAY;

  if (CNCODEC_JPEG == create_info_.codec) {
    codec_params_.max_width    = (extra && extra->max_width) ? extra->max_width : 7680;
    codec_params_.max_height   = (extra && extra->max_height) ? extra->max_height : 4320;
    codec_params_.stride_align = 64;  // must be multiple of 64 for jpeg
    return SetDecParams();
  } else {
    if (info->maximum_resolution.enable_variable_resolutions) {
      codec_params_.max_width = info->maximum_resolution.maximum_width;
      codec_params_.max_height = info->maximum_resolution.maximum_height;
    }
    codec_params_.stride_align = 1;
    codec_params_.dec_mode = CNCODEC_DEC_MODE_IPB;
  }

  return true;
}

void Mlu3xxDecoder::Destroy() {
  if (!created_) return;
  // if error happened, destroy directly, eos maybe not be transmitted from the decoder
  if (!error_flag_ && !eos_sent_) {
    Process(nullptr);
  }
  // wait eos
  if (eos_sent_) {
    eos_promise_->get_future().wait();
    eos_promise_.reset(nullptr);
  }
  /**
   * make sure all cndec buffers released before destorying cndecoder
   */
  while (cndec_buf_ref_count_) {
    std::this_thread::yield();
  }
  int codec_ret = cncodecDecDestroy(instance_);
  if (CNCODEC_SUCCESS != codec_ret) {
    LOGF(SOURCE) << "[" << stream_id_ << "]: "
                 << "Call cncodecDecDestroy failed, ret = " << codec_ret;
  }
  instance_ = 0;  // FIXME(lmx): INVALID HANDLE?
  ResetFlags();
}

bool Mlu3xxDecoder::Process(VideoEsPacket *pkt) {
  if (nullptr == pkt || nullptr == pkt->data) {
    if (eos_sent_) return true;
    // eos
    LOGI(SOURCE) << "[" << stream_id_ << "]: Sent EOS packet to decoder";
    eos_sent_ = true;
    eos_promise_.reset(new std::promise<void>);
    int codec_ret = cncodecDecSetEos(instance_);
    if (CNCODEC_SUCCESS != codec_ret) {
      LOGF(SOURCE) << "[" << stream_id_ << "]: "
                   << "Call cncodecDecSetEos failed, ret = " << codec_ret;
    }
  } else {
    if (eos_sent_) {
      LOGW(SOURCE) << "[" << stream_id_ << "]: "
                   << "EOS has been sent yet, process packet failed, pts:" << pkt->pts;
      return false;
    }
    if (error_flag_) {
      LOGW(SOURCE) << "[" << stream_id_ << "]: "
                   << "Error occurred in decoder, process packet failed, pts:" << pkt->pts;
      return false;
    }
    cncodecStream_t codec_input;
    memset(&codec_input, 0, sizeof(codec_input));
    codec_input.mem_type = CNCODEC_MEM_TYPE_HOST;
    codec_input.mem_addr = reinterpret_cast<u64_t>(pkt->data);
    codec_input.data_len = pkt->len;
    codec_input.pts = pkt->pts;
    int max_try_send_time = 3;
    while (max_try_send_time--) {
      int codec_ret = cncodecDecSendStream(instance_, &codec_input, 10000);
      switch (codec_ret) {
        case CNCODEC_SUCCESS: return true;
        case CNCODEC_ERROR_BAD_STREAM: {
          // parse jpeg stream failed.
          DecodeFrame cn_frame;
          cn_frame.valid  = false;
          result_->OnDecodeFrame(&cn_frame);
          return true;
        }
        case CNCODEC_ERROR_TIMEOUT:
          LOGW(SOURCE) << "[" << stream_id_ << "]: "
                       << "cncodecDecSendStream timeout happened, retry feed data, time: "
                       << 3 - max_try_send_time;
          continue;
        default:
          LOGE(SOURCE) << "[" << stream_id_ << "]: "
                       << "Call cncodecDecSendStream failed, ret = " << codec_ret;
          return false;
      }  // switch send stream ret
    }  // while timeout
    timeout_ = true;
    return false;
  }

  return true;
}

inline
bool Mlu3xxDecoder::SetDecParams() {
  int codec_ret = cncodecDecSetParams(instance_, &codec_params_);
  if (CNCODEC_SUCCESS != codec_ret) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "Call cncodecDecSetParams failed, ret = " << codec_ret;
    error_flag_ = true;
    return false;
  }
  return true;
}

inline
void Mlu3xxDecoder::ResetFlags() {
  eos_sent_ = false;
  timeout_  = false;
  error_flag_ = false;
  created_ = false;
}

void Mlu3xxDecoder::ReceiveFrame(cncodecFrame_t *codec_frame) {
  if (error_flag_) {
    LOGW(SOURCE) << "[" << stream_id_ << "]: "
                 << "Drop frame [pts:" << codec_frame->pts << "] because of error occurred in decoder.";
    return;
  }

  DecodeFrame cn_frame;
  cn_frame.valid  = true;
  cn_frame.width  =  codec_frame->width;
  cn_frame.height =  codec_frame->height;
  cn_frame.pts    =  codec_frame->pts;
  switch (codec_frame->pixel_format) {
  case CNCODEC_PIX_FMT_NV12:
    cn_frame.fmt = DecodeFrame::PixFmt::FMT_NV12;
    cn_frame.planeNum = 2;
    break;
  case CNCODEC_PIX_FMT_NV21:
    cn_frame.fmt = DecodeFrame::PixFmt::FMT_NV21;
    cn_frame.planeNum = 2;
    break;
  default:
    cn_frame.fmt = DecodeFrame::PixFmt::FMT_INVALID;
    cn_frame.planeNum = 0;
    break;
  }
  cn_frame.mlu_addr = true;
  cn_frame.device_id = codec_frame->device_id;
  for (int i = 0; i < cn_frame.planeNum; i++) {
    cn_frame.stride[i] = codec_frame->plane[i].stride;
    cn_frame.plane[i]  = reinterpret_cast<void *>(codec_frame->plane[i].dev_addr);
  }
#if 0
  /** reuse codec buf not support, support next cntoolkit version... **/
  std::unique_ptr<CNDeallocator> deAllocator(new CNDeallocator(this, codec_frame));
  cn_frame.buf_ref = std::move(deAllocator);
#endif
  if (result_) {
    result_->OnDecodeFrame(&cn_frame);
  }
}

inline
void Mlu3xxDecoder::ReceiveSequence(cncodecDecSequenceInfo_t *seq_info) {
  LOGI(SOURCE) << "[" << stream_id_ << "]: "
               << "Mlu3xxDecoder sequence info";

  receive_seq_time_++;
  if (1 < receive_seq_time_) {
    // variable geometry stream. check output buffer number, width, height and reset codec params
    if (codec_params_.output_buf_num < seq_info->min_output_buf_num + 1 ||
        codec_params_.max_width < seq_info->coded_width ||
        codec_params_.max_height < seq_info->coded_height) {
      LOGE(SOURCE) << "[" << stream_id_ << "]: "
                   << "Variable video resolutions, the preset parameters do not meet requirements."
                   << "max width[" << codec_params_.max_width << "], "
                   << "max height[" << codec_params_.max_height << "], "
                   << "output buffer number[" << codec_params_.output_buf_num << "]. "
                   << "But required: "
                   << "coded width[" << seq_info->coded_width << "], "
                   << "coded height[" << seq_info->coded_height << "], "
                   << "min output buffer number[" << seq_info->min_output_buf_num << "].";
      error_flag_ = true;
      if (result_) result_->OnDecodeError(DecodeErrorCode::ERROR_ABORT);
    }
  } else {
    if (codec_params_.max_width && codec_params_.max_height) {
      LOGI(SOURCE) << "[" << stream_id_ << "]: "
                   << "Variable video resolutions enabled, max width x max height : "
                   << codec_params_.max_width << " x " << codec_params_.max_height;
    } else {
      codec_params_.max_width = seq_info->coded_width;
      codec_params_.max_height = seq_info->coded_height;
    }
    codec_params_.output_buf_num = std::max(seq_info->min_output_buf_num + 1, codec_params_.output_buf_num);
    if (!SetDecParams()) {
      LOGE(SOURCE) << "[" << stream_id_ << "]: "
                    << "Set decoder params failed.";
      error_flag_ = true;
      if (result_) result_->OnDecodeError(DecodeErrorCode::ERROR_FAILED_TO_START);
    }
  }
}

inline
void Mlu3xxDecoder::ReceiveEOS() {
  eos_promise_->set_value();
  if (result_) {
    result_->OnDecodeEos();
  }
}

inline
void Mlu3xxDecoder::HandleStreamCorrupt() {
  LOGW(SOURCE) << "[" << stream_id_ << "]: "
               << "Stream corrupt...";
}

inline
void Mlu3xxDecoder::HandleStreamNotSupport() {
  LOGW(SOURCE) << "[" << stream_id_ << "]: "
               << "Stream not support event received...";
  error_flag_ = true;
  if (result_) result_->OnDecodeError(DecodeErrorCode::ERROR_ABORT);
}

inline
void Mlu3xxDecoder::HandleUnknownEvent(cncodecEventType_t type) {
  LOGW(SOURCE) << "[" << stream_id_ << "]: "
               << "Unknown event, event type: " << static_cast<int>(type);
}

Decoder* CreateMlu3xxDecoder(const std::string& stream_id, IDecodeResult *cb) {
  return new Mlu3xxDecoder(stream_id, cb);
}

}  // namespace cnstream

#else

namespace cnstream {

Decoder* CreateMlu3xxDecoder(const std::string& stream_id, IDecodeResult *cb) {
  LOGE(SOURCE) << "Run on MLU370, please compile CNStream using a cntoolkit version 2.0.0 or higher.";
  return nullptr;
}

}  // namespace cnstream

#endif  // ENABLE_MLU300_CODEC
