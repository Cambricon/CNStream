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

#ifndef __ENCODER_HANDLER_FFMPEG_HPP__
#define __ENCODER_HANDLER_FFMPEG_HPP__

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#ifdef __cplusplus
}
#endif

#include <atomic>
#include <condition_variable>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "cnedk_buf_surface.h"

#include "cnstream_frame_va.hpp"
#include "util/cnstream_queue.hpp"

#include "encode_handler.hpp"
#include "scaler/scaler.hpp"

namespace cnstream {

#define INVALID_TIMESTAMP ((uint64_t)UINT64_C(0x8000000000000000))

enum State {
  IDLE = 0,
  STARTING,
  RUNNING,
  STOPPING,
};

class VEncodeFFmpegHandler : public VencHandler{
 public:
  void SetParams(const VEncHandlerParam &param) { param_ = param; }

  int SendFrame(std::shared_ptr<CNFrameInfo> data) override;

  int SendFrame(Scaler::Buffer* data) override;

  VEncodeFFmpegHandler();
  ~VEncodeFFmpegHandler();

  int Init();
  int Stop();

 private:
  void Loop();
  void Destroy();
  int SendFrame(CNDataFramePtr frame, int timeout_ms);

 private:
  ThreadSafeQueue<AVFrame*> data_queue_;
  std::unique_ptr<std::promise<void>> eos_promise_ = nullptr;

  std::atomic<int> state_{IDLE};
  bool inited_ = false;
  std::mutex mutex_;

  std::thread thread_;
  uint32_t input_alignment_ = 32;

  ::AVPixelFormat av_pixel_format_ = AV_PIX_FMT_YUV420P;
  ::AVCodecID av_codec_id_ = AV_CODEC_ID_H264;
  AVCodecContext *av_codec_ctx_ = nullptr;
  AVCodec *av_codec_ = nullptr;
  AVDictionary *av_opts_ = nullptr;
  AVFrame *av_frame_ = nullptr;
  AVPacket *av_packet_ = nullptr;
};  // VEncodeFFmpegHandler

}  // namespace cnstream

#endif  // __ENCODER_HANDLER_FFMPEG_HPP__
