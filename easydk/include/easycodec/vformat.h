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
  NON_FORMAT,     ///< No format
  YUV420SP_NV21,  ///< NV21, YUV family
  YUV420SP_NV12,  ///< NV12, YUV family
  BGR24,          ///< BGR24, 24 bit BGR format
  RGB24           ///< RGB24, 24 bit RGB format
};

/**
 * @brief Enumeration to describe data codec type
 * @note Type contains both video and image
 */
enum class CodecType {
  MPEG4,  ///< MPEG4 video codec standard
  H264,   ///< H.264 video codec standard
  H265,   ///< H.265 video codec standard, aka HEVC
  MJPEG,  ///< Motion JPEG video codec standard
  JPEG    ///< JPEG image format
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
  uint32_t buf_id;
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
  /// MLU channel in which memory stored, not supported on MLU100
  int channel_id;
  /// Plane count for this frame, always be 1 on MLU100.
  uint32_t n_planes;
  /// Frame strides for each plane
  uint32_t strides[CN_MAXIMUM_PLANE];
  /// Frame data pointer
  void* ptrs[CN_MAXIMUM_PLANE];
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
  uint32_t buf_id;
  /// Frame data pointer
  void* data;
  /// Frame length, unit pixel
  uint64_t length;
  /// Presentation time stamp
  uint64_t pts;
  /// Video codec type, @see CodecType
  CodecType codec_type;
};

}  // namespace edk

#endif  // EASYCODEC_VFORMAT_H_
