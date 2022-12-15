/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_RTSP_SINK_HPP_
#define MODULES_RTSP_SINK_HPP_

#include <memory>
#include <mutex>
#include <string>

#include "circular_buffer.hpp"
#include "rtsp_server.hpp"

#include "cnedk_encode.h"

namespace cnstream {

class RtspSink {
 public:
  RtspSink();
  ~RtspSink();
  int Open(int rtsp_port);
  int Close();
  int SendFrame(CnedkVEncFrameBits *framebits);

 private:
  RtspSink(const RtspSink &) = delete;
  RtspSink(RtspSink &&) = delete;
  RtspSink &operator=(const RtspSink &) = delete;
  RtspSink &operator=(RtspSink &&) = delete;

 private:
  int truncated_size_ = 0;
  uint8_t* truncated_buffer_ = nullptr;
  int truncated_offset_ = 0;
  uint64_t pts_ = 0;
  std::mutex input_mtx_;
  ::CircularBuffer *input_buffer_ = nullptr;
  std::unique_ptr<RtspServer> server_ = nullptr;
};

}  // namespace cnstream

#endif
