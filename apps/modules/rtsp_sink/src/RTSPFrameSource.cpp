#include "RTSPFrameSource.h"
#include <iostream>

namespace RTSPStreaming {
RTSPFrameSource* RTSPFrameSource::createNew(UsageEnvironment& env, VideoEncoder* encoder) {
  return new RTSPFrameSource(env, encoder);
}

RTSPFrameSource::RTSPFrameSource(UsageEnvironment& env, VideoEncoder* encoder) : FramedSource(env), fEncoder_(encoder) {
  fEventTriggerId_ = envir().taskScheduler().createEventTrigger(RTSPFrameSource::deliverFrameStub);
  auto callback = std::bind(&RTSPFrameSource::onEncoderEvent, this, std::placeholders::_1);
  fEncoder_->SetCallback(callback);
  fInitTimestamp_.tv_sec = 0;
  fInitTimestamp_.tv_usec = 0;
}

RTSPFrameSource::~RTSPFrameSource() {
  if (fEventTriggerId_) envir().taskScheduler().deleteEventTrigger(fEventTriggerId_);
  fEncoder_->SetCallback(nullptr);
}

void RTSPFrameSource::doStopGettingFrames() { FramedSource::doStopGettingFrames(); }

void RTSPFrameSource::onEncoderEvent(VideoEncoder::Event event) {
  if (event == VideoEncoder::Event::NEW_FRAME) envir().taskScheduler().triggerEvent(fEventTriggerId_, this);
}

void RTSPFrameSource::doGetNextFrame() { deliverFrame(); }

void RTSPFrameSource::deliverFrame() {
  if (!isCurrentlyAwaitingData()) return;  // we're not ready for the data yet

  uint32_t frameSize;
  int64_t FramePTS;

  /* get data frame from the Encoding thread.. */
  if (fEncoder_->GetFrame(fTo, fMaxSize, &frameSize, &FramePTS)) {
    std::cout << "$$$ frameSize: " << frameSize << " -- fMaxSize: " << fMaxSize << std::endl;
    if (frameSize > 0) {
      /* This should never happen, but check anyway.. */
      if (frameSize > fMaxSize) {
        fFrameSize = fMaxSize;
        fNumTruncatedBytes = frameSize - fMaxSize;
        std::cout << "Truncated, frameSize(" << frameSize << ") > fMaxSize(" << fMaxSize << ")" << std::endl;
      } else {
        fFrameSize = frameSize;
        fNumTruncatedBytes = 0;
      }

      if (fInitTimestamp_.tv_sec == 0 && fInitTimestamp_.tv_usec == 0) {
        gettimeofday(&fInitTimestamp_, nullptr);
      }
      if (FramePTS > 0) {
        fPresentationTime.tv_sec = fInitTimestamp_.tv_sec + FramePTS / 1000;
        fPresentationTime.tv_usec = fInitTimestamp_.tv_usec + (FramePTS % 1000) * 1000;
        if (fPresentationTime.tv_usec >= 1e6) {
          fPresentationTime.tv_usec -= 1e6;
          fPresentationTime.tv_sec++;
        }
      } else {
        gettimeofday(&fPresentationTime, nullptr);
      }
    } else {
      fFrameSize = 0;
      fTo = nullptr;
      handleClosure(this);
      return;
    }
  } else {
    fFrameSize = 0;
  }

#define USE_MLU
// #define USE_FFMPEG

#ifdef USE_MLU
  nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
               (TaskFunc*)FramedSource::afterGetting, this);
#endif
#ifdef USE_FFMPEG
  if (fFrameSize > 0) {
    FramedSource::afterGetting(this);
  }
#endif
}

}  // namespace RTSPStreaming
