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
#include <glog/logging.h>

#include <map>
#include <string>

#include "easycodec/easy_decode.h"
#include "easycodec/vformat.h"
#include "format_info.h"

namespace edk {

#define FORMAT_PAIR(fmt, plane_num, supported)                                                     \
  {                                                                                                \
    PixelFmt::fmt, FormatInfo { PixelFmt::fmt, CNCODEC_PIX_FMT_##fmt, plane_num, #fmt, supported } \
  }

static const std::map<PixelFmt, FormatInfo> kFrameFormatMap = {
    FORMAT_PAIR(NV12, 2, true),          FORMAT_PAIR(NV21, 2, true),          FORMAT_PAIR(I420, 3, true),
    FORMAT_PAIR(YV12, 3, true),          FORMAT_PAIR(YUYV, 1, true),          FORMAT_PAIR(UYVY, 1, true),
    FORMAT_PAIR(YVYU, 1, true),          FORMAT_PAIR(VYUY, 1, true),          FORMAT_PAIR(P010, 2, false),
    FORMAT_PAIR(YUV420_10BIT, 2, false), FORMAT_PAIR(YUV444_10BIT, 3, false), FORMAT_PAIR(ARGB, 1, false),
    FORMAT_PAIR(ABGR, 1, false),         FORMAT_PAIR(BGRA, 1, false),         FORMAT_PAIR(RGBA, 1, false),
    FORMAT_PAIR(AYUV, 1, false),         FORMAT_PAIR(RGB565, 1, false),       FORMAT_PAIR(RAW, 1, false),
    FORMAT_PAIR(TOTAL_COUNT, 0, false),
};
#undef FORMAT_PAIR

const FormatInfo* FormatInfo::GetFormatInfo(PixelFmt fmt) {
  auto fmt_pair = kFrameFormatMap.find(fmt);
  if (fmt_pair == kFrameFormatMap.end()) {
    LOG(ERROR) << "Unsupport pixel format";
    THROW_EXCEPTION(Exception::UNSUPPORTED, "Unsupport pixel format");
  }
  return &(fmt_pair->second);
}

unsigned int FormatInfo::GetPlaneSize(unsigned int pitch, unsigned int height, unsigned int plane) const {
  unsigned int plane_size;
  if (plane >= plane_num) {
    LOG(ERROR) << "Plane index out of range, " << plane << " vs " << plane_num;
    return 0;
  }
  const cncodecPixelFormat& fmt = cncodec_fmt;
  if (fmt == CNCODEC_PIX_FMT_NV12 || fmt == CNCODEC_PIX_FMT_NV21 || fmt == CNCODEC_PIX_FMT_I420 ||
      fmt == CNCODEC_PIX_FMT_YV12 || fmt == CNCODEC_PIX_FMT_P010) {
    plane_size = plane == 0 ? (pitch * height) : (pitch * (height >> 1));
  } else {
    plane_size = pitch * height;
  }

  return plane_size;
}

cncodecType CodecTypeCast(CodecType type) {
  switch (type) {
    case CodecType::MPEG2:
      return CNCODEC_MPEG2;
    case CodecType::MPEG4:
      return CNCODEC_MPEG4;
    case CodecType::H264:
      return CNCODEC_H264;
    case CodecType::H265:
      return CNCODEC_HEVC;
    case CodecType::VP8:
      return CNCODEC_VP8;
    case CodecType::VP9:
      return CNCODEC_VP9;
    case CodecType::AVS:
      return CNCODEC_AVS;
    case CodecType::JPEG:
      return CNCODEC_JPEG;
    default:
      THROW_EXCEPTION(Exception::UNSUPPORTED, "Unsupport codec type");
  }
  return CNCODEC_H264;
}

cncodecColorSpace ColorStdCast(ColorStd color_std) {
  switch (color_std) {
    case ColorStd::ITU_BT_709:
      return CNCODEC_COLOR_SPACE_BT_709;
    case ColorStd::ITU_BT_601:
      return CNCODEC_COLOR_SPACE_BT_601;
    case ColorStd::ITU_BT_2020:
      return CNCODEC_COLOR_SPACE_BT_2020;
    case ColorStd::ITU_BT_601_ER:
      return CNCODEC_COLOR_SPACE_BT_601_ER;
    case ColorStd::ITU_BT_709_ER:
      return CNCODEC_COLOR_SPACE_BT_709_ER;
    default:
      THROW_EXCEPTION(Exception::UNSUPPORTED, "Unsupport color space standard");
  }
}

}  // namespace edk
