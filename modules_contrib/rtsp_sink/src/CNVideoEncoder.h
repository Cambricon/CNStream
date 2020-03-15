#ifndef __CN_VIDEO_ENCODER_H__
#define __CN_VIDEO_ENCODER_H__

#include "VideoEncoder.h"
#include "easycodec/easy_encode.h"
#include "easycodec/vformat.h"

class CNVideoEncoder : public VideoEncoder {
 public:
  explicit CNVideoEncoder(uint32_t width, uint32_t height, PictureFormat format, CodecType type, float frame_rate,
                          uint32_t gop_size, uint32_t bit_rate, uint32_t device_id);
  ~CNVideoEncoder();
  friend class CNVideoFrame;
  uint32_t GetBitrate() { return bit_rate_; }

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

  void Destroy();
  void EosCallback();
  void PacketCallback(const edk::CnPacket &packet);
  uint32_t GetOffSet(const uint8_t *data);

  uint32_t picture_width_;
  uint32_t picture_height_;
  edk::PixelFmt picture_format_;
  edk::CodecType codec_type_ = edk::CodecType::H264;
  uint32_t frame_rate_num_;
  uint32_t frame_rate_den_;
  uint32_t gop_size_;
  uint32_t bit_rate_;
  uint32_t device_id_;
  uint32_t frame_count_ = 0;
  edk::EasyEncode *encoder_ = nullptr;
};  // CNVideoEncoder
#endif
