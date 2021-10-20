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

#ifndef __VIDEO_ENCODER_FFMPEG_HPP__
#define __VIDEO_ENCODER_FFMPEG_HPP__

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

#include <condition_variable>
#include <list>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include "video_encoder_base.hpp"

namespace cnstream {

namespace video {

class VideoEncoderFFmpeg : public VideoEncoderBase {
 public:
  using Param = cnstream::VideoEncoder::Param;
  using EventCallback = cnstream::VideoEncoder::EventCallback;
  using PacketInfo = cnstream::VideoEncoder::PacketInfo;

  explicit VideoEncoderFFmpeg(const Param &param);
  ~VideoEncoderFFmpeg();

  int Start() override;
  int Stop() override;

  /* timeout_ms: <0: wait infinitely; 0: poll; >0: timeout in millisceonds */
  int RequestFrameBuffer(VideoFrame *frame, int timeout_ms = -1) override;
  int SendFrame(const VideoFrame *frame, int timeout_ms = -1) override;
  int GetPacket(VideoPacket *packet, PacketInfo *info = nullptr) override;

 private:
  struct EncodingInfo {
    int64_t pts, dts;
    int64_t start_tick, end_tick;
  };

  bool GetPacketInfo(int64_t pts, PacketInfo *info) override;
  void Loop();
  void Destroy();

  std::thread thread_;
  std::mutex input_mtx_;
  std::condition_variable data_cv_;
  std::condition_variable free_cv_;
  std::queue<AVFrame *> data_q_;
  std::queue<AVFrame *> free_q_;
  std::list<AVFrame *> list_;
  std::mutex info_mtx_;
  std::unordered_map<int64_t, EncodingInfo> encoding_info_;
  std::atomic<bool> eos_got_{false};
  std::atomic<bool> eos_sent_{false};
  std::atomic<bool> encoding_{false};
  int64_t frame_count_ = 0;
  int64_t packet_count_ = 0;
  int64_t data_index_ = 0;
  uint32_t input_alignment_ = 32;

  ::AVPixelFormat pixel_format_ = AV_PIX_FMT_YUV420P;
  ::AVCodecID codec_id_ = AV_CODEC_ID_H264;
  AVCodecContext *codec_ctx_ = nullptr;
  AVCodec *codec_ = nullptr;
  AVDictionary *opts_ = nullptr;
  AVFrame *frame_ = nullptr;
  AVPacket *packet_ = nullptr;
  SwsContext *sws_ctx_ = nullptr;
};  // VideoEncoderFFmpeg

}  // namespace video

}  // namespace cnstream

#endif  // __VIDEO_ENCODER_FFMPEG_HPP__
