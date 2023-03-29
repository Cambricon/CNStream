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

#include <string>

#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "UsageEnvironment.hh"
#include "liveMedia.hh"

#include "rtsp_framed_source.hpp"
#include "rtsp_media_subsession.hpp"
#include "rtsp_server.hpp"

#include "cnstream_logging.hpp"

namespace cnstream {

RtspServer::RtspServer(const Param &param) : param_(param) {}

RtspServer::~RtspServer() { Stop(); }

bool RtspServer::Start() {
  if (quit_ == 0) return true;

  // Check parameters
  if (!param_.get_packet) {
    LOGE(RtspServer) << "Start() get_packet is nullptr";
    return false;
  }
  if (param_.codec_type != H264 && param_.codec_type != H265) {
    LOGE(RtspServer) << "Start() Only support codec type: H264 & H265";
    return false;
  }

  UserAuthenticationDatabase *authDB = nullptr;
  if (param_.authentication) {
    authDB = new UserAuthenticationDatabase;
    authDB->addUserRecord(param_.user_name.c_str(), param_.password.c_str());
  }
  scheduler_ = BasicTaskScheduler::createNew();
  env_ = BasicUsageEnvironment::createNew(*scheduler_);
  rtsp_server_ = RTSPServer::createNew(*env_, param_.port, authDB);
  if (!rtsp_server_) {
    LOGE(RtspServer) << "Failed to create RTSPServer: " << env_->getResultMsg();
    return false;
  }
  quit_ = 0;
  thread_ = std::thread(&RtspServer::Loop, this);

  return true;
}

bool RtspServer::Stop() {
  if (quit_ == 1) return true;

  quit_ = 1;
  if (thread_.joinable()) thread_.join();

  if (scheduler_) delete scheduler_;

  return true;
}

void RtspServer::OnEvent(Event event) {
  if (source_) source_->onEvent(event);
}

void RtspServer::Loop() {
  char streamName[1024] = {0};
  snprintf(streamName, sizeof(streamName), "%s", "live");

  OutPacketBuffer::increaseMaxSizeTo(param_.bit_rate);

  if (!rtsp_server_) {
    LOGE(RtspServer) << "Failed to create RTSPServer";
    return;
  } else {
    if (param_.rtsp_over_http) rtsp_server_->setUpTunnelingOverHTTP(param_.port);
    source_ = RtspFramedSource::createNew(*env_, this, !param_.stream_mode);
    StreamReplicator *replicator = StreamReplicator::createNew(*env_, source_, false);
    char const *descriptionString = "RTSP Live Streaming Session";
    ServerMediaSession *sms = ServerMediaSession::createNew(*env_, streamName, streamName, descriptionString);
    RtspMediaSubsession *sub =
        RtspMediaSubsession::createNew(*env_, replicator, param_.codec_type, !param_.stream_mode);
    sub->SetBitrate(param_.bit_rate);
    sms->addSubsession(sub);
    rtsp_server_->addServerMediaSession(sms);

    char *url = rtsp_server_->rtspURL(sms);
    LOGI(RtspServer) << "\033[36m Stream URL \"" << url << "\"\033[0m";
    delete[] url;

    // signal(SIGNIT,sighandler);
    env_->taskScheduler().doEventLoop(&quit_);  // does not return

    Medium::close(rtsp_server_);
    Medium::close(replicator);

    LOGI(RtspServer) << "Loop() Exit";
  }

  env_->reclaim();
}

}  // namespace cnstream
