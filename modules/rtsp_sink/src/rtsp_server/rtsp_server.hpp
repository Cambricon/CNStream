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

#ifndef __RTSP_SERVER_HPP__
#define __RTSP_SERVER_HPP__

#include <functional>
#include <thread>
#include <string>

namespace cnstream {

class RtspFramedSource;

class RtspServer {
 public:
  enum CodecType {
    H264 = 0,
    H265,
    MPEG4,
  };

  using GetPacket = std::function<int(uint8_t *, int, double *, int *)>;

  struct Param {
    int port = 8554;
    bool rtsp_over_http = false;
    bool stream_mode = false;
    bool authentication = false;
    std::string user_name, password;
    uint32_t width, height;
    uint32_t bit_rate;
    CodecType codec_type = H264;
    GetPacket get_packet = nullptr;
  };

  enum Event {
    EVENT_DATA = 0,
    EVENT_EOS,
  };

  explicit RtspServer(const Param &param);
  ~RtspServer();

  bool Start();
  bool Stop();

  void OnEvent(Event event);

  friend class RtspFramedSource;

 private:
  void Loop();

  Param param_;
  char quit_ = 1;
  std::thread thread_;
  RtspFramedSource *source_ = nullptr;
};  // RtspServer

}  // namespace cnstream

#endif  // __RTSP_SERVER_HPP__
