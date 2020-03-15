#ifndef __LIVE_RTSP_SERVER_H_
#define __LIVE_RTSP_SERVER_H_
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <UsageEnvironment.hh>
#include <liveMedia.hh>
#include "VideoEncoder.h"

namespace RTSPStreaming {
class LiveRTSPServer {
 public:
  LiveRTSPServer(VideoEncoder *encoder, int port, int httpPort);
  ~LiveRTSPServer();
  void Run();
  void SignalExit() { fQuit_ = 1; }

  void SetBitrate(uint64_t br) {
    fBitrate_ = static_cast<unsigned int>(br / 1000);  // in kbs
  }
  void SetAccessControl(bool isOnOff);

 private:
  VideoEncoder *fVideoEncoder_;
  int fPortNumber_;
  int fHttpTunnelingPort_;
  char fQuit_;
  bool fEnablePassword_;
  unsigned int fBitrate_;  // in kbs
};                         // LiveRTSPServer
}  // namespace RTSPStreaming
#endif
