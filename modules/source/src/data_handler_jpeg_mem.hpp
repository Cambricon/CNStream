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

#include "cnstream_logging.hpp"
#include "data_handler_util.hpp"
#include "data_source.hpp"
#include "util/video_decoder.hpp"

namespace cnstream {

class ESJpegMemHandlerImpl : public IDecodeResult, public SourceRender {
 public:
  explicit ESJpegMemHandlerImpl(DataSource *module, ESJpegMemHandler *handler, int max_width, int max_height)
      : SourceRender(handler),
        module_(module),
        stream_id_(handler->GetStreamId()),
        max_width_(max_width),
        max_height_(max_height) {}
  ~ESJpegMemHandlerImpl() {}

  bool Open();
  void Close();

  int Write(ESPacket *pkt);

  // IDecodeResult methods
  void OnDecodeError(DecodeErrorCode error_code) override;
  void OnDecodeFrame(DecodeFrame *frame) override;
  void OnDecodeEos() override;

 private:
  DataSource *module_ = nullptr;
  std::string stream_id_;
  DataSourceParam param_;

#ifdef UNIT_TEST
 public:  // NOLINT
#else
 private:  // NOLINT
#endif
  bool InitDecoder();
  bool ProcessImage(ESPacket *pkt);

 private:
  std::shared_ptr<Decoder> decoder_ = nullptr;
  // maximum resolution 8K
  int max_width_ = 7680;
  int max_height_ = 4320;

  RwLock running_lock_;
  std::atomic<bool> running_{false};
  std::atomic<bool> eos_reached_{false};

#ifdef UNIT_TEST
 public:  // NOLINT
  void SetDecodeParam(const DataSourceParam &param) { param_ = param; }
#endif
};  // class ESJpegMemHandlerImpl

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_JPEG_MEM_HPP_
