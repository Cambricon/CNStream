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

/**
 * @file resize_common.h
 *
 * This file contains a declaration of common data type for resize operator.
 */

#ifndef EASYBANG_RESIZE_COMMON_H_
#define EASYBANG_RESIZE_COMMON_H_

#include <string>
#include "device/mlu_context.h"
#include "cxxutil/exception.h"
#include "easyinfer/easy_infer.h"
#include "cnrt.h"
#include "cnml.h"

namespace edk {

const int kMLU270CoreNum = 16;
const int kMLU220CoreNum = 4;

inline bool CnrtCheck(cnrtRet_t cnrtret, std::string* estr, const std::string& msg) {
  if (CNRT_RET_SUCCESS != cnrtret) {
    *estr = "CNRT " + msg + " ERRCODE:" + std::to_string(cnrtret);
    return false;
  }
  return true;
}
inline bool CnmlCheck(cnmlStatus_t cnmlret, std::string* estr, const std::string& msg) {
  if (CNML_STATUS_SUCCESS != cnmlret) {
    *estr = "CNML " + msg + " ERRCODE:" + std::to_string(cnmlret);
    return false;
  }
  return true;
}
/**
 * @brief Enumeration to specify color convert mode
 */
enum class ColorMode {
  RGBA2RGBA = 0,      ///< Convert color from RGBA to RGBA
  YUV2RGBA_NV12 = 1,  ///< Convert color from NV12 to RGBA
  YUV2RGBA_NV21 = 2,  ///< Convert color from NV21 to RGBA
  YUV2BGRA_NV12 = 3,  ///< Convert color from NV12 to BGRA
  YUV2BGRA_NV21 = 4,  ///< Convert color from NV21 to BGRA
  YUV2ARGB_NV12 = 5,  ///< Convert color from NV12 to ARGB
  YUV2ARGB_NV21 = 6,  ///< Convert color from NV21 to ARGB
  YUV2ABGR_NV12 = 7,  ///< Convert color from NV12 to ABGR
  YUV2ABGR_NV21 = 8   ///< Convert color from NV21 to ABGR
};                    // enum Mode

/**
 * @brief Enumeration to specify date transform mode
 */
enum class DataMode {
  FP16ToFP16 = 0,   ///< Transform data type from float16 to float16
  FP16ToUINT8 = 1,  ///< Transform data type from float16 to uint8
  UINT8ToFP16 = 2,  ///< Transform data type from uint8 to float16
  UINT8ToUINT8 = 3  ///< Transform data type from uint8 to uint8
};                  // enum DataMode

/**
 * @brief Params to initialize resize operator
 * 
 * If yuv2rgba, all the attributes needed to be set.
 * If yuv2yuv, only the following attributes will be used:
 * src_w, src_h, dst_w, dst_h, batch_size, core_version, core_limit
 */
struct MluResizeAttr {
  /// Color convert mode
  ColorMode color_mode = ColorMode::YUV2RGBA_NV21;
  /// Data type transform mode
  DataMode data_mode = DataMode::UINT8ToUINT8;
  /// Input image resolution
  uint32_t src_w, src_h, src_stride;
  /// Output image resolution
  uint32_t dst_w, dst_h;
  /// Crop rectangle (top-left coordinate, width, height)
  uint32_t crop_x = 0, crop_y = 0, crop_w = 0, crop_h = 0;
  uint8_t fill_color_r = 255, fill_color_g = 0, fill_color_b = 0;
  int keep_aspect_ratio = 0;
  /// Kernel batch size
  int batch_size = 1;
  /// device id
  CoreVersion core_version = CoreVersion::MLU270;
  /// number of cores used, choose from 1, 4, 8 or 16
  int core_number = 4;
};  // struct MluResizeAttr
}  // namespace edk

#endif  // EASYBANG_RESIZE_COMMON_H_
