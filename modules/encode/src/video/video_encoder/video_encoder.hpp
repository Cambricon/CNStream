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

#ifndef __VIDEO_ENCODER_H__
#define __VIDEO_ENCODER_H__

#include <functional>

#include "../video_common.hpp"

namespace cnstream {

namespace video { class VideoEncoderBase; }

class VideoEncoder {
 public:
  enum ReturnCode {
    SUCCESS = 0,
    ERROR_FAILED = -1,
    ERROR_STATE = -2,
    ERROR_PARAMETERS = -3,
    ERROR_TIMEOUT = -4,
  };

  struct Param {
    uint32_t width, height;
    double frame_rate;
    uint32_t time_base;
    uint32_t bit_rate;
    uint32_t gop_size;
    uint32_t jpeg_quality = 50;
    VideoPixelFormat pixel_format = VideoPixelFormat::I420;
    VideoCodecType codec_type = VideoCodecType::H264;
    uint32_t input_buffer_count = 6;
    uint32_t output_buffer_size = 0x100000;
    int mlu_device_id = -1;
  };

  struct PacketInfo {
    int64_t start_tick;
    int64_t end_tick;
    size_t buffer_size;
    size_t buffer_capacity;
  };

  enum Event {
    EVENT_DATA = 0,
    EVENT_EOS,
    EVENT_ERROR,
  };

  using EventCallback = std::function<void(Event)>;

  explicit VideoEncoder(const Param &param);
  ~VideoEncoder();

  // no copying
  VideoEncoder(const VideoEncoder &) = delete;
  VideoEncoder &operator=(const VideoEncoder &) = delete;

  VideoEncoder(VideoEncoder &&);
  VideoEncoder &operator=(VideoEncoder &&);

  int Start();
  int Stop();

  /* timeout_ms: <0: wait infinitely; 0: poll; >0: milliseconds of timeout */
  int RequestFrameBuffer(VideoFrame *frame, int timeout_ms = -1);
  int SendFrame(const VideoFrame *frame, int timeout_ms = -1);
  int GetPacket(VideoPacket *packet, PacketInfo *info = nullptr);

  void SetEventCallback(EventCallback func);

 private:
  video::VideoEncoderBase *encoder_ = nullptr;
};  // VideoEncoder

}  // namespace cnstream

#endif  // __VIDEO_ENCODER_H__
