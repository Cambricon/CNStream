#include "RTSPMediaSubsession.h"

namespace RTSPStreaming {
RTSPMediaSubsession*
RTSPMediaSubsession::createNew(UsageEnvironment& env, StreamReplicator* replicator) {
  return new RTSPMediaSubsession(env, replicator);
}

FramedSource* RTSPMediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate) {
  estBitrate = fBitRate_;
  FramedSource* source = m_replicator_->createStreamReplica();
  if (source == nullptr) return nullptr;
  return H264VideoStreamDiscreteFramer::createNew(envir(), source);
  // return H264VideoStreamFramer::createNew(envir(), source);
}

RTPSink* RTSPMediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
                                               FramedSource* inputSource) {
  return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}
}  // namespace RTSPStreaming
