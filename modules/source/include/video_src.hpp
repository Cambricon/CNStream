/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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

#ifndef MODULES_SOURCE_INCLUDE_VIDEO_SRC_HPP_
#define MODULES_SOURCE_INCLUDE_VIDEO_SRC_HPP_

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif
#include <string>
#include <thread>
#include "stream_src.hpp"

/*****************************************************************************
 * @brief VideoSrc is a child class of StreamSrc.
 *        It is used to extract frames of videos, whose storage path is added
 *        by pipeline.
 * Each frame will be sent at frame rate to coedc by calling callback.
 *****************************************************************************/
class VideoSrc : public StreamSrc {
 public:
  VideoSrc() {}
  explicit VideoSrc(const std::string& url) : StreamSrc(url) {}
  virtual ~VideoSrc() {}
  bool CheckTimeOut(uint64_t ulCurrentTime);
  bool Open() override;
  void Close() override;

 protected:
  virtual bool PrepareResources();
  virtual void ClearResources();
  virtual bool Extract(libstream::CnPacket* pdata);
  virtual void ReleaseData(libstream::CnPacket* pdata);

 private:
  void ExtractingLoop();
  std::thread thread_;
  bool running_ = false;
  AVFormatContext* p_format_ctx_ = nullptr;
  AVBitStreamFilterContext* bitstream_filter_ctx_ = nullptr;
  AVDictionary* options_ = NULL;
  AVPacket packet_;
  int video_index_ = -1;
  bool first_frame_ = true;
  uint64_t last_receive_frame_time_ = 0;
  uint8_t max_receive_time_out_ = 3;
  bool find_pts_ = true;  // set it to true by default!
};                        // class VideoSrc

#endif  // MODULES_SOURCE_INCLUDE_VIDEO_SRC_HPP_
