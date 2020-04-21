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

#ifndef MODULES_SOURCE_RAW_DECODER_HPP_
#define MODULES_SOURCE_RAW_DECODER_HPP_

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include "cnstream_frame.hpp"
#include "data_handler.hpp"
#include "easycodec/easy_decode.h"
#include "easycodec/vformat.h"
#include "easyinfer/mlu_context.h"

namespace cnstream {

struct RawPacket {
  uint8_t *data = nullptr;
  size_t size = 0;
  int64_t pts = 0;
  uint32_t flags = 0;
};

struct DecoderContext {
  enum CNCodecID { CN_CODEC_ID_RAWVIDEO = 1, CN_CODEC_ID_H264, CN_CODEC_ID_HEVC, CN_CODEC_ID_JPEG } codec_id;
  enum CNPixFmt { CN_PIX_FMT_NONE = 1, CN_PIX_FMT_NV21 } pix_fmt;
  size_t width;
  size_t height;
  bool interlaced;
  bool chunk_mode;  // for H264/H265
};

class RawDecoder {
 public:
  explicit RawDecoder(DataHandler &handler) : handler_(handler) {
    stream_id_ = handler_.GetStreamId();
    stream_idx_ = handler_.GetStreamIndex();
    dev_ctx_ = handler_.GetDevContext();
  }
  virtual ~RawDecoder() {}
  virtual bool Create(DecoderContext *ctx) = 0;
  virtual void Destroy() = 0;
  virtual bool Process(RawPacket *pkt, bool eos) = 0;
  virtual void ResetCount(size_t interval) {
    frame_count_ = 0;
    frame_id_ = 0;
    interval_ = interval;
  }

 protected:
  std::string stream_id_;
  DataHandler &handler_;

  uint32_t stream_idx_;
  DevContext dev_ctx_;
  size_t interval_ = 1;
  size_t frame_count_ = 0;
  uint64_t frame_id_ = 0;
};

class RawMluDecoder : public RawDecoder {
 public:
  explicit RawMluDecoder(DataHandler &handler) : RawDecoder(handler) {}
  ~RawMluDecoder() {
    edk::MluContext env;
    env.SetDeviceId(dev_ctx_.dev_id);
    env.ConfigureForThisThread();
  }
  bool Create(DecoderContext *ctx) override;
  void Destroy() override;
  bool Process(RawPacket *pkt, bool eos) override;

 private:
  std::shared_ptr<edk::EasyDecode> instance_ = nullptr;
  edk::CnPacket cn_packet_;
  std::atomic<int> eos_got_{0};
  std::atomic<int> cndec_buf_ref_count_{0};
  void FrameCallback(const edk::CnFrame &frame);
  void EOSCallback();

#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  int ProcessFrame(const edk::CnFrame &frame, bool *reused);

 private:
  class CNDeallocator : public cnstream::IDataDeallocator {
   public:
    explicit CNDeallocator(RawMluDecoder *decoder, uint64_t buf_id) : decoder_(decoder), buf_id_(buf_id) {
      ++decoder_->cndec_buf_ref_count_;
    }
    ~CNDeallocator() {
      if (decoder_->instance_) {
        decoder_->instance_->ReleaseBuffer(buf_id_);
        --decoder_->cndec_buf_ref_count_;
      }
    }

   private:
    RawMluDecoder *decoder_;
    uint64_t buf_id_;
  };
};

}  // namespace cnstream

#endif  // MODULES_SOURCE_RAW_DECODER_HPP_
