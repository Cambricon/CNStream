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

#ifndef MODULES_RTSP_SINK_SRC_RTSP_FRAME_SOURCE_HPP_
#define MODULES_RTSP_SINK_SRC_RTSP_FRAME_SOURCE_HPP_

#include <FramedSource.hh>
#include <Groupsock.hh>
#include <UsageEnvironment.hh>

#include <functional>

#include "video_encoder.hpp"

namespace cnstream {

namespace RtspStreaming {

class RtspFrameSource : public FramedSource {
 public:
  static RtspFrameSource* createNew(UsageEnvironment& env, VideoEncoder* encoder);  // NOLINT
  RtspFrameSource(UsageEnvironment& env, VideoEncoder* encoder);                    // NOLINT
  virtual ~RtspFrameSource();

 private:
  static void DeliverFrameStub(void* client_data) { reinterpret_cast<RtspFrameSource*>(client_data)->DeliverFrame(); }
  virtual void doGetNextFrame();
  void DeliverFrame();
  virtual void doStopGettingFrames();
  void OnEncoderEvent(VideoEncoder::Event event);

  VideoEncoder* encoder_;
  EventTriggerId event_trigger_id_ = 0;
  struct timeval init_timestamp_;
};  // class RtspFrameSource

}  // namespace RtspStreaming

}  // namespace cnstream

#endif  // MODULES_RTSP_SINK_SRC_RTSP_FRAME_SOURCE_HPP_
