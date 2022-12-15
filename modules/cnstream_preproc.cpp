/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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

#include "cnstream_preproc.hpp"
#include <string>

// NV12ToBGR24 via cpu
//    to be optimized...
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#ifdef __cplusplus
extern "C" {
#endif
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
};
#endif
#include "cnstream_logging.hpp"

namespace cnstream {

Preproc *Preproc::Create(const std::string &name) { return ReflexObjectEx<Preproc>::CreateObject(name); }

CnedkTransformRect KeepAspectRatio(int src_w, int src_h, int dst_w, int dst_h) {
  float src_ratio = static_cast<float>(src_w) / src_h;
  float dst_ratio = static_cast<float>(dst_w) / dst_h;
  CnedkTransformRect res;
  memset(&res, 0, sizeof(res));
  if (src_ratio < dst_ratio) {
    int pad_lenth = dst_w - src_ratio * dst_h;
    pad_lenth = (pad_lenth % 2) ? pad_lenth - 1 : pad_lenth;
    if (dst_w - pad_lenth / 2 < 0) return res;
    res.width = dst_w - pad_lenth;
    res.left = pad_lenth / 2;
    res.top = 0;
    res.height = dst_h;
  } else if (src_ratio > dst_ratio) {
    int pad_lenth = dst_h - dst_w / src_ratio;
    pad_lenth = (pad_lenth % 2) ? pad_lenth - 1 : pad_lenth;
    if (dst_h - pad_lenth / 2 < 0) return res;
    res.height = dst_h - pad_lenth;
    res.top = pad_lenth / 2;
    res.left = 0;
    res.width = dst_w;
  } else {
    res.left = 0;
    res.top = 0;
    res.width = dst_w;
    res.height = dst_h;
  }
  return res;
}

int GetNetworkInfo(const infer_server::CnPreprocTensorParams *params, cnstream::CnPreprocNetworkInfo *info) {
  if (params->input_order == infer_server::DimOrder::NHWC) {
    info->n = params->input_shape[0];
    info->h = params->input_shape[1];
    info->w = params->input_shape[2];
    info->c = params->input_shape[3];
  } else if (params->input_order == infer_server::DimOrder::NCHW) {
    info->n = params->input_shape[0];
    info->c = params->input_shape[1];
    info->h = params->input_shape[2];
    info->w = params->input_shape[3];
  } else {
    LOGE(PREPROC) << "Unsupported input dim order: " << static_cast<int>(params->input_order);
    info->n = 0;
    info->c = 0;
    info->h = 0;
    info->w = 0;
    return -1;
  }
  info->dtype = params->input_dtype;
  info->format = params->input_format;
  return 0;
}

AVPixelFormat CastColorFormat(CnedkBufSurfaceColorFormat fmt) {
  switch (fmt) {
    case CNEDK_BUF_COLOR_FORMAT_NV12:
      return AV_PIX_FMT_NV12;
    case CNEDK_BUF_COLOR_FORMAT_NV21:
      return AV_PIX_FMT_NV21;
    case CNEDK_BUF_COLOR_FORMAT_RGB:
      return AV_PIX_FMT_RGB24;
    case CNEDK_BUF_COLOR_FORMAT_BGR:
      return AV_PIX_FMT_BGR24;
    case CNEDK_BUF_COLOR_FORMAT_RGBA:
      return AV_PIX_FMT_RGBA;
    case CNEDK_BUF_COLOR_FORMAT_BGRA:
      return AV_PIX_FMT_BGRA;
    case CNEDK_BUF_COLOR_FORMAT_ARGB:
      return AV_PIX_FMT_ARGB;
    case CNEDK_BUF_COLOR_FORMAT_ABGR:
      return AV_PIX_FMT_ABGR;
    default:
      LOGE(PREPROC) << "Cast color format from CnedkBufSurfaceColorFormat to AVPixelFormat failed."
                    << " Unsupported pixel format " << fmt;
      return AV_PIX_FMT_NONE;
  }
}

void YUV420spToRGBx(uint8_t *src_y, uint8_t *src_uv, int src_w, int src_h, int src_y_stride, int srv_uv_stride,
                    CnedkBufSurfaceColorFormat src_fmt, uint8_t *dst_rgbx, int dst_w, int dst_h, int dst_stride,
                    CnedkBufSurfaceColorFormat dst_fmt) {
  AVPixelFormat src_av_fmt = CastColorFormat(src_fmt);
  AVPixelFormat dst_av_fmt = CastColorFormat(dst_fmt);
  int yuv_linesize[4] = {src_y_stride, srv_uv_stride, 0, 0};
  int rgb_linesize[4] = {dst_stride, 0, 0, 0};
  unsigned char *inaddr[4] = {src_y, src_uv, 0, 0};
  unsigned char *outaddr[4] = {dst_rgbx, 0, 0, 0};
  SwsContext *sws_ctx =
      sws_getContext(src_w, src_h, src_av_fmt, dst_w, dst_h, dst_av_fmt, SWS_BILINEAR, NULL, NULL, NULL);
  sws_scale(sws_ctx, inaddr, yuv_linesize, 0, src_h, outaddr, rgb_linesize);
  sws_freeContext(sws_ctx);
}

void NV12ToBGR24(uint8_t *src_y, uint8_t *src_uv, int src_w, int src_h, int src_stride, uint8_t *dst_bgr24, int dst_w,
                   int dst_h, int dst_stride) {
  YUV420spToRGBx(src_y, src_uv, src_w, src_h, src_stride, src_stride, CNEDK_BUF_COLOR_FORMAT_NV12,
                 dst_bgr24, dst_w, dst_h, dst_stride, CNEDK_BUF_COLOR_FORMAT_BGR);
}

void NV21ToBGR24(uint8_t *src_y, uint8_t *src_uv, int src_w, int src_h, int src_stride, uint8_t *dst_bgr24, int dst_w,
                   int dst_h, int dst_stride) {
  YUV420spToRGBx(src_y, src_uv, src_w, src_h, src_stride, src_stride, CNEDK_BUF_COLOR_FORMAT_NV21,
                 dst_bgr24, dst_w, dst_h, dst_stride, CNEDK_BUF_COLOR_FORMAT_BGR);
}

}  // namespace cnstream
