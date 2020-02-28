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

#ifndef MODULES_SOURCE_FFMPEG_DECODER_HPP_
#define MODULES_SOURCE_FFMPEG_DECODER_HPP_

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#ifdef __cplusplus
}
#endif

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include "cnstream_frame.hpp"
#include "cnstream_timer.hpp"
#include "data_handler.hpp"
#include "easycodec/easy_decode.h"
#include "easycodec/vformat.h"
#include "easyinfer/mlu_context.h"

namespace cnstream {

class FFmpegDecoder {
 public:
  explicit FFmpegDecoder(DataHandler &handler) : handler_(handler) {
    stream_id_ = handler_.GetStreamId();
    stream_idx_ = handler_.GetStreamIndex();
    dev_ctx_ = handler_.GetDevContext();
  }
  virtual ~FFmpegDecoder() {}
  virtual bool Create(AVStream *st) = 0;
  virtual void Destroy() = 0;
  virtual bool Process(AVPacket *pkt, bool eos) = 0;
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

class FFmpegMluDecoder : public FFmpegDecoder {
 public:
  explicit FFmpegMluDecoder(DataHandler &handler) : FFmpegDecoder(handler) {}
  ~FFmpegMluDecoder() {
    edk::MluContext env;
    env.SetDeviceId(dev_ctx_.dev_id);
    env.ConfigureForThisThread();
    // PrintPerformanceInfomation();
  }
  bool Create(AVStream *st) override;
  void Destroy() override;
  bool Process(AVPacket *pkt, bool eos) override;

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
#ifdef UNIT_TEST
 private:  // NOLINT
#endif

/*
 * useless
 */
#if 0
  CNTimer fps_calculators[4];
  void PrintPerformanceInfomation() const {
    printf("stream_id: %s:\n", stream_id_.c_str());
    fps_calculators[0].PrintFps("transfer memory: ");
    fps_calculators[1].PrintFps("decode delay: ");
    fps_calculators[2].PrintFps("send data to codec: ");
    fps_calculators[3].PrintFps("output :");
  }
#endif

 private:
  class CNDeallocator : public cnstream::IDataDeallocator {
   public:
    explicit CNDeallocator(FFmpegMluDecoder *decoder, uint64_t buf_id) : decoder_(decoder), buf_id_(buf_id) {
      ++decoder_->cndec_buf_ref_count_;
    }
    ~CNDeallocator() {
      if (decoder_->instance_) {
        decoder_->instance_->ReleaseBuffer(buf_id_);
        --decoder_->cndec_buf_ref_count_;
      }
    }

   private:
    FFmpegMluDecoder *decoder_;
    uint64_t buf_id_;
  };
};

class FFmpegCpuDecoder : public FFmpegDecoder {
 public:
  explicit FFmpegCpuDecoder(DataHandler &handler) : FFmpegDecoder(handler) {}
  ~FFmpegCpuDecoder() {}
  bool Create(AVStream *st) override;
  void Destroy() override;
  bool Process(AVPacket *pkt, bool eos) override;

 private:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  bool ProcessFrame(AVFrame *frame);

 private:
  AVCodecContext *instance_ = nullptr;
  AVFrame *av_frame_ = nullptr;
  std::atomic<int> eos_got_{0};
  uint8_t *nv21_data_ = nullptr;
  int y_size_ = 0;
};
}  // namespace cnstream

#endif  // MODULES_SOURCE_FFMPEG_DECODER_HPP_
