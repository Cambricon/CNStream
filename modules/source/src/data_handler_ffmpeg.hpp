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

#ifndef MODULES_SOURCE_HANDLER_FFMPEG_HPP_
#define MODULES_SOURCE_HANDLER_FFMPEG_HPP_

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
}
#endif

#include <string>
#include <thread>
#include "data_handler.hpp"
#include "data_source.hpp"
#include "ffmpeg_decoder.hpp"

namespace cnstream {

class DataHandlerFFmpeg : public DataHandler {
 public:
  explicit DataHandlerFFmpeg(DataSource* module, const std::string& stream_id, const std::string& filename,
                             int framerate, bool loop)
      : DataHandler(module, stream_id, framerate, loop), filename_(filename) {}
  ~DataHandlerFFmpeg() { Close(); }

 public:
  bool CheckTimeOut(uint64_t ul_current_time);

 private:
  // ffmpeg demuxer
  std::string filename_;
  AVFormatContext* p_format_ctx_ = nullptr;
  AVBitStreamFilterContext* bitstream_filter_ctx_ = nullptr;
  AVDictionary* options_ = NULL;
  AVPacket packet_;
  int video_index_ = -1;
  bool first_frame_ = true;
  uint64_t last_receive_frame_time_ = 0;
  uint8_t max_receive_time_out_ = 3;
  bool find_pts_ = true;  // set it to true by default!

 private:
  bool PrepareResources() override;
  void ClearResources() override;
  bool Process() override;
  bool Extract();
  size_t process_state_ = 0;

 private:
  std::shared_ptr<FFmpegDecoder> decoder_ = nullptr;
};

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_FFMPEG_HPP_
