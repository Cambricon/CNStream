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

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

#include "scaler.hpp"

namespace cnstream {

extern void ScalerFillBufferStride(Scaler::Buffer *buffer);

static AVPixelFormat ffmpeg_color_map[Scaler::ColorFormat::COLOR_MAX] = {
  AV_PIX_FMT_YUV420P,
  AV_PIX_FMT_NV12,
  AV_PIX_FMT_NV21,
  AV_PIX_FMT_BGR24,
  AV_PIX_FMT_RGB24,
  AV_PIX_FMT_BGRA,
  AV_PIX_FMT_RGBA,
  AV_PIX_FMT_ABGR,
  AV_PIX_FMT_ARGB,
};

static void FFmpegCopy(const Scaler::Buffer *src, Scaler::Buffer *dst) {
  if (src->color <= Scaler::ColorFormat::YUV_NV21) {
    for (uint32_t i = 0; i < src->height; i++) {
      memcpy(dst->data[0] + dst->stride[0] * i, src->data[0] + src->stride[0] * i, src->width);
    }
    if (src->color != Scaler::ColorFormat::YUV_I420) {
      for (uint32_t i = 0; i < (src->height / 2); i++) {
        memcpy(dst->data[1] + dst->stride[1] * i, src->data[1] + src->stride[1] * i, src->width);
      }
    } else {
      for (uint32_t i = 0; i < (src->height / 2); i++) {
        memcpy(dst->data[1] + dst->stride[1] * i, src->data[1] + src->stride[1] * i, src->width / 2);
      }
      for (uint32_t i = 0; i < (src->height / 2); i++) {
        memcpy(dst->data[2] + dst->stride[2] * i, src->data[2] + src->stride[2] * i, src->width / 2);
      }
    }
  } else if (src->color <= Scaler::ColorFormat::RGB) {
    for (uint32_t i = 0; i < src->height; i++) {
      memcpy(dst->data[0] + dst->stride[0] * i, src->data[0] + src->stride[0] * i, src->width * 3);
    }
  } else {
    for (uint32_t i = 0; i < src->height; i++) {
      memcpy(dst->data[0] + dst->stride[0] * i, src->data[0] + src->stride[0] * i, src->width * 4);
    }
  }
}

bool FFmpegProcess(const Scaler::Buffer *src, Scaler::Buffer *dst) {
  if (!src || !dst) return false;

  int ret;
  if (src->width == dst->width && src->height == dst->height && src->color == dst->color) {
    FFmpegCopy(src, dst);
    return true;
  }

  uint8_t *src_data[4], *dst_data[4];
  int src_linesize[4], dst_linesize[4];
  memset(src_data, 0, sizeof(src_data));
  memset(src_linesize, 0, sizeof(src_linesize));
  memset(dst_data, 0, sizeof(dst_data));
  memset(dst_linesize, 0, sizeof(dst_linesize));
  src_data[0] = src->data[0];
  src_linesize[0] = src->stride[0];
  if (src->color <= Scaler::YUV_NV21) {
    src_data[1] = src->data[1];
    src_linesize[1] = src->stride[1];
    if (src->color == Scaler::YUV_I420) {
      src_data[2] = src->data[2];
      src_linesize[2] = src->stride[2];
    }
  }
  dst_data[0] = dst->data[0];
  dst_linesize[0] = dst->stride[0];
  if (dst->color <= Scaler::YUV_NV21) {
    dst_data[1] = dst->data[1];
    dst_linesize[1] = dst->stride[1];
    if (dst->color == Scaler::YUV_I420) {
      dst_data[2] = dst->data[2];
      dst_linesize[2] = dst->stride[2];
    }
  }

  SwsContext *sws_ctx = sws_getContext(src->width, src->height, ffmpeg_color_map[src->color],
                                       dst->width, dst->height, ffmpeg_color_map[dst->color],
                                       SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
  if (!sws_ctx) {
    LOGE(ScalerFFmpeg) << "FFmpegProcess() sws_getContext failed";
    return false;
  }
  ret = sws_scale(sws_ctx, src_data, src_linesize, 0, src->height, dst_data, dst_linesize);
  if (ret < 0) {
    LOGE(ScalerFFmpeg) << "FFmpegProcess() sws_scale failed, ret=" << ret;
    return false;
  }

  if (sws_ctx) sws_freeContext(sws_ctx);

  return true;
}

}  // namespace cnstream
