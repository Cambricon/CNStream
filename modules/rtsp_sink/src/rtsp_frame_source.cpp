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

#include "rtsp_frame_source.hpp"

#include <glog/logging.h>

namespace cnstream {

namespace RtspStreaming {

RtspFrameSource* RtspFrameSource::createNew(UsageEnvironment& env, VideoEncoder* encoder) {
  return new RtspFrameSource(env, encoder);
}

RtspFrameSource::RtspFrameSource(UsageEnvironment& env, VideoEncoder* encoder) : FramedSource(env), encoder_(encoder) {
  event_trigger_id_ = envir().taskScheduler().createEventTrigger(RtspFrameSource::DeliverFrameStub);
  auto callback = std::bind(&RtspFrameSource::OnEncoderEvent, this, std::placeholders::_1);
  encoder_->SetCallback(callback);
  init_timestamp_.tv_sec = 0;
  init_timestamp_.tv_usec = 0;
}

RtspFrameSource::~RtspFrameSource() {
  if (event_trigger_id_) envir().taskScheduler().deleteEventTrigger(event_trigger_id_);
  encoder_->SetCallback(nullptr);
}

void RtspFrameSource::doStopGettingFrames() { FramedSource::doStopGettingFrames(); }

void RtspFrameSource::OnEncoderEvent(VideoEncoder::Event event) {
  if (event == VideoEncoder::Event::NEW_FRAME) envir().taskScheduler().triggerEvent(event_trigger_id_, this);
}

void RtspFrameSource::doGetNextFrame() { DeliverFrame(); }

void RtspFrameSource::DeliverFrame() {
  if (!isCurrentlyAwaitingData()) return;  // we're not ready for the data yet

  uint32_t frame_size;
  int64_t frame_pts;

  /* get data frame from the Encoding thread.. */
  if (encoder_->GetFrame(fTo, fMaxSize, &frame_size, &frame_pts)) {
    // std::cout << "$$$ frame_size: " << frame_size << " -- fMaxSize: " << fMaxSize << std::endl;
    if (frame_size > 0) {
      /* This should never happen, but check anyway.. */
      if (frame_size > fMaxSize) {
        fFrameSize = fMaxSize;
        fNumTruncatedBytes = frame_size - fMaxSize;
        LOG(INFO) << "Truncated, frame_size(" << frame_size << ") > fMaxSize(" << fMaxSize << ")";
      } else {
        fFrameSize = frame_size;
        fNumTruncatedBytes = 0;
      }

      if (init_timestamp_.tv_sec == 0 && init_timestamp_.tv_usec == 0) {
        gettimeofday(&init_timestamp_, nullptr);
      }
      if (frame_pts > 0) {
        fPresentationTime.tv_sec = init_timestamp_.tv_sec + frame_pts / 1000;
        fPresentationTime.tv_usec = init_timestamp_.tv_usec + (frame_pts % 1000) * 1000;
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
  if (fFrameSize > 0) {
    FramedSource::afterGetting(this);
  }
}

}  // namespace RtspStreaming

}  // namespace cnstream
