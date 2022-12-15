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

#include "rtsp_media_subsession.hpp"

namespace cnstream {

RtspMediaSubsession *RtspMediaSubsession::createNew(UsageEnvironment &env, StreamReplicator *replicator,
                                                    RtspServer::CodecType codecType, Boolean discrete) {
  return new RtspMediaSubsession(env, replicator, codecType, discrete);
}

FramedSource *RtspMediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned &estBitrate) {
  estBitrate = fBitRate;
  FramedSource *source = fReplicator->createStreamReplica();
  switch (fCodecType) {
    case RtspServer::CodecType::H264:
      if (fDiscrete)
        return H264VideoStreamDiscreteFramer::createNew(envir(), source, True);
      else
        return H264VideoStreamFramer::createNew(envir(), source, False);
    case RtspServer::CodecType::H265:
      if (fDiscrete)
        return H265VideoStreamDiscreteFramer::createNew(envir(), source, True);
      else
        return H265VideoStreamFramer::createNew(envir(), source, False);
    case RtspServer::CodecType::MPEG4:
      if (fDiscrete)
        return MPEG4VideoStreamDiscreteFramer::createNew(envir(), source);
      else
        return MPEG4VideoStreamFramer::createNew(envir(), source);
    default:
      return nullptr;
  }
  return nullptr;
}

RTPSink *RtspMediaSubsession::createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
                                               FramedSource *inputSource) {
  switch (fCodecType) {
    case RtspServer::CodecType::H264:
      return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    case RtspServer::CodecType::H265:
      return H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    case RtspServer::CodecType::MPEG4:
      return MPEG4ESVideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    default:
      return nullptr;
  }
  return nullptr;
}

}  // namespace cnstream
