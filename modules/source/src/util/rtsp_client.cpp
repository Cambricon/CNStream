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

#include "rtsp_client.hpp"
#include <iostream>
#include <string>
#include <thread>

#define HAVE_LIVE555 1

#ifdef HAVE_LIVE555

#include "BasicUsageEnvironment.hh"
#include "liveMedia.hh"
#include "util/cnstream_timer.hpp"

// Forward function definitions:

// RTSP 'response handlers':
static void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
static void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
static void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);

// Other event handler functions:
static void subsessionAfterPlaying(
    void* clientData);  // called when a stream's subsession (e.g., audio or video substream) ends
static void subsessionByeHandler(void* clientData, char const* reason);
// called when a RTCP "BYE" is received for a subsession
static void streamTimerHandler(void* clientData);
// called at the end of a stream's expected duration (if the stream has not already signaled its end using a RTCP "BYE")

// Used to iterate through each stream's 'subsessions', setting up each one:
static void setupNextSubsession(RTSPClient* rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
static void shutdownStream(RTSPClient* rtspClient, int exitCode = 1);

// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
static UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
  return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
static UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
  return env << subsession.mediumName() << "/" << subsession.codecName();
}

// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:

class StreamClientState {
 public:
  StreamClientState();
  virtual ~StreamClientState();

 public:
  MediaSubsessionIterator* iter;
  MediaSession* session;
  MediaSubsession* subsession;
  TaskToken streamTimerTask;
  double duration;
};

// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a
// single "StreamClientState" structure, as a global variable in your application.  However, because - in this demo
// application - we're showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a
// separate "StreamClientState" structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a
// "StreamClientState" field to the subclass:

static cnstream::Timer s_rtspTimer;

class ourRTSPClient : public RTSPClient {
 public:
  static ourRTSPClient* createNew(UsageEnvironment& env, char const* rtspURL, int verbosityLevel = 0,
                                  char const* applicationName = NULL, portNumBits tunnelOverHTTPPortNum = 0);

 protected:
  ourRTSPClient(UsageEnvironment& env, char const* rtspURL, int verbosityLevel, char const* applicationName,
                portNumBits tunnelOverHTTPPortNum);
  // called only by createNew();
  virtual ~ourRTSPClient();

 public:
  bool streammingPreferTcp = true;
  bool streammingOverTcp = true;
  bool setupOk = false;
  char* eventLoopWatchVariable = nullptr;
  StreamClientState scs;
  bool only_key_frame = false;

  // Use a timer to check liveness
  //
  int livenessTimeoutMs = 2000;
  cnstream::timer_id timer_id_;
  void resetLivenessTimer() {
    s_rtspTimer.remove(timer_id_);
    timer_id_ = s_rtspTimer.add(std::chrono::milliseconds(livenessTimeoutMs), [&](cnstream::timer_id) {
      *eventLoopWatchVariable = 2;
      envir() << "Liveness timeout occurred, shutdown stream...\n";
    });
  }
  cnstream::IRtspCB* cb_ = nullptr;
};

// Define a data sink (a subclass of "MediaSink") to receive the data for each subsession (i.e., each audio or video
// 'substream'). In practice, this might be a class (or a chain of classes) that decodes and then renders the incoming
// audio or video. Or it might be a "FileSink", for outputting the received data into a file (as is done by the
// "openRTSP" application). In this example code, however, we define a simple 'dummy' sink that receives incoming data,
// but does nothing with it.

class DummySink : public MediaSink, public cnstream::IParserResult {
 public:
  static DummySink* createNew(UsageEnvironment& env,    // NOLINT
                              MediaSubsession& subsession,  // NOLINT identifies the kind of data that's being received
                              char const* streamId = NULL,
                              bool only_I = false);  // identifies the stream itself (optional)

 private:
  DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId, bool only_I);  // NOLINT
  // called only by "createNew()"
  virtual ~DummySink();

  static void afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
                                struct timeval presentationTime, unsigned durationInMicroseconds);
  void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime,
                         unsigned durationInMicroseconds);


  void OnParserInfo(cnstream::VideoInfo *info) override;
  void OnParserFrame(cnstream::VideoEsFrame *frame) override;

 private:
  // redefined virtual functions:
  Boolean continuePlaying() override;
  bool FindKeyFrame(unsigned char *buf, unsigned size, bool isH264);

 private:
  std::unique_ptr<u_int8_t[]> fReceiveBuffer = nullptr;
  MediaSubsession& fSubsession;
  char* fStreamId = nullptr;
  std::unique_ptr<u_int8_t[]> paramset = nullptr;
  unsigned paramset_size = 0;
  bool spropsSent = false;
  uint64_t frameTimeStampBase = 0;
  bool firstFrame = true;
  bool only_key_frame = false;
  cnstream::EsParser parser_;
};

// Implementation of the RTSP 'response handlers':

void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {
  do {
    UsageEnvironment& env = rtspClient->envir();                            // alias
    StreamClientState& scs = static_cast<ourRTSPClient*>(rtspClient)->scs;  // alias

    if (resultCode != 0) {
      env << *rtspClient << "Failed to get a SDP description: " << resultString << "\n";
      delete[] resultString;
      break;
    }

    char* const sdpDescription = resultString;
    env << *rtspClient << "Got a SDP description:\n" << sdpDescription << "\n";

    // Create a media session object from this SDP description:
    scs.session = MediaSession::createNew(env, sdpDescription);
    delete[] sdpDescription;  // because we don't need it anymore
    if (scs.session == NULL) {
      env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg()
          << "\n";
      break;
    } else if (!scs.session->hasSubsessions()) {
      env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
      break;
    }

    // Then, create and set up our data source objects for the session.  We do this by iterating over the session's
    // 'subsessions', calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one. (Each
    // 'subsession' will have its own data source.)
    scs.iter = new MediaSubsessionIterator(*scs.session);
    setupNextSubsession(rtspClient);
    return;
  } while (0);

  // An unrecoverable error occurred with this stream.
  shutdownStream(rtspClient);
}

void setupNextSubsession(RTSPClient* rtspClient) {
  UsageEnvironment& env = rtspClient->envir();                 // alias
  ourRTSPClient* client = (ourRTSPClient*)rtspClient;          // alias
  StreamClientState& scs = client->scs;                        // alias

  if (!client->setupOk) {
    if (scs.subsession == NULL) {
      // find the first video subsession, and only handle one video subsession
      MediaSubsession *subsession = scs.iter->next();
      while (subsession != NULL) {
        const char* mediumName = subsession->mediumName();
        if (strstr(mediumName, "video") != NULL) {
          scs.subsession = subsession;
          break;
        }
        subsession = scs.iter->next();
      }
      if (scs.subsession == NULL) {
        env << "Failed to find a video session\n";
        return;
      }
    }
    if (scs.subsession != NULL) {
      if (!scs.subsession->initiate()) {
        env << *rtspClient << "Failed to initiate the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg()
            << "\n";
        setupNextSubsession(rtspClient);  // give up on this subsession; go to the next one
      } else {
        env << *rtspClient << "Initiated the \"" << *scs.subsession << "\" subsession (";
        if (scs.subsession->rtcpIsMuxed()) {
          env << "client port " << scs.subsession->clientPortNum();
        } else {
          env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum() + 1;
        }
        env << ")\n";

        // Continue setting up this subsession, by sending a RTSP "SETUP" command:
        Boolean streamUsingTCP = (client->streammingPreferTcp && client->streammingOverTcp);
        rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False, streamUsingTCP, false);
      }
      return;
    }
  }

  // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
  if (scs.session->absStartTime() != NULL) {
    // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
    rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(), scs.session->absEndTime(),
                                1.0f);
  } else {
    scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
    rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, 0.0f, -1.0f, 1.0f);
  }
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {
  do {
    UsageEnvironment& env = rtspClient->envir();                 // alias
    ourRTSPClient* client = (ourRTSPClient*)rtspClient;          // alias
    StreamClientState& scs = client->scs;                        // alias

    if (resultCode != 0) {
      env << "Failed to set up the \"" << client->scs.subsession->mediumName()
          << "\" subsession: " << resultString << "\n";
      if (!client->setupOk) {
        if (client->streammingPreferTcp && client->streammingOverTcp) {
          env << "Failed to set up streaming over TCP, try UDP\n";
          client->streammingOverTcp = false;
          break;
        } else {
          env << "Failed to set up streaming over UDP\n";
          break;
        }
      }
    }
    if (!client->setupOk) client->setupOk = true;

    env << *rtspClient << "Set up the \"" << *scs.subsession << "\" subsession (";
    if (scs.subsession->rtcpIsMuxed()) {
      env << "client port " << scs.subsession->clientPortNum();
    } else {
      env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum() + 1;
    }
    env << ")\n";

    // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
    // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening
    // until later, after we've sent a RTSP "PLAY" command.)

    scs.subsession->sink = DummySink::createNew(env, *scs.subsession, rtspClient->url(), client->only_key_frame);
    // perhaps use your own custom "MediaSink" subclass instead
    if (scs.subsession->sink == NULL) {
      env << *rtspClient << "Failed to create a data sink for the \"" << *scs.subsession
          << "\" subsession: " << env.getResultMsg() << "\n";
      break;
    }

    env << *rtspClient << "Created a data sink for the \"" << *scs.subsession << "\" subsession\n";
    scs.subsession->miscPtr =
        rtspClient;  // a hack to let subsession handler functions get the "RTSPClient" from the subsession
    scs.subsession->sink->startPlaying(*(scs.subsession->readSource()), subsessionAfterPlaying, scs.subsession);
    // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
    if (scs.subsession->rtcpInstance() != NULL) {
      scs.subsession->rtcpInstance()->setByeWithReasonHandler(subsessionByeHandler, scs.subsession);
    }
  } while (0);
  delete[] resultString;

  // Set up the next subsession, if any:
  setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) {
  Boolean success = False;

  do {
    UsageEnvironment& env = rtspClient->envir();                 // alias
    StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs;  // alias

    if (resultCode != 0) {
      env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
      break;
    }

    // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its
    // end using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can
    // later 'seek' back within it and do another RTSP "PLAY" - then you can omit this code. (Alternatively, if you
    // don't want to receive the entire stream, you could set this timer for some shorter value.)
    if (scs.duration > 0) {
      unsigned const delaySlop =
          2;  // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
      scs.duration += delaySlop;
      unsigned uSecsToDelay = (unsigned)(scs.duration * 1000000);
      scs.streamTimerTask =
          env.taskScheduler().scheduleDelayedTask(uSecsToDelay, static_cast<TaskFunc*>(streamTimerHandler), rtspClient);
    } else {
      // start to check liveness for livestream
      static_cast<ourRTSPClient*>(rtspClient)->resetLivenessTimer();
    }

    env << *rtspClient << "Started playing session";
    if (scs.duration > 0) {
      env << " (for up to " << scs.duration << " seconds)";
    }
    env << "...\n";

    success = True;
  } while (0);
  delete[] resultString;

  if (!success) {
    // An unrecoverable error occurred with this stream.
    shutdownStream(rtspClient);
  }
}

// Implementation of the other event handlers:

void subsessionAfterPlaying(void* clientData) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

  // Begin by closing this subsession's stream:
  Medium::close(subsession->sink);
  subsession->sink = NULL;

  // Next, check whether *all* subsessions' streams have now been closed:
  MediaSession& session = subsession->parentSession();
  MediaSubsessionIterator iter(session);
  while ((subsession = iter.next()) != NULL) {
    if (subsession->sink != NULL) return;  // this subsession is still active
  }

  // All subsessions' streams have now been closed, so shutdown the client:
  shutdownStream(rtspClient);
}

void subsessionByeHandler(void* clientData, char const* reason) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
  UsageEnvironment& env = rtspClient->envir();  // alias

  env << *rtspClient << "Received RTCP \"BYE\"";
  if (reason != NULL) {
    env << " (reason:\"" << reason << "\")";
    delete[](char*) reason;
  }
  env << " on \"" << *subsession << "\" subsession\n";

  // Now act as if the subsession had closed:
  subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData) {
  ourRTSPClient* rtspClient = (ourRTSPClient*)clientData;
  StreamClientState& scs = rtspClient->scs;  // alias

  scs.streamTimerTask = NULL;

  // Shut down the stream:
  shutdownStream(rtspClient);
}

void shutdownStream(RTSPClient* rtspClient, int exitCode) {
  UsageEnvironment& env = rtspClient->envir();                 // alias
  StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs;  // alias

  // First, check whether any subsessions have still to be closed:
  if (scs.session != NULL) {
    Boolean someSubsessionsWereActive = False;
    MediaSubsessionIterator iter(*scs.session);
    MediaSubsession* subsession;

    while ((subsession = iter.next()) != NULL) {
      if (subsession->sink != NULL) {
        Medium::close(subsession->sink);
        subsession->sink = NULL;

        if (subsession->rtcpInstance() != NULL) {
          subsession->rtcpInstance()->setByeHandler(
              NULL, NULL);  // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
        }

        someSubsessionsWereActive = True;
      }
    }

    if (someSubsessionsWereActive) {
      // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
      // Don't bother handling the response to the "TEARDOWN".
      rtspClient->sendTeardownCommand(*scs.session, NULL);
    }
  }

  // leave the LIVE555 event loop
  (*((ourRTSPClient*)rtspClient)->eventLoopWatchVariable) = 1;

  env << *rtspClient << "Closing the stream.\n";
  Medium::close(rtspClient);
  // Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.
}

// Implementation of "ourRTSPClient":

ourRTSPClient* ourRTSPClient::createNew(UsageEnvironment& env, char const* rtspURL, int verbosityLevel,
                                        char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
  return new ourRTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

ourRTSPClient::ourRTSPClient(UsageEnvironment& env, char const* rtspURL, int verbosityLevel,
                             char const* applicationName, portNumBits tunnelOverHTTPPortNum)
    : RTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1) {
}

ourRTSPClient::~ourRTSPClient() {
  envir() << "ourRTSPClient::~ourRTSPClient() called\n";
  s_rtspTimer.remove(timer_id_);
}

// Implementation of "StreamClientState":

StreamClientState::StreamClientState()
    : iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {}

StreamClientState::~StreamClientState() {
  delete iter;
  if (session != NULL) {
    // We also need to delete "session", and unschedule "streamTimerTask" (if set)
    UsageEnvironment& env = session->envir();  // alias

    env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
    Medium::close(session);
  }
}

// Implementation of "DummySink":

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE (1024 * 1024)

DummySink* DummySink::createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId,
                                bool only_key_frame) {
  return new (std::nothrow) DummySink(env, subsession, streamId, only_key_frame);
}

DummySink::DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId, bool only_I)
    : MediaSink(env), fSubsession(subsession), only_key_frame(only_I) {
  fStreamId = strDup(streamId);
  fReceiveBuffer.reset(new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE + 4]);

  AVCodecID codec_id;
  if (!strcmp(fSubsession.codecName(), "H264")) {
    codec_id = AV_CODEC_ID_H264;
  } else if (!strcmp(fSubsession.codecName(), "H265")) {
    codec_id = AV_CODEC_ID_HEVC;
  } else {
    throw std::runtime_error("Unsupported codec type");  // FIXME
  }

  unsigned num = 1;
  unsigned records_num[3];
  SPropRecord* records[3] = {nullptr, nullptr, nullptr};
  records[0] = parseSPropParameterSets(fSubsession.fmtp_spropparametersets(), records_num[0]);
  if (records_num[0] == 0 || (records_num[0] == 1 && records[0]->sPropLength == 0)) {
    num = 3;
    if (records[0]) delete[] records[0];
    records[0] = parseSPropParameterSets(fSubsession.fmtp_spropvps(), records_num[0]);
    records[1] = parseSPropParameterSets(fSubsession.fmtp_spropsps(), records_num[1]);
    records[2] = parseSPropParameterSets(fSubsession.fmtp_sproppps(), records_num[2]);
  }

  paramset_size = 0;
  for (unsigned j = 0; j < num; j++) {
    SPropRecord* record = records[j];
    unsigned record_num = records_num[j];
    for (unsigned i = 0; i < record_num; i++) {
      if (record[i].sPropLength > 0) paramset_size += 4 + record[i].sPropLength;
    }
  }
  paramset.reset(new u_int8_t[paramset_size]);
  if (paramset) {
    unsigned char* tmp = paramset.get();
    for (unsigned j = 0; j < num; j++) {
      SPropRecord* record = records[j];
      unsigned record_num = records_num[j];
      for (unsigned i = 0; i < record_num; i++) {
        if (record[i].sPropLength <= 0) continue;
        tmp[0] = 0x00;
        tmp[1] = 0x00;
        tmp[2] = 0x00;
        tmp[3] = 0x01;
        memcpy(tmp + 4, record[i].sPropBytes, record[i].sPropLength);
        tmp += 4 + record[i].sPropLength;
      }
    }
  }
  for (unsigned j = 0; j < num; j++) {
    if (records[j]) delete[] records[j];
  }
  parser_.Open(codec_id, this, paramset.get(), paramset_size, only_key_frame);
}

DummySink::~DummySink() {
  delete[] fStreamId;
  parser_.Close();
}

void DummySink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
                                  struct timeval presentationTime, unsigned durationInMicroseconds) {
  DummySink* sink = (DummySink*)clientData;
  sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

// If you don't want to see debugging output for each received frame, then comment out the following line:
// #define DEBUG_PRINT_EACH_RECEIVED_FRAME 1

void DummySink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime,
                                  unsigned /*durationInMicroseconds*/) {
  // We've just received a frame of data.  (Optionally) print out information about it:
  #ifdef DEBUG_PRINT_EACH_RECEIVED_FRAME
    if (fStreamId != NULL) envir() << "Stream \"" << fStreamId << "\"; ";
    envir() << fSubsession.mediumName() << "/" << fSubsession.codecName() << ":\tReceived " << frameSize << " bytes";
    if (numTruncatedBytes > 0) envir() << " (with " << numTruncatedBytes << " bytes truncated)";
    char uSecsStr[6+1]; // used to output the 'microseconds' part of the presentation time
    sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);
    envir() << ".\tPresentation time: " << (int)presentationTime.tv_sec << "." << uSecsStr;
    if (fSubsession.rtpSource() != NULL && !fSubsession.rtpSource()->hasBeenSynchronizedUsingRTCP()) {
      envir() << "!"; // mark the debugging output to indicate that this presentation time is not RTCP-synchronized
    }
  #ifdef DEBUG_PRINT_NPT
    envir() << "\tNPT: " << fSubsession.getNormalPlayTime(presentationTime);
  #endif
    envir() << "\n";
  #endif

  ourRTSPClient* client = reinterpret_cast<ourRTSPClient*>(fSubsession.miscPtr);
  // start to check liveness for livestream, FIXME
  client->resetLivenessTimer();

  if (client->cb_ && frameSize) {
    /*H264/H265, video frame*/
    u_int8_t *buffer = fReceiveBuffer.get();
    buffer[0] = 0x00;
    buffer[1] = 0x00;
    buffer[2] = 0x00;
    buffer[3] = 0x01;

    uint64_t ts = (presentationTime.tv_usec/1000 + presentationTime.tv_sec * 1000) * 90;
    if (firstFrame) {
      frameTimeStampBase = ts;
      firstFrame = false;
    }
    cnstream::VideoEsPacket packet;
    packet.data = buffer;
    packet.len = frameSize + 4;
    packet.pts = ts - frameTimeStampBase;
    parser_.Parse(packet);
  }

  // Then continue, to request the next frame of data:
  continuePlaying();
}

void DummySink::OnParserInfo(cnstream::VideoInfo *info) {
  ourRTSPClient* client = reinterpret_cast<ourRTSPClient*>(fSubsession.miscPtr);
  if (client && client->cb_ && info) {
    client->cb_->OnRtspInfo(info);
  }
}
void DummySink::OnParserFrame(cnstream::VideoEsFrame *frame) {
  ourRTSPClient* client = reinterpret_cast<ourRTSPClient*>(fSubsession.miscPtr);
  if (client && client->cb_) {
    client->cb_->OnRtspFrame(frame);
  }
}

Boolean DummySink::continuePlaying() {
  if (fSource == NULL) {
    return False;  // sanity check (should not happen)
  }

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it
  // arrives:
  u_int8_t *buffer = fReceiveBuffer.get();
  fSource->getNextFrame(buffer + 4, DUMMY_SINK_RECEIVE_BUFFER_SIZE, afterGettingFrame, this, onSourceClosure,
                        this);
  return True;
}

#endif  // HAVE_LIVE555

namespace cnstream {

class RtspSessionImpl {
 public:
  RtspSessionImpl() {}
  ~RtspSessionImpl() { Close(); }

  int Open(const OpenParam& param) {
#ifdef HAVE_LIVE555
    param_ = param;
    exit_flag_ = 0;
    thread_id_ = std::thread(&RtspSessionImpl::TaskRoutine, this);
    return 0;
#else
    return -1;
#endif  // HAVE_LIVE555
  }
  void Close() {
#ifdef HAVE_LIVE555
    exit_flag_ = 1;
    if (thread_id_.joinable()) {
      this->eventLoopWatchVariable = 2;
      thread_id_.join();
    }
#endif  // HAVE_LIVE555
  }

 private:
  void TaskRoutine() {
    int reconnect = param_.reconnect;
    while (!exit_flag_) {
      TaskRoutine_();

      // FIXME, apply reconnect strategy
      if (reconnect < 0) {
        break;
      }
      --reconnect;
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    std::cout << "TaskRoutine exit" << std::endl;
    if(param_.cb) {
      param_.cb->OnRtspFrame(nullptr);
    }
  }

  void TaskRoutine_() {
#ifdef HAVE_LIVE555
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    if (!scheduler) {
      return;
    }
    UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
    if (!env) {
      return;
    }
    // Begin by creating a "RTSPClient" object.  Note that there is a separate "RTSPClient" object for each stream that
    // we wish to receive (even if more than stream uses the same "rtsp://" URL).
    RTSPClient* rtspClient =
        ourRTSPClient::createNew(*env, param_.url.c_str(), RTSP_CLIENT_VERBOSITY_LEVEL, "cnstream");
    if (rtspClient == NULL) {
      *env << "Failed to create a RTSP client for URL \"" << param_.url.c_str() << "\": " << env->getResultMsg()
           << "\n";
      return;
    }

    this->eventLoopWatchVariable = 0;
    reinterpret_cast<ourRTSPClient*>(rtspClient)->eventLoopWatchVariable = &this->eventLoopWatchVariable;
    reinterpret_cast<ourRTSPClient*>(rtspClient)->livenessTimeoutMs = param_.livenessTimeoutMs;
    reinterpret_cast<ourRTSPClient*>(rtspClient)->streammingPreferTcp = param_.streammingPreferTcp;
    reinterpret_cast<ourRTSPClient*>(rtspClient)->only_key_frame = param_.only_key_frame;
    reinterpret_cast<ourRTSPClient*>(rtspClient)->streammingOverTcp = true;
    reinterpret_cast<ourRTSPClient*>(rtspClient)->setupOk = false;
    reinterpret_cast<ourRTSPClient*>(rtspClient)->cb_ = param_.cb;

    // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
    // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a
    // response. Instead, the following function call returns immediately, and we handle the RTSP response later, from
    // within the event loop:
    rtspClient->sendDescribeCommand(continueAfterDESCRIBE);

    env->taskScheduler().doEventLoop(&this->eventLoopWatchVariable);

    //In case that client stops the session...
    if (this->eventLoopWatchVariable == 2) {
      shutdownStream(rtspClient);
    }

    if (env) {
      env->reclaim();
      env = NULL;
    }
    if (scheduler) {
      delete scheduler;
      scheduler = NULL;
    }
#endif  // HAVE_LIVE555
  }

 private:
  OpenParam param_;
  std::thread thread_id_;
  volatile char exit_flag_ = 0;
  // by default, print verbose output from each "RTSPClient"
  int RTSP_CLIENT_VERBOSITY_LEVEL = 1;
  char eventLoopWatchVariable = 0;
};

RtspSession::RtspSession() {}
RtspSession::~RtspSession() {
  if (impl_) {
    delete impl_, impl_ = nullptr;
  }
}

int RtspSession::Open(const OpenParam& param) {
  impl_ = new (std::nothrow) RtspSessionImpl;
  if (!impl_) {
    return -1;
  }
  return impl_->Open(param);
}

void RtspSession::Close() {
  if (impl_) {
    impl_->Close();
  }
}

}  // namespace cnstream
