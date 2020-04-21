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
#ifndef MODULES_RTSP_SINK_SRC_RTSP_MEDIA_SUBSESSION_HPP_
#define MODULES_RTSP_SINK_SRC_RTSP_MEDIA_SUBSESSION_HPP_

#include <Groupsock.hh>
#include <H264VideoRTPSink.hh>
#include <H264VideoStreamDiscreteFramer.hh>
#include <H264VideoStreamFramer.hh>
#include <OnDemandServerMediaSubsession.hh>
#include <StreamReplicator.hh>
#include <UsageEnvironment.hh>

#include "rtsp_frame_source.hpp"

namespace cnstream {

namespace RtspStreaming {

class RtspMediaSubsession : public OnDemandServerMediaSubsession {
 public:
  static RtspMediaSubsession* createNew(UsageEnvironment& env, StreamReplicator* replicator);  // NOLINT

  void SetBitRate(uint64_t br) {
    if (br > 500 * 1000) {
      kbit_rate_ = static_cast<uint32_t>(br / 1000);
    } else {
      kbit_rate_ = 500;  // 500k
    }
  }

 protected:
  RtspMediaSubsession(UsageEnvironment& env, StreamReplicator* replicator)  // NOLINT
      : OnDemandServerMediaSubsession(env, False), m_replicator_(replicator), kbit_rate_(1000) {}
  virtual ~RtspMediaSubsession() {}
  virtual FramedSource* createNewStreamSource(unsigned int client_session_id, unsigned int& est_bit_rate);  // NOLINT
  virtual RTPSink* createNewRTPSink(Groupsock* rtp_groupsock, unsigned char rtp_payload_type_if_dynamic,
                                    FramedSource* input_source);
  StreamReplicator* m_replicator_;
  uint32_t kbit_rate_;
};

}  // namespace RtspStreaming

}  // namespace cnstream

#endif  // MODULES_RTSP_SINK_SRC_RTSP_MEDIA_SUBSESSION_HPP_
