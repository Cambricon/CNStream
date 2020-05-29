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

#ifndef MODULES_RTSP_SINK_SRC_FFMPEG_VIDEO_ENCODER_HPP_
#define MODULES_RTSP_SINK_SRC_FFMPEG_VIDEO_ENCODER_HPP_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include "rtsp_sink.hpp"
#include "video_encoder.hpp"

namespace cnstream {

class FFmpegVideoEncoder : public VideoEncoder {
 public:
  explicit FFmpegVideoEncoder(const RtspParam& rtsp_param);
  ~FFmpegVideoEncoder();

  uint32_t GetBitRate() {
    if (avcodec_ctx_ != nullptr) {
      return avcodec_ctx_->bit_rate;
    } else {
      LOG(ERROR) << "avcodec_ctx_ is nullptr.";
      return 0;
    }
  }

  friend class FFmpegVideoFrame;

 private:
  class FFmpegVideoFrame : public VideoFrame {
   public:
    explicit FFmpegVideoFrame(FFmpegVideoEncoder *encoder);
    ~FFmpegVideoFrame();
    void Fill(uint8_t *data, int64_t timestamp) override;
    AVFrame *Get() { return frame_; }

   private:
    FFmpegVideoEncoder *encoder_ = nullptr;
    AVFrame *frame_ = nullptr;
  };

  virtual VideoFrame *NewFrame();
  uint32_t GetOffset(const uint8_t* data);
  virtual void EncodeFrame(VideoFrame *frame);
  // virtual void EncodeFrame(void *y, void *uv, int64_t timestamp) {return;};
  void Destroy();

  AVPixelFormat picture_format_;
  AVRational frame_rate_;
  uint32_t frame_count_ = 0;

  enum AVCodecID avcodec_id_ = AV_CODEC_ID_H264;
  AVCodecContext *avcodec_ctx_ = nullptr;
  AVCodec *avcodec_ = nullptr;
  AVDictionary *avcodec_opts_ = nullptr;
  AVFrame *sync_input_avframe_ = nullptr;
  AVFrame *avframe_ = nullptr;
  AVPacket *avpacket_ = nullptr;
  SwsContext *sws_ctx_ = nullptr;
};  // FFmpegVideoEncoder

}  // namespace cnstream

#endif  // MODULES_RTSP_SINK_SRC_FFMPEG_VIDEO_ENCODER_HPP_
