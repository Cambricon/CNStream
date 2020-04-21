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

#ifndef FORMAT_INFO_H_
#define FORMAT_INFO_H_

#include <cn_codec_common.h>

#include "easycodec/vformat.h"

namespace edk {

#if 0
class FormatInfo {
 public:
  explicit FormatInfo(PixelFmt fmt);
  inline cncodecPixelFormat GetCncodecFormat() {
    return cncodec_fmt_;
  }
  inline unsigned int GetPlanesNum() {
    return plane_num_;
  }
  unsigned int GetPlaneSize(unsigned int pitch, unsigned int height, unsigned int plane);
 private:
  PixelFmt edk_fmt_;
  cncodecPixelFormat cncodec_fmt_;
  unsigned int plane_num_;
};
#endif

cncodecType CodecTypeCast(CodecType type);
cncodecColorSpace ColorStdCast(ColorStd color_std);
cncodecPixelFormat PixelFormatCast(const PixelFmt& pixel_format);
unsigned int GetPlaneSize(cncodecPixelFormat fmt, unsigned int pitch, unsigned int height, unsigned int plane);

}  // namespace edk

#endif  // FORMAT_INFO_H_

