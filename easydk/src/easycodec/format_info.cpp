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

#include <cn_codec_common.h>

#include <map>
#include <string>

#include "cxxutil/logger.h"
#include "easycodec/easy_decode.h"
#include "easycodec/vformat.h"
#include "format_info.h"

namespace edk {

#if 0
struct FormatInfoPriv {
  FormatInfoPriv(cncodecPixelFormat _fmt, unsigned int _plane_num, const char* _str, bool _supported) {
    fmt = _fmt;
    plane_num = _plane_num;
    fmt_str = _str;
    supported = _supported;
  }
  cncodecPixelFormat fmt;
  unsigned int plane_num = 0;
  const char* fmt_str = nullptr;
  bool supported = false;
};

#define FORMAT_PAIR(fmt, plane_num, supported) \
{ PixelFmt::fmt, {CNCODEC_PIX_FMT_##fmt, plane_num, #fmt, supported} }

/* static constexpr FormatInfoPriv kFrameFormatMap[19] = { */
static const std::map<PixelFmt, FormatInfoPriv> kFrameFormatMap = {
    FORMAT_PAIR(NV12,         2, true),
    FORMAT_PAIR(NV21,         2, true),
    FORMAT_PAIR(I420,         3, true),
    FORMAT_PAIR(YV12,         3, true),
    FORMAT_PAIR(YUYV,         1, true),
    FORMAT_PAIR(UYVY,         1, true),
    FORMAT_PAIR(YVYU,         1, true),
    FORMAT_PAIR(VYUY,         1, true),
    FORMAT_PAIR(P010,         2, false),
    FORMAT_PAIR(YUV420_10BIT, 2, false),
    FORMAT_PAIR(YUV444_10BIT, 3, false),
    FORMAT_PAIR(ARGB,         1, false),
    FORMAT_PAIR(ABGR,         1, false),
    FORMAT_PAIR(BGRA,         1, false),
    FORMAT_PAIR(RGBA,         1, false),
    FORMAT_PAIR(AYUV,         1, false),
    FORMAT_PAIR(RGB565,       1, false),
    FORMAT_PAIR(RAW,          1, false),
    FORMAT_PAIR(TOTAL_COUNT,  0, false),
};

static unsigned int kFrameFormatMapSize = kFrameFormatMap.size();

/* static_assert(sizeof(kFrameFormatMap) - 1 ==
 * static_cast<int>(PixelFmt::TOTAL_COUNT), "Format map has not cover all format"); */
/* static_assert(sizeof(kFrameFormatMap) - 1 == 18, "Format map has not cover all format"); */

FormatInfo::FormatInfo(PixelFmt fmt) {
  if (kFrameFormatMapSize - 1 != static_cast<int>(PixelFmt::TOTAL_COUNT)) {
    LOG(ERROR, "%u, %d", kFrameFormatMapSize - 1, static_cast<int>(PixelFmt::TOTAL_COUNT));
    std::string msg = "Format map has not cover all format";
    LOG(ERROR, msg.c_str());
    throw EasyDecodeError(msg);
  }
  const FormatInfoPriv &info = kFrameFormatMap.at(fmt);
  if (!info.supported) {
    std::string msg = std::string("Unsupported pixel format, PixelFmt::") + info.fmt_str;
    LOG(ERROR, msg.c_str());
    throw EasyDecodeError(msg);
  }

  edk_fmt_ = fmt;
  cncodec_fmt_ = info.fmt;
  plane_num_ = info.plane_num;
}
#endif

static inline unsigned int GetPlanesNum(cncodecPixelFormat fmt) {
  if (fmt == CNCODEC_PIX_FMT_NV12 || fmt == CNCODEC_PIX_FMT_NV21 ||
      fmt == CNCODEC_PIX_FMT_P010) {
    return 2;
  } else if (fmt == CNCODEC_PIX_FMT_I420 || fmt == CNCODEC_PIX_FMT_YV12) {
    return 3;
  }

  return 1;
}

unsigned int GetPlaneSize(cncodecPixelFormat fmt, unsigned int pitch, unsigned int height, unsigned int plane) {
  unsigned int plane_size;
  unsigned int plane_num = GetPlanesNum(fmt);
  if (plane >= plane_num) {
    LOG(ERROR, "Plane index out of range, %d vs %d", plane, plane_num);
    return 0;
  }
  if (fmt == CNCODEC_PIX_FMT_NV12 || fmt == CNCODEC_PIX_FMT_NV21 ||
      fmt == CNCODEC_PIX_FMT_I420 || fmt == CNCODEC_PIX_FMT_YV12 ||
      fmt == CNCODEC_PIX_FMT_P010) {
    plane_size = plane == 0 ? (pitch * height) : (pitch * (height >> 1));
  } else {
    plane_size = pitch * height;
  }

  return plane_size;
}

cncodecType CodecTypeCast(CodecType type) {
  switch (type) {
    case CodecType::MPEG2:  return CNCODEC_MPEG2;
    case CodecType::MPEG4:  return CNCODEC_MPEG4;
    case CodecType::H264:   return CNCODEC_H264;
    case CodecType::H265:   return CNCODEC_HEVC;
    case CodecType::VP8:    return CNCODEC_VP8;
    case CodecType::VP9:    return CNCODEC_VP9;
    case CodecType::AVS:    return CNCODEC_AVS;
    case CodecType::JPEG:   return CNCODEC_JPEG;
    default: throw edk::EasyDecodeError("Unsupport codec type");
  }
  return CNCODEC_H264;
}

cncodecColorSpace ColorStdCast(ColorStd color_std) {
  switch (color_std) {
    case ColorStd::ITU_BT_709:     return CNCODEC_COLOR_SPACE_BT_709;
    case ColorStd::ITU_BT_601:     return CNCODEC_COLOR_SPACE_BT_601;
    case ColorStd::ITU_BT_2020:    return CNCODEC_COLOR_SPACE_BT_2020;
    case ColorStd::ITU_BT_601_ER:  return CNCODEC_COLOR_SPACE_BT_601_ER;
    case ColorStd::ITU_BT_709_ER:  return CNCODEC_COLOR_SPACE_BT_709_ER;
    default: throw EasyDecodeError("Unsupport color space standard");
  }
}

cncodecPixelFormat PixelFormatCast(const PixelFmt& pixel_format) {
  switch (pixel_format) {
    case PixelFmt::NV12: return CNCODEC_PIX_FMT_NV12;
    case PixelFmt::NV21: return CNCODEC_PIX_FMT_NV21;
    case PixelFmt::I420: return CNCODEC_PIX_FMT_I420;
    case PixelFmt::YV12: return CNCODEC_PIX_FMT_YV12;
    case PixelFmt::YUYV: return CNCODEC_PIX_FMT_YUYV;
    case PixelFmt::UYVY: return CNCODEC_PIX_FMT_UYVY;
    case PixelFmt::YVYU: return CNCODEC_PIX_FMT_YVYU;
    case PixelFmt::VYUY: return CNCODEC_PIX_FMT_VYUY;
    case PixelFmt::P010: return CNCODEC_PIX_FMT_P010;
    case PixelFmt::YUV420_10BIT: return CNCODEC_PIX_FMT_YUV420_10BIT;
    case PixelFmt::YUV444_10BIT: return CNCODEC_PIX_FMT_YUV444_10BIT;
    case PixelFmt::ARGB: return CNCODEC_PIX_FMT_ARGB;
    case PixelFmt::ABGR: return CNCODEC_PIX_FMT_ABGR;
    case PixelFmt::BGRA: return CNCODEC_PIX_FMT_BGRA;
    case PixelFmt::RGBA: return CNCODEC_PIX_FMT_RGBA;
    case PixelFmt::AYUV: return CNCODEC_PIX_FMT_AYUV;
    case PixelFmt::RGB565: return CNCODEC_PIX_FMT_RGB565;
    default: throw edk::EasyDecodeError("Unsupport pixel format");
  }
  return CNCODEC_PIX_FMT_NV12;
}

}  // namespace edk

