#ifndef __FFMPEG_VIDEO_ENCODER_H__
#define __FFMPEG_VIDEO_ENCODER_H__

#include "VideoEncoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

class FFmpegVideoEncoder : public VideoEncoder {
 public:
  explicit FFmpegVideoEncoder(uint32_t width, uint32_t height, PictureFormat format, CodecType type, float frame_rate,
                              uint32_t gop_size, uint32_t bit_rate);
  ~FFmpegVideoEncoder();

  uint32_t GetBitrate() { return bit_rate_; }

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
  uint32_t GetOffSet(const uint8_t *data);
  virtual void EncodeFrame(VideoFrame *frame);
  void Destroy();

  uint32_t picture_width_;
  uint32_t picture_height_;
  AVPixelFormat picture_format_;
  AVRational frame_rate_;
  uint32_t gop_size_;
  uint32_t bit_rate_;
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
#endif
