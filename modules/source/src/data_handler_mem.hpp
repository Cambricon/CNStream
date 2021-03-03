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

#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>

#include "cnstream_logging.hpp"
#include "data_handler_util.hpp"
#include "data_source.hpp"
#include "util/video_decoder.hpp"
#include "util/video_parser.hpp"

namespace cnstream {

class ESMemHandlerImpl : public IParserResult, public IDecodeResult, public SourceRender {
 public:
  explicit ESMemHandlerImpl(DataSource *module, ESMemHandler *handler)  // NOLINT
    : SourceRender(handler), module_(module), handler_(*handler) {
      stream_id_ = handler_.GetStreamId();
  }

  ~ESMemHandlerImpl() {}

  bool Open();
  void Close();

  int SetDataType(ESMemHandler::DataType data_type) {
    data_type_ = data_type;
    int ret = -1;
    if (data_type_ == ESMemHandler::H264) {
      ret = parser_.Open(AV_CODEC_ID_H264, this);
    } else if (data_type_ == ESMemHandler::H265) {
      ret = parser_.Open(AV_CODEC_ID_HEVC, this);
    } else {
      LOGF(SOURCE) << "Unsupported data type " << data_type;
      ret = -1;
    }
    return ret;
  }

  int Write(ESPacket *pkt);
  int Write(unsigned char *data, int len);

  // IParserResult methods
  void OnParserInfo(VideoInfo *info) override;
  void OnParserFrame(VideoEsFrame *frame) override;

  // IDecodeResult methods
  void OnDecodeError(DecodeErrorCode error_code) override;
  void OnDecodeFrame(DecodeFrame *frame) override;
  void OnDecodeEos() override;

 private:
  DataSource *module_ = nullptr;
  ESMemHandler &handler_;
  std::string stream_id_;
  DataSourceParam param_;
  ESMemHandler::DataType data_type_ = ESMemHandler::INVALID;
  bool first_frame_ = true;

  std::mutex info_mutex_;
  VideoInfo info_;
  std::atomic<bool> info_set_{false};

 private:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  bool PrepareResources();
  void ClearResources();
  bool Process();
  bool Extract();
  void DecodeLoop();

 private:
  /**/
  std::atomic<bool> running_{false};
  std::thread thread_;
  bool eos_sent_ = false;
  std::atomic<bool> generate_pts_{false};
  uint64_t fake_pts_ = 0;

  EsParser parser_;
  BoundedQueue<std::shared_ptr<EsPacket>> *queue_ = nullptr;
  /*
   * Ensure that the queue_ is not deleted when the push is blocked.
   */
  std::mutex queue_mutex_;

  std::shared_ptr<Decoder> decoder_ = nullptr;
  uint64_t pts_ = 0;

  // for parsing es-block
  bool eos_reached_ = false;

#ifdef UNIT_TEST
 public:  // NOLINT
  void SetDecodeParam(const DataSourceParam &param) { param_ = param; }
#endif
};  // class ESMemHandlerImpl

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_MEM_HPP_
