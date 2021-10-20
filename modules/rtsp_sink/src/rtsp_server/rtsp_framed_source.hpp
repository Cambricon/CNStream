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

#ifndef __RTSP_FRAMED_SOURCE_HPP__
#define __RTSP_FRAMED_SOURCE_HPP__

#include <FramedSource.hh>
#include <Groupsock.hh>
#include <UsageEnvironment.hh>

#include "rtsp_server.hpp"

namespace cnstream {

class RtspFramedSource : public FramedSource {
 public:
  static RtspFramedSource *createNew(UsageEnvironment &env, RtspServer *server, Boolean discrete = True);  // NOLINT
  RtspFramedSource(UsageEnvironment &env, RtspServer *server, Boolean discrete = True);                    // NOLINT
  ~RtspFramedSource();

  friend class RtspServer;

 private:
  static void deliverFrameStub(void *clientData) { (reinterpret_cast<RtspFramedSource *>(clientData))->deliverFrame(); }
  void doGetNextFrame() override;
  void deliverFrame();
  void doStopGettingFrames() override;
  void onEvent(RtspServer::Event event);
  Boolean isKeyFrame(Boolean h264, const unsigned char *data, unsigned size);

  RtspServer *fServer = nullptr;
  Boolean fDiscrete = True;
  Boolean fFirstFrame = True;
  EventTriggerId fEventTriggerId = 0;
  struct timeval fInitTimestamp = {0, 0};
  double fInitPTS = -1;
};  // RtspFramedSource

}  // namespace cnstream

#endif  // __RTSP_FRAMED_SOURCE_HPP__
