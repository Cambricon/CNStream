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

  quit_ = 0;
  thread_ = std::thread(&RtspServer::Loop, this);

  return true;
}

bool RtspServer::Stop() {
  if (quit_ == 1) return true;

  quit_ = 1;
  if (thread_.joinable()) thread_.join();

  return true;
}

void RtspServer::OnEvent(Event event) {
  if (source_) source_->onEvent(event);
}

void RtspServer::Loop() {
  TaskScheduler *scheduler;
  UsageEnvironment *env;

  char streamName[1024] = {0};
  snprintf(streamName, sizeof(streamName), "%s", "live");

  scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);
  UserAuthenticationDatabase *authDB = nullptr;
  if (param_.authentication) {
    authDB = new UserAuthenticationDatabase;
    authDB->addUserRecord(param_.user_name.c_str(), param_.password.c_str());
  }

  OutPacketBuffer::increaseMaxSizeTo(param_.bit_rate);

  RTSPServer *rtspServer = RTSPServer::createNew(*env, param_.port, authDB);
  if (!rtspServer) {
    LOGE(RtspServer) << "Failed to create RTSPServer: " << env->getResultMsg();
    return;
  } else {
    if (param_.rtsp_over_http) rtspServer->setUpTunnelingOverHTTP(param_.port);
    source_ = RtspFramedSource::createNew(*env, this, !param_.stream_mode);
    StreamReplicator *replicator = StreamReplicator::createNew(*env, source_, false);
    char const *descriptionString = "RTSP Live Streaming Session";
    ServerMediaSession *sms = ServerMediaSession::createNew(*env, streamName, streamName, descriptionString);
    RtspMediaSubsession *sub = RtspMediaSubsession::createNew(*env, replicator, param_.codec_type, !param_.stream_mode);
    sub->SetBitrate(param_.bit_rate);
    sms->addSubsession(sub);
    rtspServer->addServerMediaSession(sms);

    char *url = rtspServer->rtspURL(sms);
    LOGI(RtspServer) << "\033[36m Stream URL \"" << url << "\"\033[0m";
    delete[] url;

    // signal(SIGNIT,sighandler);
    env->taskScheduler().doEventLoop(&quit_);  // does not return

    Medium::close(rtspServer);
    Medium::close(replicator);

    LOGI(RtspServer) << "Loop() Exit";
  }

  env->reclaim();
  delete scheduler;
}

}  // namespace cnstream
