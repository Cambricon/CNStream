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

#ifndef MODULES_SOURCE_HANDLER_JPEG_MEM_HPP_
#define MODULES_SOURCE_HANDLER_JPEG_MEM_HPP_

#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>

#include "data_source.hpp"
#include "easyinfer/mlu_context.h"
#include "ffmpeg_decoder.hpp"
#include "glog/logging.h"
#include "ffmpeg_parser.hpp"
#include "data_handler_util.hpp"

namespace cnstream {

class ESJpegMemHandlerImpl : public IHandler {
 public:
  explicit ESJpegMemHandlerImpl(DataSource *module, ESJpegMemHandler &handler,  // NOLINT
                                int max_width, int max_height)
    : module_(module), handler_(handler), max_width_(max_width), max_height_(max_height) {
      stream_id_ = handler_.GetStreamId();
  }

  ~ESJpegMemHandlerImpl() {}

  bool Open();
  void Close();

  int Write(ESPacket *pkt);

 private:
  DataSource *module_ = nullptr;
  ESJpegMemHandler &handler_;
  std::string stream_id_;
  DataSourceParam param_;
  size_t interval_ = 1;

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

  BoundedQueue<std::shared_ptr<EsPacket>> *queue_ = nullptr;
  /*
   * Ensure that the queue_ is not deleted when the push is blocked.
   */
  std::mutex queue_mutex_;

  std::shared_ptr<Decoder> decoder_ = nullptr;
  uint64_t pts_ = 0;
  // maximum resolution 8K
  int max_width_ = 7680;
  int max_height_ = 4320;

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
};  // class ESJpegMemHandlerImpl

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_JPEG_MEM_HPP_
