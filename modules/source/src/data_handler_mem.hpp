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
#include <mutex>

#include "data_source.hpp"
#include "device/mlu_context.h"
#include "ffmpeg_decoder.hpp"
#include "glog/logging.h"
#include "ffmpeg_parser.hpp"
#include "data_handler_util.hpp"

namespace cnstream {

class ESMemHandlerImpl : public IHandler, public H2645NalSplitter {
 public:
  explicit ESMemHandlerImpl(DataSource *module, ESMemHandler &handler)  // NOLINT
    : module_(module), handler_(handler) {
      stream_id_ = handler_.GetStreamId();
  }

  ~ESMemHandlerImpl() {}

  bool Open();
  void Close();

  int SetDataType(ESMemHandler::DataType data_type) {
    data_type_ = data_type;
    int ret = -1;
    if (data_type_ == ESMemHandler::H264) {
      ret = parser_.Init("h264");
      this->SplitterInit(true);
    } else if (data_type_ == ESMemHandler::H265) {
      ret = parser_.Init("h265");
      this->SplitterInit(false);
    } else {
      ret = -1;
    }
    return ret;
  }

  int Write(ESPacket *pkt);
  int Write(unsigned char *data, int len);
  void SplitterOnNal(NalDesc &desc, bool eos) override;

 private:
  DataSource *module_ = nullptr;
  ESMemHandler &handler_;
  std::string stream_id_;
  DataSourceParam param_;
  size_t interval_ = 1;
  ESMemHandler::DataType data_type_ = ESMemHandler::INVALID;

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
  std::atomic<int> running_{0};
  std::thread thread_;
  bool eos_sent_ = false;

  ParserHelper parser_;
  BoundedQueue<std::shared_ptr<EsPacket>> *queue_ = nullptr;
  /*
   * Ensure that the queue_ is not deleted when the push is blocked.
   */
  std::mutex queue_mutex_;

  std::shared_ptr<Decoder> decoder_ = nullptr;
  uint64_t pts_ = 0;

 public:
  void SendFlowEos() override {
    if (eos_sent_) return;
    auto data = CreateFrameInfo(true);
    if (!data) {
      LOG(ERROR) << "SendFlowEos: Create CNFrameInfo failed while received eos. stream id is " << stream_id_;
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
};  // class ESMemHandlerImpl

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_MEM_HPP_
