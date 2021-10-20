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

#ifndef __VIDEO_SINK_H__
#define __VIDEO_SINK_H__

#include <string>

#include "../video_common.hpp"

namespace cnstream {

namespace video { class VideoSink; }

class VideoSink {
 public:
  enum ReturnCode {
    SUCCESS = 0,
    ERROR_FAILED = -1,
    ERROR_STATE = -2,
    ERROR_PARAMETERS = -3,
  };

  struct Param {
    std::string file_name;
    uint32_t width, height;
    double frame_rate;
    uint32_t time_base;
    uint32_t bit_rate;
    uint32_t gop_size;
    VideoPixelFormat pixel_format = VideoPixelFormat::I420;
    VideoCodecType codec_type = VideoCodecType::H264;
    bool start_from_key_frame = true;
  };

  explicit VideoSink(const Param &param);
  ~VideoSink();

  VideoSink(const VideoSink &) = delete;
  VideoSink &operator=(const VideoSink &) = delete;

  VideoSink(VideoSink &&);
  VideoSink &operator=(VideoSink &&);

  int Start();
  int Stop();
  int Write(const VideoPacket *packet);

 private:
  video::VideoSink *sink_ = nullptr;
};  // VideoSink

}  // namespace cnstream

#endif  // __VIDEO_SINK_H__
