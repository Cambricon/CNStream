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
#include <string>

#include "easycodec/vformat.h"

namespace edk {

struct FormatInfo {
  PixelFmt edk_fmt;
  cncodecPixelFormat cncodec_fmt;
  unsigned int plane_num;
  std::string fmt_str;
  bool supported;

  static const FormatInfo* GetFormatInfo(PixelFmt fmt);
  unsigned int GetPlaneSize(unsigned int pitch, unsigned int height, unsigned int plane) const;
};

cncodecType CodecTypeCast(CodecType type);
cncodecColorSpace ColorStdCast(ColorStd color_std);

}  // namespace edk

#endif  // FORMAT_INFO_H_
