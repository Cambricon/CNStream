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

#ifndef __RTSP_STREAM_PIPE__
#define __RTSP_STREAM_PIPE__

#include <stdint.h>

typedef struct StreamPipeCtx StreamPipeCtx;

typedef enum VideoColorFormat_enum {
  ColorFormat_YUV420 = 0,   /* Planar Y4-U1-V1 */
  ColorFormat_RGB24,
  ColorFormat_BGR24,    /* Packed B8G8R8.*/
  ColorFormat_NV21,     /* Semi-Planar Y4-V1U1*/
  ColorFormat_NV12,     /* Semi-Planar Y4-U1V1*/
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
  ColorFormat format = ColorFormat_NV21;
  VideoCodecType codec = VideoCodec_H264;
  VideoCodecHWType hw = FFMPEG;  // FFMPEG
} StreamContext;

typedef void* RTSPStreamHandle;

#ifdef __cplusplus
extern "C" {
#endif

StreamPipeCtx* StreamPipeCreate(StreamContext* ctx, uint32_t device_id);
int StreamPipePutPacket(StreamPipeCtx* ctx, uint8_t* data, int64_t timestamp = 0);
int StreamPipeClose(StreamPipeCtx* ctx);

#ifdef __cplusplus
}
#endif

#endif
