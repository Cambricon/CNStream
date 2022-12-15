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

#ifndef ___RTSP_MEDIA_SUBSESSION_HPP__
#define ___RTSP_MEDIA_SUBSESSION_HPP__

#include <Groupsock.hh>
#include <H264VideoRTPSink.hh>
#include <H264VideoStreamDiscreteFramer.hh>
#include <H264VideoStreamFramer.hh>
#include <H265VideoRTPSink.hh>
#include <H265VideoStreamDiscreteFramer.hh>
#include <H265VideoStreamFramer.hh>
#include <MPEG4ESVideoRTPSink.hh>
#include <MPEG4VideoStreamDiscreteFramer.hh>
#include <MPEG4VideoStreamFramer.hh>
#include <OnDemandServerMediaSubsession.hh>
#include <StreamReplicator.hh>
#include <UsageEnvironment.hh>

#include "rtsp_server.hpp"

namespace cnstream {

class RtspMediaSubsession : public OnDemandServerMediaSubsession {
 public:
  static RtspMediaSubsession *createNew(UsageEnvironment &env, StreamReplicator *replicator,  // NOLINT
                                        RtspServer::CodecType codecType, Boolean discrete = True);

  void SetBitrate(uint64_t br) {
    if (br > 102400) {
      fBitRate = static_cast<uint64_t>(br / 1024);
    } else {
      fBitRate = 500;  // 500k
    }
  }

 protected:
  RtspMediaSubsession(UsageEnvironment &env, StreamReplicator *replicator,  // NOLINT
                      RtspServer::CodecType codecType, Boolean discrete = True)
      : OnDemandServerMediaSubsession(env, False),
        fReplicator(replicator),
        fCodecType(codecType),
        fDiscrete(discrete),
        fBitRate(1024) {}
  virtual FramedSource *createNewStreamSource(unsigned clientSessionId, unsigned &estBitrate);  // NOLINT
  virtual RTPSink *createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
                                    FramedSource *inputSource);
  StreamReplicator *fReplicator;
  uint64_t fCodecType;
  Boolean fDiscrete;
  uint64_t fBitRate;
};  // RtspMediaSubsession

}  // namespace cnstream

#endif  // ___RTSP_MEDIA_SUBSESSION_HPP__
