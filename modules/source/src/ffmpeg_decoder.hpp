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
#include <string>
#include <thread>
#include "cndecode/cndecode.h"
#include "cnstream_frame.hpp"
#include "cnstream_timer.hpp"
#include "cnvformat/cnvformat.h"
#include "data_handler.hpp"

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
  uint64_t GetDiscardNum() { return discard_frame_num_; }

 protected:
  std::string stream_id_;
  DataHandler &handler_;

  uint32_t stream_idx_;
  DevContext dev_ctx_;
  size_t interval_ = 1;
  size_t frame_count_ = 0;
  uint64_t frame_id_ = 0;
  uint64_t discard_frame_num_ = 0;
};

class FFmpegMluDecoder : public FFmpegDecoder {
 public:
  explicit FFmpegMluDecoder(DataHandler &handler) : FFmpegDecoder(handler) {}
  ~FFmpegMluDecoder() {
    Destroy();
    PrintPerformanceInfomation();
  }
  bool Create(AVStream *st) override;
  void Destroy() override;
  bool Process(AVPacket *pkt, bool eos) override;

 private:
  std::shared_ptr<libstream::CnDecode> instance_ = nullptr;
  libstream::CnPacket cn_packet_;
  std::atomic<int> eos_got_{0};
  void FrameCallback(const libstream::CnFrame &frame);
  void EOSCallback();
  int ProcessFrame(const libstream::CnFrame &frame, bool &reused);
  CNTimer fps_calculators[4];
  void PrintPerformanceInfomation() const {
    printf("stream_id: %s:\n", stream_id_.c_str());
    fps_calculators[0].PrintFps("transfer memory: ");
    fps_calculators[1].PrintFps("decode delay: ");
    fps_calculators[2].PrintFps("send data to codec: ");
    fps_calculators[3].PrintFps("output :");
  }
  void PerfCallback(const libstream::CnDecodePerfInfo &info) {
    fps_calculators[0].Dot(1.0f * info.transfer_us / 1000, 1);
    fps_calculators[1].Dot(1.0f * info.decode_us / 1000, 1);
    fps_calculators[2].Dot(1.0f * info.total_us / 1000, 1);
    fps_calculators[3].Dot(1);
  }

 private:
  class CNDeallocator : public cnstream::IDataDeallocator {
   public:
    explicit CNDeallocator(std::shared_ptr<libstream::CnDecode> decoder, uint32_t buf_id)
        : decoder_(decoder), buf_id_(buf_id) {}
    ~CNDeallocator() {
      // LOG(INFO) << "Buffer released :" << buf_id_;
      decoder_->ReleaseBuffer(buf_id_);
    }

   private:
    std::shared_ptr<libstream::CnDecode> decoder_;
    uint32_t buf_id_;
  };
};

class FFmpegCpuDecoder : public FFmpegDecoder {
 public:
  explicit FFmpegCpuDecoder(DataHandler &handler) : FFmpegDecoder(handler) {}
  ~FFmpegCpuDecoder() {
    Destroy();
    if (nullptr != nv21_data_) {
      delete[] nv21_data_, nv21_data_ = nullptr;
    }
  }
  bool Create(AVStream *st) override;
  void Destroy() override;
  bool Process(AVPacket *pkt, bool eos) override;

 private:
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
