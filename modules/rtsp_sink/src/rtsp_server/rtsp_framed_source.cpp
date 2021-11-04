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

#include "cnstream_logging.hpp"

#include "rtsp_framed_source.hpp"

namespace cnstream {

RtspFramedSource* RtspFramedSource::createNew(UsageEnvironment& env, RtspServer* server, Boolean discrete) {
  if (!server) return nullptr;
  return new RtspFramedSource(env, server, discrete);
}

RtspFramedSource::RtspFramedSource(UsageEnvironment& env, RtspServer* server, Boolean discrete)
    : FramedSource(env), fServer(server), fDiscrete(discrete) {
  fEventTriggerId = envir().taskScheduler().createEventTrigger(RtspFramedSource::deliverFrameStub);
}

RtspFramedSource::~RtspFramedSource() {
  if (fEventTriggerId) {
    envir().taskScheduler().deleteEventTrigger(fEventTriggerId);
  }
}

void RtspFramedSource::doStopGettingFrames() { FramedSource::doStopGettingFrames(); }

void RtspFramedSource::onEvent(RtspServer::Event event) {
  switch (event) {
    case RtspServer::Event::EVENT_DATA:
      // LOGI(RtspFramedSource) << "OnEvent() EVENT_DATA";
      envir().taskScheduler().triggerEvent(fEventTriggerId, this);
      break;
    case RtspServer::Event::EVENT_EOS:
      LOGI(RtspFramedSource) << "OnEvent() EVENT_EOS";
      break;
    default:
      LOGE(RtspFramedSource) << "OnEvent() Unknown event=" << event;
      return;
  }
}

Boolean RtspFramedSource::isKeyFrame(Boolean h264, const unsigned char* data, unsigned size) {
  const unsigned char* p = data;
  const unsigned char* end = p + size;
  const unsigned char* nalStart;

  auto findStartcode = [](const unsigned char* start, const unsigned char* end) -> const unsigned char* {
    if (start[0] == 0 && start[1] == 0) {
      if (start[2] == 1 && (start + 3) <= end) {
        return start + 3;
      }
      if (start[2] == 0 && start[3] == 1 && (start + 4) <= end) {
        return start + 4;
      }
    }
    return NULL;
  };

  do {
    nalStart = findStartcode(p, end);
    if (nalStart) {
      uint8_t nalType;
      if (h264) {
        nalType = *nalStart & 0x1f;
        if (nalType == 5) {
          return True;
        }
      } else {
        nalType = (*nalStart & 0x7e) >> 1;
        if (nalType >= 16 && nalType <= 21) {
          return True;
        }
      }
      p = nalStart;
    }
    while (p < end && *(p++) == 0) {
    }
  } while (p < end);

  return False;
}

void RtspFramedSource::doGetNextFrame() { deliverFrame(); }

void RtspFramedSource::deliverFrame() {
  int frameSize;
  int bufferPercent = 0;

  if (!isCurrentlyAwaitingData()) {
    /* we're not ready for the data yet, just wait buffer fill to 80% full and then drop the beginning data. */
    while (1) {
      fServer->param_.get_packet(nullptr, 0, nullptr, &bufferPercent);
      if (bufferPercent < 80) break;
      frameSize = fServer->param_.get_packet(nullptr, -1, nullptr, nullptr);
      // LOGI(RtspFramedSource) << "deliverFrame() dropped " << frameSize << " bytes, for buffer is >= 80% full.";
    }
    return;
  }

  frameSize = fServer->param_.get_packet(nullptr, 0, nullptr, nullptr);
  if (frameSize > 0) {
    /* This should never happen, but check anyway.. */
    if (static_cast<unsigned>(frameSize) > fMaxSize) {
      fNumTruncatedBytes = frameSize - fMaxSize;
      LOGW(RtspFramedSource) << "deliverFrame() Truncated, "
                                "frameSize("
                             << frameSize << ") > MaxSize(" << fMaxSize << ")";
    } else {
      fNumTruncatedBytes = 0;
    }

    double timestamp;
    fFrameSize = fServer->param_.get_packet(fTo, fMaxSize, &timestamp, nullptr);
    if (fFrameSize <= 0) {
      fFrameSize = 0;
      fNumTruncatedBytes = 0;
      fTo = nullptr;
      handleClosure(this);
      return;
    }

    if (fFirstFrame) {
      if (isKeyFrame(fServer->param_.codec_type == RtspServer::CodecType::H264, fTo, fFrameSize)) {
        LOGI(RtspFramedSource) << "deliverFrame() got IDR frame.";
        fFirstFrame = False;
      } else {
        // LOGI(RtspFramedSource) << "deliverFrame() skipped " << fFrameSize << " bytes before IDR frame.";
        fFrameSize = 0;
        return;
      }
    }
    // LOGI(RtspFramedSource) << "deliverFrame() got data, size=" << fFrameSize << ", pts=" << timestamp * 1000;
    if (fDiscrete) {
      int offset = 0;
      unsigned char* data = fTo;
      if (data[0] == 0 && data[1] == 0) {
        if (data[2] == 1) {
          offset = 3;
        } else if (data[2] == 0 && data[3] == 1) {
          offset = 4;
        }
      }
      if (offset > 0) memmove(fTo, fTo + offset, fFrameSize - offset);
      fFrameSize -= offset;
    }

    if (fInitPTS == -1) {
      gettimeofday(&fInitTimestamp, nullptr);
      fInitPTS = timestamp;
    }
    double pts = timestamp - fInitPTS;
    int64_t pts_secs = static_cast<int64_t>(pts);
    int64_t pts_usecs = (pts - pts_secs) * 1e6;
    fPresentationTime.tv_sec = fInitTimestamp.tv_sec + pts_secs;
    fPresentationTime.tv_usec = fInitTimestamp.tv_usec + pts_usecs;
    if (fPresentationTime.tv_usec >= 1e6) {
      fPresentationTime.tv_usec -= 1e6;
      fPresentationTime.tv_sec++;
    }
    // fDurationInMicroseconds = 33333;
  } else {
    fFrameSize = 0;
  }

  if (fFrameSize > 0) {
    FramedSource::afterGetting(this);
  }
}

}  // namespace cnstream
