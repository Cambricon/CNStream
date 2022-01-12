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

#ifndef __VIDEO_ENCODER_BASE_H__
#define __VIDEO_ENCODER_BASE_H__

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "../rw_mutex.hpp"
#include "../circular_buffer.hpp"
#include "video_encoder.hpp"

namespace cnstream {

namespace video {

struct IndexedVideoPacket {
  VideoPacket packet;
  int64_t index;
};

class VideoEncoderBase {
 public:
  using Param = cnstream::VideoEncoder::Param;
  using EventCallback = cnstream::VideoEncoder::EventCallback;
  using PacketInfo = cnstream::VideoEncoder::PacketInfo;

  explicit VideoEncoderBase(const Param &param);
  virtual ~VideoEncoderBase();

  virtual int Start();
  virtual int Stop();

  /* timeout_ms: <0: wait infinitely; 0: poll; >0: millisceonds of timeout */
  virtual int RequestFrameBuffer(VideoFrame *frame, int timeout_ms = -1) = 0;
  virtual int SendFrame(const VideoFrame *frame, int timeout_ms = -1) = 0;
  virtual int GetPacket(VideoPacket *packet, PacketInfo *info = nullptr);

  void SetEventCallback(EventCallback func) {
    std::lock_guard<std::mutex> lk(cb_mtx_);
    event_callback_ = func;
  }

 protected:
  enum State {
    IDLE = 0,
    STARTING,
    RUNNING,
    STOPPING,
  };

  bool PushBuffer(IndexedVideoPacket *packet);
  virtual bool GetPacketInfo(int64_t index, PacketInfo *info) = 0;

  Param param_;
  RwMutex state_mtx_;
  std::atomic<int> state_{IDLE};
  std::mutex cb_mtx_;
  EventCallback event_callback_ = nullptr;

 private:
  std::mutex output_mtx_;
  std::condition_variable output_cv_;
  CircularBuffer *output_buffer_ = nullptr;
  VideoPacket truncated_packet_;
  size_t truncated_buffer_size_ = 0;
  size_t truncated_size_ = 0;
  PacketInfo truncated_info_;
};  // VideoEncoderBase

}  // namespace video

}  // namespace cnstream

#endif  // __VIDEO_ENCODER_BASE_H__
