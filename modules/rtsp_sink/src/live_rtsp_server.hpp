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

#ifndef MODULES_RTSP_SINK_SRC_LIVE_RTSP_SERVER_HPP_
#define MODULES_RTSP_SINK_SRC_LIVE_RTSP_SERVER_HPP_

#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <UsageEnvironment.hh>
#include <liveMedia.hh>

#include <string>

#include "video_encoder.hpp"

namespace cnstream {

namespace RtspStreaming {

class LiveRtspServer {
 public:
  LiveRtspServer(VideoEncoder *encoder, int port, int http_port);
  ~LiveRtspServer();
  void Run();
  void SignalExit() { is_quit_ = 1; }

  void SetBitRate(uint64_t br) {
    kbit_rate_ = static_cast<unsigned int>(br / 1000);  // in kbs
  }
  void SetAccessControl(bool enable);

 private:
  VideoEncoder *video_encoder_;
  int port_number_;
  int http_tunneling_port_;
  char is_quit_;
  bool enable_password_;
  unsigned int kbit_rate_;  // in kbs
};                         // LiveRtspServer

}  // namespace RtspStreaming

}  // namespace cnstream

#endif  // __LIVE_RTSP_SERVER_HPP_
