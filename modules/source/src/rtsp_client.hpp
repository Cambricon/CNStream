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
#ifndef CNSTREAM_RTSP_CLIENT_H_
#define CNSTREAM_RTSP_CLIENT_H_

#include <string>

namespace cnstream {

struct FrameInfo {
  enum CodecType {
    INVALID,
    H264,
    H265
  } codec_type = INVALID;
  uint32_t pts;
  uint32_t flags;
  enum {FLAG_KEY_FRAME = 0x01};
};

struct IRtspCB {
  virtual void OnFrame(unsigned char *data, size_t size, FrameInfo *frameInfo) = 0;
  virtual void OnEvent(int type) = 0;
  virtual ~IRtspCB() {}
};

struct OpenParam {
  std::string url; /*rtsp://ip[:port]/stream_id
                    * rtsp://username:password@ip[:port]/stream_id
                    */
  bool streammingPreferTcp = true;
  int reconnect = 0;
  int livenessTimeoutMs = 2000;
  IRtspCB *cb = nullptr;
};

class RtspSessionImpl;
class RtspSession {
 public:
  RtspSession();
  ~RtspSession();

  int Open(const OpenParam &param);
  void Close();

 private:
  RtspSession(const RtspSession &) = delete;
  RtspSession &operator=(const RtspSession &) = delete;
  RtspSessionImpl *impl_ = nullptr;
};

}  // namespace cnstream

#endif  // CNSTREAM_RTSP_CLIENT_H_
