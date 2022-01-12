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

#ifndef __VIDEO_STREAM_HPP__
#define __VIDEO_STREAM_HPP__

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#if (CV_MAJOR_VERSION >= 3)
#include <opencv2/imgcodecs/imgcodecs.hpp>
#endif

#include <string>

#include "tiler/tiler.hpp"
#include "../video_encoder/video_encoder.hpp"

#include "../video_common.hpp"

namespace cnstream {

namespace video { class VideoStream; }

class VideoStream {
 public:
  using PacketInfo = VideoEncoder::PacketInfo;
  using Event = VideoEncoder::Event;
  using EventCallback = VideoEncoder::EventCallback;
  using ColorFormat = Tiler::ColorFormat;
  using Buffer = Tiler::Buffer;
  using Rect = Tiler::Rect;

  struct Param {
    int width;
    int height;
    int tile_cols;
    int tile_rows;
    double frame_rate;
    int time_base;
    int bit_rate;
    int gop_size;
    VideoPixelFormat pixel_format = VideoPixelFormat::NV21;
    VideoCodecType codec_type = VideoCodecType::H264;
    bool mlu_encoder = true;
    bool resample = true;
    int device_id = -1;
  };

  explicit VideoStream(const Param &param);
  ~VideoStream();

  // no copying
  VideoStream(const VideoStream &) = delete;
  VideoStream &operator=(const VideoStream &) = delete;

  VideoStream(VideoStream &&);
  VideoStream &operator=(VideoStream &&);

  bool Open();
  bool Close(bool wait_finish = false);
  bool Update(const cv::Mat &mat, ColorFormat color, int64_t timestamp, const std::string &stream_id,
              void *user_data = nullptr);
  bool Update(const Buffer *buffer, int64_t timestamp, const std::string &stream_id, void *user_data = nullptr);
  bool Clear(const std::string &stream_id);

  void SetEventCallback(EventCallback func);
  int RequestFrameBuffer(VideoFrame *frame);
  int GetPacket(VideoPacket *packet, PacketInfo *info = nullptr);

 private:
  video::VideoStream *stream_ = nullptr;
};

}  // namespace cnstream

#endif  // __VIDEO_STREAM_HPP__
