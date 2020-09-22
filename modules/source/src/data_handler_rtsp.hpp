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

#ifndef MODULES_SOURCE_HANDLER_RTSP_HPP_
#define MODULES_SOURCE_HANDLER_RTSP_HPP_

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
}
#endif

#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "data_source.hpp"
#include "device/mlu_context.h"
#include "ffmpeg_decoder.hpp"
#include "cnstream_logging.hpp"
#include "rtsp_client.hpp"
#include "data_handler_util.hpp"
#include "util/cnstream_queue.hpp"

#define DEFAULT_MODULE_CATEGORY SOURCE

namespace cnstream {

class RtspHandlerImpl : public IHandler {
 public:
  explicit RtspHandlerImpl(DataSource *module, const std::string &url_name, RtspHandler &handler, // NOLINT
                           bool use_ffmpeg, int reconnect)
      : module_(module), url_name_(url_name), handler_(handler), use_ffmpeg_(use_ffmpeg), reconnect_(reconnect) {
    stream_id_ = handler_.GetStreamId();
  }
  ~RtspHandlerImpl() {}
  bool Open();
  void Close();

 private:
  DataSource *module_ = nullptr;
  std::string url_name_;
  RtspHandler &handler_;
  std::string stream_id_;
  DataSourceParam param_;
  size_t interval_ = 1;
  bool use_ffmpeg_ = false;
  int reconnect_ = 0;

 private:
  /**/
  std::atomic<int> demux_exit_flag_ {0};
  std::thread demux_thread_;
  std::atomic<int> decode_exit_flag_{0};
  std::thread decode_thread_;
  bool eos_sent_ = false;

 private:
  std::mutex mutex_;
  bool stream_info_set_ = false;
  VideoStreamInfo stream_info_;
  BoundedQueue<std::shared_ptr<EsPacket>> *queue_ = nullptr;
  void DemuxLoop();
  void DecodeLoop();

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
};  // class RtspHandlerImpl

}  // namespace cnstream

#undef DEFAULT_MODULE_CATEGORY
#endif  // MODULES_SOURCE_HANDLER_RTSP_HPP_
