#ifndef __RTSP_STREAM_PIPE__
#define __RTSP_STREAM_PIPE__

#include <stdint.h>

typedef struct StreamPipeCtx StreamPipeCtx;

typedef enum VideoColorFormat_enum {
  ColorFormat_NV12 = 0, /* Semi-Planar Y4-U1V1*/
  ColorFormat_NV21,     /* Semi-Planar Y4-V1U1*/
  ColorFormat_YUV420,   /* Planar Y4-U1-V1 */
  ColorFormat_BGR24,    /* Packed B8G8R8.*/
} ColorFormat;

typedef enum VideoCodeType_enum {
  VideoCodec_H264 = 0,
  VideoCodec_HEVC,
} VideoCodecType;

typedef enum CodecHWType_enum {
  FFMPEG = 0,
  MLU,
} VideoCodecHWType;
/**
 * RTSP StreamPipe context
 */
typedef struct {
  int fps = 25;  // target fps
  int udp_port = 8553;
  int http_port = 8080;
  int width_in = 1920;    // source width;
  int height_in = 1080;   // source height;
  int width_out = 1920;   // target width,prefered size same with input
  int height_out = 1080;  // target height,prefered size same with input
  int gop = 20;           // target gop,default is 10
  int kbps = 2 * 1024;    // target Kbps,default is 2*1024(2M)
  ColorFormat format = ColorFormat_BGR24;
  VideoCodecType codec = VideoCodec_H264;
  VideoCodecHWType hw = FFMPEG;
} StreamContext;

typedef void* RTSPStreamHandle;

#ifdef __cplusplus
extern "C" {
#endif

StreamPipeCtx* StreamPipeCreate(StreamContext* ctx);
int StreamPipePutPacket(StreamPipeCtx* ctx, uint8_t* data, int64_t timestamp = 0);
int StreamPipeClose(StreamPipeCtx* ctx);

#ifdef __cplusplus
}
#endif

#endif
