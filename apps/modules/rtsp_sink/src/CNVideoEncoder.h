#ifndef __CN_VIDEO_ENCODER_H__
#define __CN_VIDEO_ENCODER_H__

#include "easycodec/easy_encode.h"
#include "easycodec/vformat.h"

#include "VideoEncoder.h"

class CNVideoEncoder : public VideoEncoder {
 public:
  explicit CNVideoEncoder(uint32_t width, uint32_t height, PictureFormat format, CodecType type, float frame_rate,
                          uint32_t gop_size, uint32_t bit_rate);
  ~CNVideoEncoder();

  uint32_t GetBitrate() { return bit_rate_; };

  friend class CNVideoFrame;

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

  VideoFrame *NewFrame() override;
  void EncodeFrame(VideoFrame *frame) override;

  void PacketCallback(const edk::CnPacket &packet);
  void EosCallback();
  void PerfCallback(const edk::EncodePerfInfo &info);
  void Destroy();

  uint32_t picture_width_;
  uint32_t picture_height_;
  edk::PixelFmt picture_format_ = edk::PixelFmt::BGR24;
  edk::CodecType codec_type_ = edk::CodecType::H264;
  uint32_t frame_rate_num_;
  uint32_t frame_rate_den_;
  uint32_t gop_size_;
  uint32_t bit_rate_;
  uint32_t frame_count_ = 0;
  bool copy_frame_buffer_ = false;

  edk::EasyEncode *encoder_ = nullptr;
};  // CNVideoEncoder
#endif
