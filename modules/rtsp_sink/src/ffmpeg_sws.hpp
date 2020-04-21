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

#ifndef MODULES_RTSP_SINK_SRC_FFMPEG_SWS_HPP_
#define MODULES_RTSP_SINK_SRC_FFMPEG_SWS_HPP_

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>  // av_image_alloc
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <mutex>
#include <string>

namespace cnstream {

class FFSws {
 public:
  virtual ~FFSws();
  enum Status { STOP, LOCKED };
  int SetSrcOpt(AVPixelFormat pix_fmt, int w, int h);
  int SetDstOpt(AVPixelFormat pix_fmt, int w, int h);
  int LockOpt();
  int UnlockOpt();
  int Convert(const uint8_t* const src_slice[], const int src_stride[],
              int src_slice_y, int src_slice_h, uint8_t* const dst[],
              const int dst_stride[]);
  int Convert(const uint8_t* src_buffer, const size_t src_buffer_size,
              uint8_t* dst_buffer, const size_t dst_buffer_size);

 private:
  Status status_ = STOP;
  std::recursive_mutex mutex_;
  SwsContext* sws_ctx_ = nullptr;
  AVFrame* src_pic_ = nullptr;
  AVFrame* dst_pic_ = nullptr;
  AVPixelFormat src_pix_fmt_ = AV_PIX_FMT_NONE;
  AVPixelFormat dst_pix_fmt_ = AV_PIX_FMT_NONE;
  int src_w_ = 0;
  int src_h_ = 0;
  int dst_w_ = 0;
  int dst_h_ = 0;
};

}  // namespace cnstream

#endif  // MODULES_RTSP_SINK_SRC_FFMPEG_SWS_HPP_
