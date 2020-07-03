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

#ifndef MODULES_SOURCE_DECODER_HPP_
#define MODULES_SOURCE_DECODER_HPP_

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
#include "data_source.hpp"
#include "easyinfer/mlu_context.h"
#include "cnstream_frame_va.hpp"
#include "cn_video_dec.h"
#include "cn_jpeg_dec.h"
#include "ffmpeg_parser.hpp"

namespace cnstream {

class IHandler {
 public:
  virtual ~IHandler() {}
  virtual std::shared_ptr<CNFrameInfo> CreateFrameInfo(bool eos = false) = 0;
  virtual bool SendFrameInfo(std::shared_ptr<CNFrameInfo> data) = 0;
  virtual void SendFlowEos() = 0;
  virtual const DataSourceParam& GetDecodeParam() const = 0;
};

class Decoder {
 public:
  explicit Decoder(IHandler *handler) : handler_(handler) {
    if (handler_) {
      param_ = handler_->GetDecodeParam();
    }
  }
  virtual ~Decoder() {}
  virtual bool Create(AVStream *st, int interval = 1) { return false ;}
  virtual bool Create(VideoStreamInfo *info, int interval) {return false;}
  virtual bool Process(AVPacket *pkt, bool eos) {return false;}
  virtual bool Process(ESPacket *pkt) {return false;}
  virtual void Destroy() = 0;

 protected:
  IHandler *handler_;
  DataSourceParam param_;
  size_t interval_ = 1;
  size_t frame_count_ = 0;
  uint64_t frame_id_ = 0;
};

class MluDecoder : public Decoder {
 public:
  explicit MluDecoder(IHandler *handler) : Decoder(handler) {}
  ~MluDecoder() {
    edk::MluContext env;
    env.SetDeviceId(param_.device_id_);
    env.ConfigureForThisThread();
    // PrintPerformanceInfomation();
  }
  bool Create(AVStream *st, int interval = 1) override;
  bool Create(VideoStreamInfo *info, int interval) override;
  void Destroy() override;
  bool Process(ESPacket *pkt) override;
  bool Process(AVPacket *pkt, bool eos) override;

 public:
  bool CreateVideoDecoder(VideoStreamInfo *info);
  void DestroyVideoDecoder();
  void SequenceCallback(cnvideoDecSequenceInfo *pFormat);
  void VideoFrameCallback(cnvideoDecOutput *dec_output);
  void VideoEosCallback();
  void VideoResetCallback();

  bool CreateJpegDecoder(VideoStreamInfo *info);
  void DestroyJpegDecoder();
  void JpegFrameCallback(cnjpegDecOutput *dec_output);
  void JpegEosCallback();
  void JpegResetCallback();

 private:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  int ProcessFrame(cnvideoDecOutput *output, bool *reused);
  int ProcessJpegFrame(cnjpegDecOutput *output, bool *reused);

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
  class CNDeallocator : public cnstream::IDataDeallocator {
   public:
    explicit CNDeallocator(MluDecoder *decoder, cncodecFrame *frame) : decoder_(decoder), frame_(frame) {
      ++decoder_->cndec_buf_ref_count_;
    }
    ~CNDeallocator() {
      if (decoder_->instance_) {
        cnvideoDecReleaseReference(decoder_->instance_, frame_);
        --decoder_->cndec_buf_ref_count_;
      }
    }

   private:
    MluDecoder *decoder_;
    cncodecFrame *frame_;
  };

  // cnjpeg
  cnjpegDecCreateInfo create_jpg_info_;
  cnjpegDecoder jpg_instance_ = nullptr;
  class CNDeallocatorJpg : public cnstream::IDataDeallocator {
   public:
    explicit CNDeallocatorJpg(MluDecoder *decoder, cncodecFrame *frame) : decoder_(decoder), frame_(frame) {
      ++decoder_->cndec_buf_ref_count_;
    }
    ~CNDeallocatorJpg() {
      if (decoder_->jpg_instance_) {
        cnjpegDecReleaseReference(decoder_->jpg_instance_, frame_);
        --decoder_->cndec_buf_ref_count_;
      }
    }

   private:
    MluDecoder *decoder_;
    cncodecFrame *frame_;
  };
};

class FFmpegCpuDecoder : public Decoder {
 public:
  explicit FFmpegCpuDecoder(IHandler *handler) : Decoder(handler) {}
  ~FFmpegCpuDecoder() {}
  bool Create(VideoStreamInfo *info, int interval) override;
  bool Create(AVStream *st, int interval = 1) override;
  void Destroy() override;
  bool Process(ESPacket *pkt) override;
  bool Process(AVPacket *pkt, bool eos) override;

 private:
#ifdef UNIT_TEST

 public:
#endif
  /**
   * yuv420p convert to yuv420p nv12/nv21
   */
  bool FrameCvt2Yuv420sp(AVFrame *frame, uint8_t *sp, int dst_stride, bool nv21 = false);
  bool ProcessFrame(AVFrame *frame);

 private:
  AVStream *stream_ = nullptr;
  AVCodecContext *instance_ = nullptr;
  AVFrame *av_frame_ = nullptr;
  std::atomic<int> eos_got_{0};
  std::atomic<int> eos_sent_{0};
};  // class FFmpegCpuDecoder

}  // namespace cnstream

#endif  // MODULES_SOURCE_DECODER_HPP_
