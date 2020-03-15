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

/**
 * @file vformat.h
 *
 * This file contains a declaration of structures used in decode and encode.
 */

#ifndef EASYCODEC_VFORMAT_H_
#define EASYCODEC_VFORMAT_H_

#include <cstdint>

#define CN_MAXIMUM_PLANE 6

namespace edk {

/**
 * @brief Structure to describe resolution of video or image.
 */
struct Geometry {
  unsigned int w;  ///< width in pixel
  unsigned int h;  ///< height in pixel
};

/**
 * @brief Enumeration to describe image colorspace.
 */
enum class PixelFmt {
  NV12 = 0,  ///< NV12, YUV family
  NV21,  ///< NV21, YUV family
  I420,
  YV12,
  YUYV,
  UYVY,
  YVYU,
  VYUY,
  P010,
  YUV420_10BIT,
  YUV444_10BIT,
  ARGB,
  ABGR,
  BGRA,
  RGBA,
  AYUV,
  RGB565,
  RAW,     ///< No format
  TOTAL_COUNT
};

/**
 * @brief Enumeration to describe data codec type
 * @note Type contains both video and image
 */
enum class CodecType {
  MPEG2,
  MPEG4,  ///< MPEG4 video codec standard
  H264,   ///< H.264 video codec standard
  H265,   ///< H.265 video codec standard, aka HEVC
  VP8,
  VP9,
  AVS,
  MJPEG,  ///< Motion JPEG video codec standard
  JPEG    ///< JPEG image format
};

enum class ColorStd {
  ITU_BT_709 = 0,   /* ITU BT 709 color standard */
  ITU_BT_601,       /* ITU BT.601 color standard */
  ITU_BT_2020,      /* ITU BT 2020 color standard */
  ITU_BT_601_ER,    /* ITU BT 601 color standard extend range */
  ITU_BT_709_ER,    /* ITU BT 709 color standard extend range */
  COLOR_STANDARD_INVALID,
};

enum class BufferStrategy {
  CNCODEC,
  EDK
};

/**
 * @brief Structure contains raw data and informations
 * @note Used as output in decode and input in encode
 */
struct CnFrame {
  /**
   * Used to release buffer in EasyDecode::ReleaseBuffer
   * when frame memory from decoder will not be used. Useless in encoder.
   */
  uint64_t buf_id;
  /// Presentation time stamp
  uint64_t pts;
  /// Frame height in pixel
  uint32_t height;
  /// Frame width in pixel
  uint32_t width;
  /// Frame data size, unit: byte
  uint64_t frame_size;
  /// Frame color space, @see PixelFmt
  PixelFmt pformat;
  /// Color standard
  ColorStd color_std;
  /// MLU device identification
  int device_id;
  /// MLU channel in which memory stored
  int channel_id;
  /// Plane count for this frame
  uint32_t n_planes;
  /// Frame strides for each plane
  uint32_t strides[CN_MAXIMUM_PLANE];
  /// Frame data pointer
  void* ptrs[CN_MAXIMUM_PLANE];
};

/**
 * @brief Encode bitstream slice type.
 */
enum class BitStreamSliceType {
  SPS_PPS,
  FRAME
};

/**
 * @brief Structure contains encoded data and informations
 * @note Used as output in encode and input in decode
 */
struct CnPacket {
  /**
   * Used to release buffer in EasyEncode::ReleaseBuffer
   * when memory from encoder will not be used. Useless in decoder.
  */
  uint64_t buf_id;
  /// Frame data pointer
  void* data;
  /// Frame length, unit pixel
  uint64_t length;
  /// Presentation time stamp
  uint64_t pts;
  /// Video codec type, @see CodecType
  CodecType codec_type;
  /// Bitstream slice type, only used in EasyEncode, @see BitStreamSliceType
  BitStreamSliceType slice_type;
};

}  // namespace edk

#endif  // EASYCODEC_VFORMAT_H_

