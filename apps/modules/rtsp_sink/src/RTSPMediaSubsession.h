#ifndef __RTSP_MEDIA_SUBSESSION_H_
#define __RTSP_MEDIA_SUBSESSION_H_

#include <Groupsock.hh>
#include <H264VideoRTPSink.hh>
#include <H264VideoStreamDiscreteFramer.hh>
#include <H264VideoStreamFramer.hh>
#include <OnDemandServerMediaSubsession.hh>
#include <StreamReplicator.hh>
#include <UsageEnvironment.hh>

namespace RTSPStreaming {
class RTSPMediaSubsession : public OnDemandServerMediaSubsession {
 public:
  static RTSPMediaSubsession* createNew(UsageEnvironment& env, StreamReplicator* replicator);

  void SetBitrate(uint64_t br) {
    if (br > 500 * 1000) {
      fBitRate_ = static_cast<unsigned long>(br / 1024);
    } else {
      fBitRate_ = 500;  // 500k
    }
  };

 protected:
  RTSPMediaSubsession(UsageEnvironment& env, StreamReplicator* replicator)
      : OnDemandServerMediaSubsession(env, False), m_replicator_(replicator), fBitRate_(1024){};
  virtual FramedSource* createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate);
  virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
                                    FramedSource* inputSource);
  StreamReplicator* m_replicator_;
  unsigned long fBitRate_;
};
}  // namespace RTSPStreaming

#endif