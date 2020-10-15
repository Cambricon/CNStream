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

#ifndef MODULES_COMMON_HPP_
#define MODULES_COMMON_HPP_

#define JEPG_ENC_ALIGNMENT 64
#define DEC_ALIGNMENT 64
#define ALIGN(size, alignment) (((uint32_t)(size) + (alignment)-1) & ~((alignment)-1))

namespace cnstream {
/**
 * @brief The enum of picture format
 */
enum  CNPixelFormat{
  YUV420P = 0,  /// Planar Y4-U1-V1
  RGB24,        /// Packed R8G8B8
  BGR24,        /// Packed B8G8R8
  NV21,         /// Semi-Planar Y4-V1U1
  NV12,         /// Semi-Planar Y4-U1V1
};
/**
 * @brief The enum of codec type
 */
enum CNCodecType {
  H264 = 0,   /// H264
  HEVC,       /// HEVC
  MPEG4,      /// MPEG4
  JPEG        /// JPEG
};

}  // namespace cnstream

#endif  // MODULES_FORMAT_HPP_
