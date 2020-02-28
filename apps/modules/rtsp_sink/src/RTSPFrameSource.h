#ifndef __RTSP_FRAME_SOURCE_H_
#define __RTSP_FRAME_SOURCE_H_

#include <FramedSource.hh>
#include <Groupsock.hh>
#include <UsageEnvironment.hh>
#include <functional>
#include "VideoEncoder.h"

namespace RTSPStreaming {
class RTSPFrameSource : public FramedSource {
 public:
  static RTSPFrameSource* createNew(UsageEnvironment& env, VideoEncoder* encoder);
  RTSPFrameSource(UsageEnvironment& env, VideoEncoder* encoder);
  virtual ~RTSPFrameSource();

 private:
  static void deliverFrameStub(void* clientData) { ((RTSPFrameSource*)clientData)->deliverFrame(); }
  virtual void doGetNextFrame();
  void deliverFrame();
  virtual void doStopGettingFrames();
  void onEncoderEvent(VideoEncoder::Event event);

  VideoEncoder* fEncoder_;
  EventTriggerId fEventTriggerId_ = 0;
  struct timeval fInitTimestamp_;
};  // RTSPFrameSource

}  // namespace RTSPStreaming

#endif  // RTSPFrameSource.h
