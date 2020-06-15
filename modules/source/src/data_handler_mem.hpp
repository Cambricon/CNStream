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

#ifndef MODULES_SOURCE_HANDLER_MEM_HPP_
#define MODULES_SOURCE_HANDLER_MEM_HPP_

#include <string>
#include <thread>
#include <memory>
#include <utility>

#include "data_source.hpp"
#include "cnstream_frame.hpp"
#include "cnstream_pipeline.hpp"
#include "cnstream_source.hpp"
#include "data_handler.hpp"
#include "ffmpeg_decoder.hpp"
#include "threadsafe_queue.hpp"

namespace cnstream {

struct IOBuffer;

class DataHandlerMem : public DataHandler {
 public:
  explicit DataHandlerMem(DataSource *module, std::string stream_id,
                          const std::string& filename, int frame_rate = 0)
    :DataHandler(module, stream_id, frame_rate, false),
    // filename_(filename) {}
     module_(module), stream_id_(stream_id), filename_(filename) {}
  ~DataHandlerMem() {}
  int Write(unsigned char *buf, int size);

 private:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  bool PrepareResources(bool demux_only = false);
  void ClearResources(bool demux_only = false);
  bool Process();
  bool Extract();

 private:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  DataSource *module_ = nullptr;
  std::string stream_id_;
  std::string filename_;

 private:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  AVFormatContext* p_format_ctx_ = nullptr;
  AVIOContext *avio_ = nullptr;
  static constexpr int io_buffer_size_ = 32768;
  unsigned char *io_buffer_;
  ThreadSafeQueue<std::shared_ptr<IOBuffer>> queue_;

  AVPacket packet_;
  int video_index_ = -1;
  bool first_frame_ = true;
  bool find_pts_ = true;  // set it to true by default!
  int64_t pts_ = 0;
  std::shared_ptr<FFmpegDecoder> decoder_ = nullptr;
};

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_MEM_HPP_
