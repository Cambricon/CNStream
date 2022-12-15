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

#include "rtsp_sink.hpp"

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <string>
#include <iostream>

#include "cnstream_logging.hpp"

namespace cnstream {

static inline int64_t CurrentTick() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

RtspSink::RtspSink() { }

RtspSink::~RtspSink() {
  if (input_buffer_) {
    delete input_buffer_;
    input_buffer_ = nullptr;
  }
  if (truncated_buffer_) {
    delete[] truncated_buffer_;
    truncated_buffer_ = nullptr;
  }
  if (server_) {
    server_.reset();
    server_ = nullptr;
  }
}

int RtspSink::Open(int rtsp_port) {
  if (!input_buffer_) input_buffer_ = new CircularBuffer();

  auto get_packet = [this](uint8_t *data, int size, uint64_t *pts) {
    CnedkVEncFrameBits packet;
    int ret = 0;

    std::unique_lock<std::mutex> lk(input_mtx_);
    int len = input_buffer_->Read(reinterpret_cast<uint8_t*>(&packet), sizeof(CnedkVEncFrameBits), true);
    if (len > 0) ret = packet.len;

    if (!data) return ret;  //  get packet size

    if (input_buffer_->Size() <= sizeof(CnedkVEncFrameBits)) {  //  there is no avaiable buffer
      return 0;
    }

    //  get buffer is lagger than one encode packet
    //  malloc a temp buffer, than return the temp buffer
    if (packet.len > size && packet.len > truncated_size_) {
      if (truncated_buffer_) delete[] truncated_buffer_;
      truncated_buffer_ = new uint8_t[packet.len];
      truncated_size_ = packet.len;
    }

    if (truncated_offset_ != 0) {
      int left_size = truncated_size_ - truncated_offset_;
      int temp_size = size < left_size ? size : left_size;
      memcpy(data, truncated_buffer_ + truncated_offset_, temp_size);
      truncated_offset_ = temp_size == left_size ? 0 : (truncated_offset_ + temp_size);
      if (pts) *pts = pts_;
      return temp_size;
    } else if (packet.len > size) {
      input_buffer_->Read(reinterpret_cast<uint8_t*>(&packet), sizeof(CnedkVEncFrameBits));
      ret = input_buffer_->Read(truncated_buffer_, packet.len);
      pts_ = packet.pts;
      if (pts) *pts = pts_;
      int left_size = truncated_size_ - truncated_offset_;
      int temp_size = size < left_size ? size : left_size;
      memcpy(data, truncated_buffer_ + truncated_offset_, temp_size);
      truncated_offset_ = temp_size == left_size ? 0 : (truncated_offset_ + temp_size);
      return temp_size;
    } else {
      input_buffer_->Read(reinterpret_cast<uint8_t*>(&packet), sizeof(CnedkVEncFrameBits));
      ret = input_buffer_->Read(data, packet.len);
      if (pts) *pts = packet.pts;
    }

    return ret;
  };

  RtspServer::Param rparam;
  rparam.port = rtsp_port;
  // rparam.rtsp_over_http = params.rtsp_over_http;
  rparam.authentication = false;
  rparam.bit_rate = 8000000;
  rparam.codec_type = RtspServer::H264;
  rparam.get_packet = std::bind(get_packet, std::placeholders::_1, std::placeholders::_2,
                                std::placeholders::_3);

  server_.reset(new RtspServer(rparam));
  if (!server_->Start()) {
    LOGE(RTSP) << "Rtsp server start failed";
    return -1;
  }
  return 0;
}

int RtspSink::Close() {
  server_->Stop();
  return 0;
}

int RtspSink::SendFrame(CnedkVEncFrameBits *framebits) {
  std::unique_lock<std::mutex> lk(input_mtx_);
  if (input_buffer_->Size() * 100 / input_buffer_->Capacity() > 90) {
    CnedkVEncFrameBits packet;
    input_buffer_->Read(reinterpret_cast<uint8_t*>(&packet), sizeof(packet));
    input_buffer_->Read(nullptr, packet.len);
  }

  size_t written_size = input_buffer_->Write(reinterpret_cast<uint8_t*>(framebits), sizeof(CnedkVEncFrameBits));
  written_size += input_buffer_->Write(framebits->bits, framebits->len);
  lk.unlock();
  if (server_) server_->OnEvent(RtspServer::Event::EVENT_DATA);
  return 0;
}

}  // namespace cnstream
