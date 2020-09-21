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

#ifndef MODULES_SOURCE_HANDLER_FILE_HPP_
#define MODULES_SOURCE_HANDLER_FILE_HPP_
#ifdef HAVE_FFMPEG_AVDEVICE
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavdevice/avdevice.h>
#ifdef __cplusplus
}
#endif

#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "data_source.hpp"
#include "device/mlu_context.h"
#include "ffmpeg_decoder.hpp"
#include "cnstream_logging.hpp"
#include "perf_manager.hpp"

#define DEFAULT_MODULE_CATEGORY SOURCE

namespace cnstream {

class UsbHandlerImpl : public IHandler {
 public:
  explicit UsbHandlerImpl(DataSource *module, const std::string &filename, int framerate, bool loop,
                           UsbHandler &handler)  // NOLINT
      : module_(module), filename_(filename), framerate_(framerate), loop_(loop), handler_(handler) {
    stream_id_ = handler_.GetStreamId();
  }
  ~UsbHandlerImpl() {}
  bool Open();
  void Close();

 private:
  DataSource *module_ = nullptr;
  std::shared_ptr<PerfManager> perf_manager_;
  std::string filename_;
  int framerate_;
  bool loop_ = false;
  UsbHandler &handler_;
  std::string stream_id_;
  DataSourceParam param_;
  size_t interval_ = 1;

 private:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  bool PrepareResources(bool demux_only = false);
  void ClearResources(bool demux_only = false);
  bool Process();
  bool Extract();
  void Loop();

  /**/
  std::atomic<int> running_{0};
  std::thread thread_;
  bool eos_sent_ = false;

 private:
  // ffmpeg demuxer
  AVFormatContext *p_format_ctx_ = nullptr;
  AVBitStreamFilterContext *bitstream_filter_ctx_ = nullptr;
  AVDictionary *options_ = NULL;
  AVPacket packet_;
  int video_index_ = -1;
  bool first_frame_ = true;
  bool find_pts_ = true;  // set it to true by default!
  uint64_t pts_ = 0;
  std::shared_ptr<Decoder> decoder_ = nullptr;

 public:
  void SendFlowEos() override {
    if (eos_sent_) return;
    auto data = CreateFrameInfo(true);
    if (!data) {
      MLOG(ERROR) << "SendFlowEos: Create CNFrameInfo failed while received eos. stream id is " << stream_id_;
      return;
    }
    SendFrameInfo(data);
    eos_sent_ = true;
  }

  std::shared_ptr<CNFrameInfo> CreateFrameInfo(bool eos = false) override {
    return handler_.CreateFrameInfo(eos);
  }

  bool SendFrameInfo(std::shared_ptr<CNFrameInfo> data) override {
    return handler_.SendData(data);
  }

  const DataSourceParam& GetDecodeParam() const override {
    return param_;
  }
#ifdef UNIT_TEST
 public:  // NOLINT
  void SetDecodeParam(const DataSourceParam &param) { param_ = param; }
#endif
};  // class UsbHandlerImpl

}  // namespace cnstream

#undef DEFAULT_MODULE_CATEGORY
#endif  // HAVE_FFMPEG_AVDEVICE
#endif  // MODULES_SOURCE_HANDLER_FILE_HPP_

