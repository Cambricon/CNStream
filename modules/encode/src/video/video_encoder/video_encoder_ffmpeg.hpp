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
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "video_encoder_base.hpp"

namespace cnstream {

namespace video {

struct VideoEncoderFFmpegPrivate;

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
  bool GetPacketInfo(int64_t index, PacketInfo *info) override;
  void Loop();
  void Destroy();

  std::unique_ptr<VideoEncoderFFmpegPrivate> priv_;
};  // VideoEncoderFFmpeg

}  // namespace video

}  // namespace cnstream

#endif  // __VIDEO_ENCODER_FFMPEG_HPP__
