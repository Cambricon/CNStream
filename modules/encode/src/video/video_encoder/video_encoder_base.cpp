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

#include "video_encoder_base.hpp"

#include "cnstream_logging.hpp"

namespace cnstream {

namespace video {

VideoEncoderBase::VideoEncoderBase(const Param &param) : param_(param) {
  if (param_.output_buffer_size < 0x80000) {
    LOGW(VideoEncoderBase) << "VideoEncoderBase() output buffer size must no fewer than 512K bytes";
    param_.output_buffer_size = 0x80000;
  }
  output_buffer_ = new (std::nothrow) CircularBuffer(param_.output_buffer_size);
  memset(&truncated_packet_, 0, sizeof(VideoPacket));
  truncated_buffer_size_ = 0;
  truncated_size_ = 0;
  memset(&truncated_info_, 0, sizeof(PacketInfo));
}

VideoEncoderBase::~VideoEncoderBase() {
  if (output_buffer_) {
    delete output_buffer_;
    output_buffer_ = nullptr;
  }
  if (truncated_packet_.data) {
    delete[] truncated_packet_.data;
  }
  memset(&truncated_packet_, 0, sizeof(VideoPacket));
  truncated_buffer_size_ = 0;
  truncated_size_ = 0;
  memset(&truncated_info_, 0, sizeof(PacketInfo));
}

int VideoEncoderBase::Start() {
  WriteLockGuard lk(state_mtx_);
  if (state_ == IDLE) state_ = RUNNING;
  return cnstream::VideoEncoder::ERROR_STATE;
}

int VideoEncoderBase::Stop() {
  WriteLockGuard lk(state_mtx_);
  if (state_ == RUNNING) state_ = IDLE;
  return cnstream::VideoEncoder::ERROR_STATE;
}

bool VideoEncoderBase::PushBuffer(IndexedVideoPacket *packet) {
  if (state_ != RUNNING || !output_buffer_) return false;

  if (!packet || !packet->packet.data || packet->packet.size <= 0) {
    LOGE(VideoEncoderBase) << "PushBuffer() invalid parameters.";
    return false;
  }

  std::unique_lock<std::mutex> lk(output_mtx_);
  size_t push_size = packet->packet.size + sizeof(IndexedVideoPacket);
  output_cv_.wait(
      lk, [&] { return (state_ != RUNNING || (output_buffer_->Capacity() - output_buffer_->Size()) >= push_size); });
  if (state_ != RUNNING) return false;

  size_t written_size = output_buffer_->Write(reinterpret_cast<uint8_t *>(packet), sizeof(IndexedVideoPacket));
  written_size += output_buffer_->Write(packet->packet.data, packet->packet.size);

  return (written_size == push_size);
}

int VideoEncoderBase::GetPacket(VideoPacket *packet, PacketInfo *info) {
  if (state_ != RUNNING) return cnstream::VideoEncoder::ERROR_STATE;
  if (!output_buffer_) return cnstream::VideoEncoder::ERROR_FAILED;

  int ret = -1;
  IndexedVideoPacket vpacket;

  std::unique_lock<std::mutex> lk(output_mtx_);
  if (truncated_size_ == 0) {
    if (output_buffer_->Size() <= sizeof(IndexedVideoPacket)) return 0;
    output_buffer_->Read(reinterpret_cast<uint8_t *>(&vpacket), sizeof(IndexedVideoPacket), true);
  }
  if (!packet) {
    /* skip packet */
    if (truncated_size_ > 0) {
      ret = truncated_size_;
      truncated_size_ = 0;
      memset(&truncated_info_, 0, sizeof(PacketInfo));
      if (info) {
        memset(info, 0, sizeof(PacketInfo));
        info->buffer_size = output_buffer_->Size();
        info->buffer_capacity = output_buffer_->Capacity();
      }
    } else {
      output_buffer_->Read(nullptr, sizeof(IndexedVideoPacket) + vpacket.packet.size);
      ret = vpacket.packet.size;
      if (info) {
        GetPacketInfo(vpacket.index, info);
        info->buffer_size = output_buffer_->Size();
        info->buffer_capacity = output_buffer_->Capacity();
      } else {
        PacketInfo pi;
        GetPacketInfo(vpacket.index, &pi);
      }
      lk.unlock();
      output_cv_.notify_one();
    }
  } else if (!packet->data) {
    /* get packet size */
    if (info) {
      memset(info, 0, sizeof(PacketInfo));
      info->buffer_size = output_buffer_->Size();
      info->buffer_capacity = output_buffer_->Capacity();
    }
    if (truncated_size_ > 0) {
      packet->size = truncated_size_;
      packet->pts = truncated_packet_.pts;
      packet->dts = truncated_packet_.dts;
      packet->flags = truncated_packet_.flags;
      packet->user_data = truncated_packet_.user_data;
      return truncated_size_;
    } else {
      packet->size = vpacket.packet.size;
      packet->pts = vpacket.packet.pts;
      packet->dts = vpacket.packet.dts;
      packet->flags = vpacket.packet.flags;
      packet->user_data = vpacket.packet.user_data;
      return vpacket.packet.size;
    }
  } else {
    /* read out packet data */
    if (truncated_size_ > 0) {
      packet->pts = truncated_packet_.pts;
      packet->dts = truncated_packet_.dts;
      packet->flags = truncated_packet_.flags;
      packet->user_data = truncated_packet_.user_data;
      if (info) {
        memcpy(info, &truncated_info_, sizeof(PacketInfo));
        info->buffer_size = output_buffer_->Size();
        info->buffer_capacity = output_buffer_->Capacity();
      }
      if (packet->size < truncated_size_) {
        memcpy(packet->data, truncated_packet_.data + truncated_packet_.size - truncated_size_, packet->size);
        truncated_size_ -= packet->size;
        ret = packet->size;
      } else {
        packet->size = truncated_size_;
        memcpy(packet->data, truncated_packet_.data + truncated_packet_.size - truncated_size_, truncated_size_);
        ret = truncated_size_;
        truncated_size_ = 0;
        memset(&truncated_info_, 0, sizeof(PacketInfo));
      }
    } else if (packet->size < vpacket.packet.size) {
      output_buffer_->Read(reinterpret_cast<uint8_t *>(&vpacket), sizeof(IndexedVideoPacket));
      if (truncated_buffer_size_ < vpacket.packet.size) {
        if (truncated_packet_.data) delete[] truncated_packet_.data;
        truncated_packet_.data = new (std::nothrow) uint8_t[vpacket.packet.size];
        truncated_buffer_size_ = vpacket.packet.size;
      }
      truncated_packet_.size = vpacket.packet.size;
      truncated_packet_.pts = vpacket.packet.pts;
      truncated_packet_.dts = vpacket.packet.dts;
      truncated_packet_.flags = vpacket.packet.flags;
      truncated_packet_.user_data = vpacket.packet.user_data;
      output_buffer_->Read(truncated_packet_.data, vpacket.packet.size);
      packet->pts = truncated_packet_.pts;
      packet->dts = truncated_packet_.dts;
      packet->flags = truncated_packet_.flags;
      packet->user_data = truncated_packet_.user_data;
      memcpy(packet->data, truncated_packet_.data, packet->size);
      truncated_size_ = vpacket.packet.size - packet->size;
      ret = packet->size;
      GetPacketInfo(vpacket.index, &truncated_info_);
      if (info) {
        memcpy(info, &truncated_info_, sizeof(PacketInfo));
        info->buffer_size = output_buffer_->Size();
        info->buffer_capacity = output_buffer_->Capacity();
      }
      lk.unlock();
      output_cv_.notify_one();
    } else {
      output_buffer_->Read(reinterpret_cast<uint8_t *>(&vpacket), sizeof(IndexedVideoPacket));
      packet->size = vpacket.packet.size;
      packet->pts = vpacket.packet.pts;
      packet->dts = vpacket.packet.dts;
      packet->flags = vpacket.packet.flags;
      packet->user_data = vpacket.packet.user_data;
      ret = output_buffer_->Read(packet->data, packet->size);
      if (info) {
        GetPacketInfo(vpacket.index, info);
        info->buffer_size = output_buffer_->Size();
        info->buffer_capacity = output_buffer_->Capacity();
      } else {
        PacketInfo pi;
        GetPacketInfo(vpacket.index, &pi);
      }
      lk.unlock();
      output_cv_.notify_one();
    }
  }
  return ret;
}

}  // namespace video

}  // namespace cnstream
