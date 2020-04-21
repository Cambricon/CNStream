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

#include "rtsp_media_subsession.hpp"

namespace cnstream {

namespace RtspStreaming {

RtspMediaSubsession*
RtspMediaSubsession::createNew(UsageEnvironment& env, StreamReplicator* replicator) {  // NOLINT 
  return new RtspMediaSubsession(env, replicator);
}

FramedSource* RtspMediaSubsession::createNewStreamSource(unsigned int client_session_id,
                                                         unsigned int& est_bit_rate) {  // NOLINT
  est_bit_rate = kbit_rate_;
  FramedSource* source = m_replicator_->createStreamReplica();
  if (source == nullptr) return nullptr;
  return H264VideoStreamDiscreteFramer::createNew(envir(), source);
  // return H264VideoStreamFramer::createNew(envir(), source);
}

RTPSink* RtspMediaSubsession::createNewRTPSink(Groupsock* rtp_groupsock, unsigned char rtp_payload_type_if_dynamic,
                                               FramedSource* input_source) {
  return H264VideoRTPSink::createNew(envir(), rtp_groupsock, rtp_payload_type_if_dynamic);
}

}  // namespace RtspStreaming

}  // namespace cnstream
