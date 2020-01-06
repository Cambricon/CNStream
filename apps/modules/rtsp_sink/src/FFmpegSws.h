
#ifndef __FFMPEG_SWS_H__
#define __FFMPEG_SWS_H__

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>  // av_image_alloc
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <string>
#include <mutex>

class FFSws {
 public:
  virtual ~FFSws();
  enum STATUS { STOP, LOCKED };
  int SetSrcOpt(AVPixelFormat pixfmt, int w, int h);
  int SetDstOpt(AVPixelFormat pixfmt, int w, int h);
  int LockOpt();
  int UnlockOpt();
  int Convert(const uint8_t* const srcSlice[], const int srcStride[],
            int srcSliceY, int srcSliceH, uint8_t* const dst[],
            const int dstStride[]);
  int Convert(const uint8_t* src_buffer, const size_t src_buffer_size,
              uint8_t* dst_buffer, const size_t dst_buffer_size);

 private:
  STATUS status_ = STOP;
  std::recursive_mutex mutex_;
  SwsContext* swsctx_ = nullptr;
  AVFrame* src_pic_ = nullptr;
  AVFrame* dst_pic_ = nullptr;
  AVPixelFormat src_pix_fmt_ = AV_PIX_FMT_NONE;
  AVPixelFormat dst_pix_fmt_ = AV_PIX_FMT_NONE;
  int src_w_ = 0;
  int src_h_ = 0;
  int dst_w_ = 0;
  int dst_h_ = 0;
};

#endif  // __FFMPEG_SWS_H__
