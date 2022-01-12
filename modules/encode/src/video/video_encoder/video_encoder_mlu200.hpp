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

#ifndef __VIDEO_ENCODER_MLU200_HPP__
#define __VIDEO_ENCODER_MLU200_HPP__

#include <memory>

#include "video_encoder_base.hpp"

namespace cnstream {

namespace video {

struct VideoEncoderMlu200Private;

class VideoEncoderMlu200 : public VideoEncoderBase {
 public:
  using Param = cnstream::VideoEncoder::Param;
  using EventCallback = cnstream::VideoEncoder::EventCallback;
  using PacketInfo = cnstream::VideoEncoder::PacketInfo;

  explicit VideoEncoderMlu200(const Param &param);
  ~VideoEncoderMlu200();

  int Start() override;
  int Stop() override;

  /* timeout_ms: <0: wait infinitely; 0: poll; >0: timeout in milliseconds */
  int RequestFrameBuffer(VideoFrame *frame, int timeout_ms = -1) override;
  int SendFrame(const VideoFrame *frame, int timeout_ms = -1) override;
  int GetPacket(VideoPacket *packet, PacketInfo *info = nullptr) override;

  i32_t EventHandlerCallback(int event, void *data);
  i32_t EventHandler(int event, void *data);

 private:
  bool GetPacketInfo(int64_t index, PacketInfo *info) override;
  void ReceivePacket(void *data);
  void ReceiveEOS();
  i32_t ErrorHandler(int event);

  std::unique_ptr<VideoEncoderMlu200Private> priv_;
};  // VideoEncoderMlu200

}  // namespace video

}  // namespace cnstream

#endif  // __VIDEO_ENCODER_MLU200_HPP__
