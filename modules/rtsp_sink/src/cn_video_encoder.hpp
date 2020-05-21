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

#ifndef MODULES_RTSP_SINK_SRC_CN_VIDEO_ENCODER_HPP_
#define MODULES_RTSP_SINK_SRC_CN_VIDEO_ENCODER_HPP_

#include <string>

#include "easycodec/easy_encode.h"
#include "easycodec/vformat.h"
#include "easyinfer/mlu_context.h"

#include "rtsp_sink.hpp"
#include "video_encoder.hpp"

namespace cnstream {

class CNVideoEncoder : public VideoEncoder {
 public:
  explicit CNVideoEncoder(const RtspParam& rtsp_param);
  ~CNVideoEncoder();
  friend class CNVideoFrame;
  uint32_t GetBitRate() { return rtsp_param_.kbps * 1000; }

 private:
  class CNVideoFrame : public VideoFrame {
   public:
    explicit CNVideoFrame(CNVideoEncoder *encoder);
    ~CNVideoFrame();
    void Fill(uint8_t *data, int64_t timestamp) override;
    edk::CnFrame *Get() { return frame_; }

   private:
    CNVideoEncoder *encoder_ = nullptr;
    edk::CnFrame *frame_ = nullptr;
  };

  virtual VideoFrame *NewFrame();
  virtual void EncodeFrame(VideoFrame *frame);
  // virtual void EncodeFrame(void *y, void *uv, int64_t timestamp);

  void Destroy();
  void EosCallback();
  void PacketCallback(const edk::CnPacket &packet);
  uint32_t GetOffset(const uint8_t* data);

  RtspParam rtsp_param_;
  uint32_t frame_count_ = 0;
  uint32_t frame_rate_num_;
  uint32_t frame_rate_den_;
  edk::CodecType codec_type_;
  edk::PixelFmt picture_format_;
  edk::EasyEncode *encoder_ = nullptr;

  std::string preproc_type_;
  // edk::MluResizeYuv2Yuv *resize_ = nullptr;
};  // CNVideoEncoder

}  // namespace cnstream

#endif  // MODULES_RTSP_SINK_SRC_CN_VIDEO_ENCODER_HPP_
