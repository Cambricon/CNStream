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

#ifndef _CNPREPROCESS_RESIZE_AND_CONVERT_H_
#define _CNPREPROCESS_RESIZE_AND_CONVERT_H_

#include <cnrt.h>
#include <string>
#include "cnbase/streamlibs_error.h"

struct KernelParam;
namespace libstream {

/***********************************************************
 * @brief mlu resize and convert operator
 ***********************************************************/
STREAMLIBS_REGISTER_EXCEPTION(MluRCOp);
class MluRCOp {
 public:
  ~MluRCOp();
  enum ColorMode {
    RGBA2RGBA = 0,
    YUV2RGBA_NV12 = 1,
    YUV2RGBA_NV21 = 2,
    YUV2BGRA_NV12 = 3,
    YUV2BGRA_NV21 = 4,
    YUV2ARGB_NV12 = 5,
    YUV2ARGB_NV21 = 6,
    YUV2ABGR_NV12 = 7,
    YUV2ABGR_NV21 = 8
  };  // enum Mode
  enum DataMode {
    FP162FP16 = 0,
    FP162UINT8 = 1,
    UINT82FP16 = 2,
    UINT82UINT8 = 3
  };  // enum DataMode
  inline int color_mode() const {
    return static_cast<int>(cmode_);
  }
  inline void set_cmode(ColorMode m) {
    cmode_ = m;
  }
  inline int data_mode() const {
    return static_cast<int>(dmode_);
  }
  inline void set_dmode(DataMode m) {
    dmode_ = m;
  }
  inline void set_src_resolution(uint32_t w, uint32_t h, uint32_t stride = 1) {
    src_w_ = w;
    src_h_ = h;
    src_stride_ = stride;
  }
  inline void get_src_resolution(uint32_t* w, uint32_t* h, uint32_t* stride) {
    *w = src_w_;
    *h = src_h_;
    *stride = src_stride_;
  }
  inline void set_crop_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    crop_x_ = x;
    crop_y_ = y;
    crop_w_ = w;
    crop_h_ = h;
  }
  inline void get_crop_rect(uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h) {
    *x = crop_x_;
    *y = crop_y_;
    *w = crop_w_;
    *h = crop_h_;
  }
  inline void set_dst_resolution(uint32_t w, uint32_t h) {
    dst_w_ = w;
    dst_h_ = h;
  }
  inline void get_dst_resolution(uint32_t* w, uint32_t* h) {
    *w = dst_w_;
    *h = dst_h_;
  }
  inline void set_ftype(cnrtFunctionType_t ftype) {
    ftype_ = ftype;
  }
  inline cnrtFunctionType_t ftype() const {
    return ftype_;
  }
  inline void set_cnrt_stream(cnrtStream_t stream) {
    rt_stream_ = stream;
  }
  inline cnrtStream_t cnrt_stream() const {
    return rt_stream_;
  }
  inline KernelParam* kernel_param() const {
    return kparam_;
  }
  /****************************************
   * @brief init op
   ****************************************/
  bool Init();
  /****************************************
   * @brief excute
   ****************************************/
  float InvokeOp(void* dst, void* src_y, void* src_uv);
  void Destroy();
  inline std::string GetLastError() const {
    return estr_;
  }

 private:
  ColorMode cmode_ = YUV2RGBA_NV21;
  DataMode dmode_ = UINT82UINT8;
  uint32_t src_w_, src_h_, src_stride_, dst_w_, dst_h_;
  uint32_t crop_x_ = 0, crop_y_ = 0, crop_w_ = 0, crop_h_ = 0;
  cnrtFunctionType_t ftype_;
  cnrtStream_t rt_stream_ = nullptr;
  KernelParam* kparam_ = nullptr;
  std::string estr_;
};  // class MluResizeAndConvertOp

}  // namespace libstream

#endif  // _CNPREPROCESS_RESIZE_AND_CONVERT_H_

