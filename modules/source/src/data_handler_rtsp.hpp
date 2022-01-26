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

#include "cnstream_logging.hpp"
#include "data_handler_util.hpp"
#include "data_source.hpp"
#include "util/rtsp_client.hpp"
#include "util/cnstream_queue.hpp"
#include "util/video_decoder.hpp"

namespace cnstream {

class RtspHandlerImpl : public IDecodeResult, public SourceRender {
 public:
  explicit RtspHandlerImpl(DataSource *module, const std::string &url_name, RtspHandler *handler, bool use_ffmpeg,
                           int reconnect, const MaximumVideoResolution &maximum_resolution,
                           std::function<void(ESPacket, std::string)> callback)
      : SourceRender(handler),
        module_(module),
        url_name_(url_name),
        handler_(handler),
        use_ffmpeg_(use_ffmpeg),
        reconnect_(reconnect),
        maximum_resolution_(maximum_resolution),
        save_es_packet_(callback) {
    stream_id_ = handler_->GetStreamId();
  }
  ~RtspHandlerImpl() {}
  bool Open();
  void Close();

  // IDecodeResult methods
  void OnDecodeError(DecodeErrorCode error_code) override;
  void OnDecodeFrame(DecodeFrame *frame) override;
  void OnDecodeEos() override;

 private:
  DataSource *module_ = nullptr;
  std::string url_name_;
  RtspHandler *handler_;
  std::string stream_id_;
  DataSourceParam param_;
  bool use_ffmpeg_ = false;
  int reconnect_ = 0;
  MaximumVideoResolution maximum_resolution_;
  std::atomic<int> demux_exit_flag_ {0};
  std::thread demux_thread_;
  std::atomic<int> decode_exit_flag_{0};
  std::thread decode_thread_;
  std::atomic<bool> stream_info_set_{false};
  std::mutex mutex_;
  VideoInfo stream_info_;
  BoundedQueue<std::shared_ptr<EsPacket>> *queue_ = nullptr;
  void DemuxLoop();
  void DecodeLoop();
  std::function<void(cnstream::ESPacket, std::string)> save_es_packet_ = nullptr;

#ifdef UNIT_TEST
 public:  // NOLINT
  void SetDecodeParam(const DataSourceParam &param) { param_ = param; }
#endif
};  // class RtspHandlerImpl

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_RTSP_HPP_
