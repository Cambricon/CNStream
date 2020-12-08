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

#include "live_rtsp_server.hpp"

#include "cnstream_logging.hpp"

#include "rtsp_frame_source.hpp"
#include "rtsp_media_subsession.hpp"

namespace cnstream {

namespace RtspStreaming {

LiveRtspServer::LiveRtspServer(VideoEncoder* encoder, int port, int http_port)
    : video_encoder_(encoder), port_number_(port), http_tunneling_port_(http_port) {
  is_quit_ = 0;
  kbit_rate_ = 0;
  enable_password_ = false;
}

LiveRtspServer::~LiveRtspServer() {}

void LiveRtspServer::SetAccessControl(bool enable) { enable_password_ = enable; }

void LiveRtspServer::Run() {
  TaskScheduler* scheduler;
  UsageEnvironment* env;

  const char* user_name = "admin";
  const char* password = "hello123";

  char rtsp_address[1024];
  rtsp_address[0] = 0x00;
  strcpy(rtsp_address, "rtsp_live");  // NOLINT
  scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);
  UserAuthenticationDatabase* auth_db = nullptr;
  if (enable_password_) {
    auth_db = new UserAuthenticationDatabase;
    auth_db->addUserRecord(user_name, password);
  }
  OutPacketBuffer::increaseMaxSizeTo(1920 * 1080 * 3 / 2);
  RTSPServer* rtsp_server = RTSPServer::createNew(*env, port_number_, auth_db);
  if (rtsp_server == nullptr) {
    *env << "LIVE555: Failed to create RTSP server:" << env->getResultMsg() << "\n";
    exit(EXIT_FAILURE);
  } else {
    if (http_tunneling_port_) {
      rtsp_server->setUpTunnelingOverHTTP(http_tunneling_port_);
    }

    char const* description_str = "Live Streaming Session";
    RtspFrameSource* source = RtspFrameSource::createNew(*env, video_encoder_);
    StreamReplicator* input_device = StreamReplicator::createNew(*env, source, false);

    ServerMediaSession* sms = ServerMediaSession::createNew(*env, rtsp_address, rtsp_address, description_str);
    RtspMediaSubsession* sub = RtspMediaSubsession::createNew(*env, input_device);
    if (kbit_rate_ > 0) {
      sub->SetBitRate(kbit_rate_);
    } else {
      sub->SetBitRate(video_encoder_->GetBitRate());
    }
    sms->addSubsession(sub);
    rtsp_server->addServerMediaSession(sms);
    char* url = rtsp_server->rtspURL(sms);

    std::ofstream out_file("RTSP_url_names.txt", std::ios::app);
    if (!out_file.is_open()) {
      LOGE(RTSP) << "Open RTSP_url_names.txt failure..." << std::endl;
    } else {
      out_file << url << std::endl;
      out_file.close();
    }
    LOGI(RTSP) << "================================================================";
    LOGI(RTSP) << " Stream URL \"" << url << "\"\n";
    LOGI(RTSP) << "================================================================";
    delete[] url;

    // signal(SIGNIT,sighandler);
    env->taskScheduler().doEventLoop(&is_quit_);  // does not return

    Medium::close(rtsp_server);
    Medium::close(input_device);
  }
  env->reclaim();
  delete scheduler;
}

}  // namespace RtspStreaming

}  // namespace cnstream
