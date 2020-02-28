#include "LiveRTSPServer.h"
#include "RTSPFrameSource.h"
#include "RTSPMediaSubsession.h"

namespace RTSPStreaming {
LiveRTSPServer::LiveRTSPServer(VideoEncoder* encoder, int port, int httpPort)
    : fVideoEncoder_(encoder), fPortNumber_(port), fHttpTunnelingPort_(httpPort) {
  fQuit_ = 0;
  fBitrate_ = 0;
  fEnablePassword_ = false;
}

LiveRTSPServer::~LiveRTSPServer() {}

void LiveRTSPServer::SetAccessControl(bool isOnOff) { fEnablePassword_ = isOnOff; }

void LiveRTSPServer::Run() {
  TaskScheduler* scheduler;
  UsageEnvironment* env;

  const char* UserN = "admin";
  const char* PassW = "hello123";

  char RTSP_Address[1024];
  RTSP_Address[0] = 0x00;
  strcpy(RTSP_Address, "rtsp_live");
  scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);
  UserAuthenticationDatabase* authDB = nullptr;
  if (fEnablePassword_) {
    authDB = new UserAuthenticationDatabase;
    authDB->addUserRecord(UserN, PassW);
  }
  OutPacketBuffer::increaseMaxSizeTo(1920 * 1080 * 3 / 2);  // 2M
  RTSPServer* rtspServer = RTSPServer::createNew(*env, fPortNumber_, authDB);
  if (rtspServer == nullptr) {
    *env << "LIVE555: Failed to create RTSP server:" << env->getResultMsg() << "\n";
    exit(EXIT_FAILURE);
  } else {
    if (fHttpTunnelingPort_) {
      rtspServer->setUpTunnelingOverHTTP(fHttpTunnelingPort_);
    }

    char const* descriptionString = "Live Streaming Session";
    RTSPFrameSource* source = RTSPFrameSource::createNew(*env, fVideoEncoder_);
    StreamReplicator* inputDevice = StreamReplicator::createNew(*env, source, false);

    ServerMediaSession* sms = ServerMediaSession::createNew(*env, RTSP_Address, RTSP_Address, descriptionString);
    RTSPMediaSubsession* sub = RTSPMediaSubsession::createNew(*env, inputDevice);
    if (fBitrate_ > 0) {
      sub->SetBitrate(fBitrate_);
    } else {
      sub->SetBitrate(fVideoEncoder_->GetBitrate());
    }
    sms->addSubsession(sub);
    rtspServer->addServerMediaSession(sms);

    char* url = rtspServer->rtspURL(sms);
    *env << "Play this stream using the URL \"" << url << "\"\n";
    delete[] url;

    // signal(SIGNIT,sighandler);
    env->taskScheduler().doEventLoop(&fQuit_);  // does not return

    Medium::close(rtspServer);
    Medium::close(inputDevice);
  }
  env->reclaim();
  delete scheduler;
}
}  // namespace RTSPStreaming
