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

#ifndef __VIDEO_ENCODER_MLU_HPP__
#define __VIDEO_ENCODER_MLU_HPP__

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <unordered_map>

#include "cn_codec_common.h"
#include "cn_jpeg_enc.h"
#include "cn_video_enc.h"
#include "video_encoder_base.hpp"

namespace cnstream {

namespace video {

class VideoEncoderMlu : public VideoEncoderBase {
 public:
  using Param = cnstream::VideoEncoder::Param;
  using EventCallback = cnstream::VideoEncoder::EventCallback;
  using PacketInfo = cnstream::VideoEncoder::PacketInfo;

  explicit VideoEncoderMlu(const Param &param);
  ~VideoEncoderMlu();

  int Start() override;
  int Stop() override;

  /* timeout_ms: <0: wait infinitely; 0: poll; >0: timeout in millisceonds */
  int RequestFrameBuffer(VideoFrame *frame, int timeout_ms = -1) override;
  int SendFrame(const VideoFrame *frame, int timeout_ms = -1) override;
  int GetPacket(VideoPacket *packet, PacketInfo *info = nullptr) override;

  int EventHandler(cncodecCbEventType event, void *info);

 private:
  struct EncodingInfo {
    int64_t pts, dts;
    int64_t start_tick, end_tick;
  };

  void ReceivePacket(void *info);
  void ReceiveEOS();
  i32_t ErrorHandler(cncodecCbEventType event);
  bool GetPacketInfo(int64_t pts, PacketInfo *info) override;

  std::mutex list_mtx_;
  std::list<cnjpegEncInput> ji_list_;
  std::list<cnvideoEncInput> vi_list_;
  std::condition_variable list_cv_;
  std::mutex info_mtx_;
  std::unordered_map<int64_t, EncodingInfo> encoding_info_;
  std::mutex eos_mtx_;
  std::condition_variable eos_cv_;
  std::atomic<bool> eos_sent_{false};
  std::atomic<bool> eos_got_{false};
  std::atomic<bool> error_{false};
  uint8_t *stream_buffer_ = nullptr;
  uint32_t stream_buffer_size_ = 0;
  uint8_t *ps_buffer_ = nullptr;
  uint32_t ps_size_ = 0;
  int64_t frame_count_ = 0;
  int64_t packet_count_ = 0;
  int64_t data_index_ = 0;

  cnvideoEncCreateInfo ve_param_;
  cnjpegEncCreateInfo je_param_;
  void *cn_encoder_ = nullptr;
};  // VideoEncoderMlu

}  // namespace video

}  // namespace cnstream

#endif  // __VIDEO_ENCODER_MLU_HPP__
