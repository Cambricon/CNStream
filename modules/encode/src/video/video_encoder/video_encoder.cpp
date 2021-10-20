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

#include "video_encoder.hpp"
#include "video_encoder_base.hpp"
#include "video_encoder_mlu.hpp"
#include "video_encoder_ffmpeg.hpp"

namespace cnstream {

VideoEncoder::VideoEncoder(const Param &param) {
  if (param.mlu_device_id >= 0) {
    encoder_ = new (std::nothrow) cnstream::video::VideoEncoderMlu(param);
  } else {
    encoder_ = new (std::nothrow) cnstream::video::VideoEncoderFFmpeg(param);
  }
}

VideoEncoder::~VideoEncoder() {
  if (encoder_) {
    delete encoder_;
    encoder_ = nullptr;
  }
}

VideoEncoder::VideoEncoder(VideoEncoder &&encoder) {
  encoder_ = encoder.encoder_;
  encoder.encoder_ = nullptr;
}

VideoEncoder & VideoEncoder::operator=(VideoEncoder &&encoder) {
  if (encoder_) {
    delete encoder_;
  }
  encoder_ = encoder.encoder_;
  encoder.encoder_ = nullptr;
  return *this;
}

int VideoEncoder::Start() {
  if (encoder_) return encoder_->Start();
  return ERROR_FAILED;
}

int VideoEncoder::Stop() {
  if (encoder_) return encoder_->Stop();
  return ERROR_FAILED;
}

int VideoEncoder::RequestFrameBuffer(VideoFrame *frame, int timeout_ms) {
  if (encoder_) return encoder_->RequestFrameBuffer(frame, timeout_ms);
  return ERROR_FAILED;
}

int VideoEncoder::SendFrame(const VideoFrame *frame, int timeout_ms) {
  if (encoder_) return encoder_->SendFrame(frame, timeout_ms);
  return ERROR_FAILED;
}

int VideoEncoder::GetPacket(VideoPacket *packet, PacketInfo *info) {
  if (encoder_) return encoder_->GetPacket(packet, info);
  return ERROR_FAILED;
}

void VideoEncoder::SetEventCallback(EventCallback func) {
  if (encoder_) encoder_->SetEventCallback(func);
}

}  // namespace cnstream
