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

#include <cnrt.h>
#include <sys/time.h>
#include <cassert>
#include <iostream>
#include <string>

#include "resize_and_convert_kernel.h"
#include "ResizeAndConvertMacro.h"

using std::string;
using std::to_string;

struct KernelParam {
  half* consts_mlu = nullptr;
  // half* maskUV_mlu = nullptr;
  uint8_t* fill_color = nullptr;  // pad value
  half* yuvFilter = nullptr;
  half* yuvBias = nullptr;
  int d_row, d_col;
  int input2half = 1, output2uint = 1;
  int batchNum = 1;
  int keep_aspect_ratio = 0;
  cnrtKernelInitParam_t init_param = nullptr;
  void *kernel_func = nullptr;
  int padMethod = 0;
};

void FreeKernelParam(KernelParam* param) {
  if (param) {
    if (param->consts_mlu) {
      cnrtFree(param->consts_mlu);
    }
    if (param->fill_color) {
      cnrtFree(param->fill_color);
    }
    if (param->init_param) {
      cnrtDestroyKernelInitParamAndMemory(param->init_param);
    }
    //   if (param->maskUV_mlu) {
    //     cnrtFree(param->maskUV_mlu);
    //   }
    delete param;
  }
}

union _bit32_u {
  float f;
  uint32_t i;
};

static uint16_t float2half(const float f) {
  // assert((f > HALF_MIN) && (f < HALF_MAX));
  // assert((f == 0) || (f > HALF_PRECISION) || (f < -HALF_PRECISION));
  _bit32_u u;
  u.f = f;
  unsigned int bytes = u.i;
  unsigned char sign = (bytes >> 31) & 0x00000001;
  unsigned char exp = (bytes >> 23) & 0x000000FF;
  unsigned int eff = ((bytes >> 13) & 0x000003FF);  // + ((bytes >> 12) & 0x00000001);

  if (exp == 0xFF) {
    // inf or nan
    exp = 0x1F;
    if (eff) {
      // nan        -NaN     +NaN
      return sign ? 0xFFFF : 0x7FFF;
    } else {
      // inf        -inf     +inf
      return sign ? 0xFC00 : 0x7C00;
    }
  } else if (exp == 0x00) {
    // zero or denormal
    if (eff) {
      // denormal
      return sign ? 0x8000 : 0x0000;
    } else {
      return sign ? 0x8000 : 0x0000;
    }
  } else if (exp - 0x7F >= 0x1F - 0x0F) {
    // +/- inf
    // inf        -inf     +inf
    return sign ? 0xFC00 : 0x7C00;
  } else if (exp - 0x7F <= 0x00 - 0x0F) {
    // denormal
    int shift = (0x7F - exp - 0x0E);
    shift = shift > 11 ? 11 : shift;
    return ((sign << 15) | ((0x0400 | eff) >> shift));
  } else {
    // normal number
    exp = ((exp - 0x7F) + 0x0F) & 0x1F;
    return (sign << 15) | (exp << 10) | eff;
  }
}

#define CHECK_CNRT_RET(cnrt_ret, _estr, msg, code, ret_value) \
  do {                                                        \
    if (cnrt_ret != CNRT_RET_SUCCESS) {                       \
      if (_estr) {                                            \
        *_estr = msg;                                         \
      }                                                       \
      { code }                                                \
      return ret_value;                                       \
    }                                                         \
  } while (0)

bool PrepareKernelParam(int d_row, int d_col, int color_mode, int data_type,
  int batchsize, bool keep_aspect_ratio, KernelParam** param, int dev_type, int padMethod, string* estr) {
  const int CI = 64;
  const int CO = 256;
  const int LT_NUM = 64;

  *param = new KernelParam;
  // parse mode
  int inputType, outputType;

  switch (color_mode) {
    case YUV_TO_RGBA_NV12:
      inputType = YUVNV12;
      outputType = RGBA;
      break;
    case YUV_TO_RGBA_NV21:
      inputType = YUVNV21;
      outputType = RGBA;
      break;
    case YUV_TO_BGRA_NV12:
      inputType = YUVNV12;
      outputType = BGRA;
      break;
    case YUV_TO_BGRA_NV21:
      inputType = YUVNV21;
      outputType = BGRA;
      break;
    case YUV_TO_ARGB_NV12:
      inputType = YUVNV12;
      outputType = ARGB;
      break;
    case YUV_TO_ARGB_NV21:
      inputType = YUVNV21;
      outputType = ARGB;
      break;
    case YUV_TO_ABGR_NV12:
      inputType = YUVNV12;
      outputType = ABGR;
      break;
    case YUV_TO_ABGR_NV21:
      inputType = YUVNV21;
      outputType = ABGR;
      break;
    case RGBA_TO_RGBA:
      inputType = RGBA;
      outputType = RGBA;
      break;
    default:
      std::cout << "COLOR CONVERSION NOT SURPPORTED!" << std::endl;
      assert(0);
      return false;
  }

  // parse inputType
  // int channelIn = 1;   // ch in NCHW mode
  // int channelOut = 1;  // ch in NCHW mode
  int layerIn = 1;     // ch in NHWC mode
  // int layerOut = 1;    // ch in HHWC mode
  int reverseChannel = 0;
  int input2half = 0;
  int output2uint = 0;

  if (inputType == YUVNV21) {
    inputType = YUVNV12;
    reverseChannel = true;
  }

  switch (inputType) {
    case RGB:
      // channelIn = 3;
      break;
    case RGBA:
      // channelIn = 4;
      break;
    case GRAY:
      // channelIn = 1;
      break;
    case YUVNV12:
      // channelIn = 1;
      layerIn = 3;
      break;
    default:
      std::cout << "INPUT COLOR_TYPE NOT SURPPORTED!" << std::endl;
      assert(0);
      return false;
  }

  // // parse outputType
  // switch (outputType) {
  //   case RGB:
  //   case BGR:
  //     channelOut = 3;
  //     break;
  //   case RGBA:
  //   case BGRA:
  //   case ARGB:
  //   case ABGR:
  //     channelOut = 4;
  //     break;
  //   case GRAY:
  //     channelOut = 1;
  //     break;
  //   default:
  //     std::cout << "OUTPUT COLOR_TYPE NOT SURPPORTED!" << std::endl;
  //     assert(0);
  //     return false;
  // }

  // input2half = 1 when in_datatype = uint8
  input2half = 1 - sizeof(IN_DATA_TYPE) / 2;
  // output2uint = 1 when out_datatype = uint8
  output2uint = 1 - sizeof(OUT_DATA_TYPE) / 2;
  half* consts = (half*)malloc((2 * CI * CO + CO) * sizeof(int16_t));
  // int ratio = 150;
  // int total = ratio * (ratio + 1) / 2;
  // half* maskTP = (half*)malloc((CI * CI * total) * sizeof(int16_t));
  // half* maskUV = (half*)malloc((CI * CI * total) * sizeof(int16_t));
  // half temp[CI * CI];
  // for (int i = 0; i < CI; i++) {
  //  for (int j = 0; j < CI; j++) {
  //    temp[j + CI * i] = 0;
  //  }
  //}
  // for (int i = 0; i < CI; i++) {
  //  temp[i * CI + i] = 1;
  //}
  // for (int i = 0; i < CI * total; i++) {
  //  for (int j = 0; j < CI; j++) {
  //    maskUV[j + CI * i] = 0;
  //    maskTP[j + CI * i] = 0;
  //  }
  //}

  // int multOffset = 0;
  // int tSize = CI * 4;
  // for (int mult = 1; mult <= ratio; mult++) {
  //  multOffset += CI * CI * (mult - 1);
  //  for (int i = 0; i < CI / 4; i++) {
  //    for (int j = 0; j < mult; j++) {
  //      memcpy(maskTP + multOffset + tSize * (i * mult + j),
  //               temp + tSize * i,
  //               tSize * sizeof(int16_t));
  //    }
  //  }

  //  int kernelNum = CI * mult / LT_NUM;
  //  int ltSize = CI * mult / LT_NUM * CI;
  //  for (int lt = 0; lt < LT_NUM; lt++) {
  //    for (int kernel = 0; kernel < kernelNum; kernel++) {
  //      memcpy(maskUV + multOffset + lt * ltSize + kernel * CI,
  //             maskTP + multOffset + kernel * LT_NUM * CI + lt * CI,
  //             CI * sizeof(int16_t));
  //    }
  //  }
  //}
  // for (int i = 0; i < 64; i++) {
  //  for (int j = 0; j < 64; j++) {
  //    maskUV[j + 64 * i] = 0;
  //  }
  //}
  // for (int i = 0; i < 64; i++) {
  //  maskUV[i * 64 + i] = 1;
  //}
  // multOffset = 0;
  // for (int mult = 1; mult <= 10; mult++) {
  //  multOffset += CI * CI * (mult - 1);
  //  std::cout << "mult: " << mult << " " << multOffset << std::endl;
  //  for (int i = 0; i < CI * mult; i++) {
  //    for (int j = 0; j < CI; j++) {
  //      std::cout << maskUV[multOffset + i * CI + j] << " ";
  //    }
  //    std::cout << std::endl;
  //  }
  //  std::cout << std::endl;
  //  std::cout << std::endl;
  //}

  // prepare const(weights and bias)
  if (layerIn > 1) {
    int kernelLen = 2 * CI;
    for (int i = 0; i < 2 * CI * CO + CO; i++) {
      consts[i] = 0;
    }
    for (int lt = 0; lt < LT_NUM; lt++) {
      for (int idx = 0; idx < CO / LT_NUM; idx++) {
        int offsetY = (lt * CO / LT_NUM + idx) * kernelLen + (idx * LT_NUM + lt) / 4;

        int offsetU, offsetV;
        if (!reverseChannel) {
          offsetU = offsetY + CI - ((lt / 4) % 2);
          offsetV = offsetU + 1;
        } else {
          offsetV = offsetY + CI - ((lt / 4) % 2);
          offsetU = offsetV + 1;
        }

        // distribute contents of YUV terms
        int rIdx, gIdx, bIdx, zIdx;
        if (outputType == RGBA) {
          rIdx = 0;
          gIdx = 1;
          bIdx = 2;
          zIdx = 3;
        } else if (outputType == BGRA) {
          rIdx = 2;
          gIdx = 1;
          bIdx = 0;
          zIdx = 3;
        } else if (outputType == ARGB) {
          rIdx = 1;
          gIdx = 2;
          bIdx = 3;
          zIdx = 0;
        } else {
          rIdx = 3;
          gIdx = 2;
          bIdx = 1;
          zIdx = 0;
        }
        consts[idx * LT_NUM + lt + 2 * CI * CO] = float2half(  // bias
            -222.912 * ((lt % 4) == rIdx) + 135.616 * ((lt % 4) == gIdx) + -276.800 * ((lt % 4) == bIdx));
        // Y
        ((int16_t*)consts)[offsetY] = ((lt % 4) != zIdx) * 0x253F;

        // U
        ((int16_t*)consts)[offsetU] = ((lt % 4) == gIdx) * (0xF375)   // G
                                      + ((lt % 4) == bIdx) * 0x408B;  // B
        // V
        ((int16_t*)consts)[offsetV] = ((lt % 4) == rIdx) * 0x3312       // R
                                      + ((lt % 4) == gIdx) * (0xE5FC);  // G
      }
    }
  }
  //  std::cout << "channelIn: " << channelIn << std::endl;
  //  std::cout << "channelOut: " << channelOut << std::endl;
  //  std::cout << "layerIn: " << layerIn << std::endl;
  //  std::cout << "layerOut: " << layerOut << std::endl;
  //  std::cout << std::endl;
  // half* maskUV_mlu;

  // malloc and copy consts_mlu
  int ecode = cnrtMalloc((void**)&((*param)->consts_mlu), (2 * CI * CO + CO) * sizeof(half));
  CHECK_CNRT_RET(ecode, estr, "Malloc consts FAILED! ERRCODE:" + to_string(ecode),
                 { FreeKernelParam(*param); free(consts); },
                 false);

  ecode = cnrtMemcpy((*param)->consts_mlu, reinterpret_cast<void*>(consts), (2 * CI * CO + CO) * sizeof(half),
                     CNRT_MEM_TRANS_DIR_HOST2DEV);
  CHECK_CNRT_RET(ecode, estr, "H2D consts FAILED! ERRCODE:" + to_string(ecode),
                 { FreeKernelParam(*param); free(consts); },
                 false);
  free(consts);

  ecode = cnrtMalloc(reinterpret_cast<void**>(&((*param)->fill_color)), sizeof(uint8_t) * 4);
  CHECK_CNRT_RET(ecode, estr, "cnrtm malloc FAILED! ERRCODE:" + std::to_string(ecode), {FreeKernelParam(*param); }, false);
  cnrtMemset((*param)->fill_color, 0, sizeof(uint8_t) * 4);
  // // malloc and copy maskUV_mlu
  // if (CNRT_RET_SUCCESS !=
  //   cnrtMalloc((void**)&maskUV_mlu, CI * CI * total * sizeof(half))) {
  //   printf("cnrtMalloc FAILED!\n");
  //   exit(-1);
  // }
  // if (CNRT_RET_SUCCESS != cnrtMemcpy(maskUV_mlu, (half*)maskUV,
  //                                   CI * CI * total * sizeof(half),
  //                                    CNRT_MEM_TRANS_DIR_HOST2DEV)) {
  //   printf("cnrtMemcpy FAILED!\n");
  //   exit(-1);
  // }
  
  if (1 == dev_type) {
    (*param)->kernel_func = reinterpret_cast<void*>(&ResizeYuvToRgbaKernel_V2_MLU220);
  } else if (2 == dev_type) {
    (*param)->kernel_func = reinterpret_cast<void*>(&ResizeYuvToRgbaKernel_V2_MLU270);
  } else {
    (*param)->kernel_func = reinterpret_cast<void*>(&ResizeYuvToRgbaKernel_V2_MLU270);
  }
  cnrtCreateKernelInitParam(&(*param)->init_param);
  cnrtInitKernelMemory((*param)->kernel_func, (*param)->init_param);

  // params.
  (*param)->yuvFilter = (*param)->consts_mlu;
  (*param)->yuvBias = (*param)->consts_mlu + 2 * CI * CO;
  (*param)->d_row = d_row;
  (*param)->d_col = d_col;
  (*param)->input2half = input2half;
  (*param)->output2uint = output2uint;
  (*param)->batchNum = batchsize;
  (*param)->keep_aspect_ratio = keep_aspect_ratio ? 1 : 0;
  (*param)->padMethod = padMethod;
  return true;
}

float ResizeAndConvert(void* dst, void** y_plane_addrs, void** uv_plane_addrs,
                       int **src_whs, int** src_rois,
                       KernelParam* kparam, cnrtFunctionType_t func_type,
                       cnrtDim3_t dim, cnrtQueue_t queue, int dev_type,
                       string* estr) {
  cnrtKernelParamsBuffer_t params;
  cnrtGetKernelParamsBuffer(&params);
  cnrtKernelParamsBufferAddParam(params, &dst, sizeof(half*));
  cnrtKernelParamsBufferAddParam(params, reinterpret_cast<void*>(&y_plane_addrs), sizeof(half*));
  cnrtKernelParamsBufferAddParam(params, reinterpret_cast<void*>(&uv_plane_addrs), sizeof(half*));
  cnrtKernelParamsBufferAddParam(params, reinterpret_cast<void*>(&src_whs), sizeof(int**));
  cnrtKernelParamsBufferAddParam(params, reinterpret_cast<void*>(&src_rois), sizeof(int**));
  cnrtKernelParamsBufferAddParam(params, &kparam->fill_color, sizeof(half*));
  cnrtKernelParamsBufferAddParam(params, &kparam->yuvFilter, sizeof(half*));
  cnrtKernelParamsBufferAddParam(params, &kparam->yuvBias, sizeof(half*));
  // cnrtKernelParamsBufferAddParam(params, &kparam->maskUV_mlu, sizeof(half *));
  cnrtKernelParamsBufferAddParam(params, &kparam->d_row, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->d_col, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->input2half, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->output2uint, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->batchNum, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->keep_aspect_ratio, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->padMethod, sizeof(int));

  int ecode;

  ecode = cnrtInvokeKernel_V3(kparam->kernel_func, kparam->init_param, dim, params, func_type, queue, NULL);

  CHECK_CNRT_RET(ecode, estr, "[ResizeAndConvert] cnrtInvokeKernel FAILED. ERRCODE:" + to_string(ecode),
                 { cnrtDestroyKernelParamsBuffer(params); }, -1);

  float _time = 0;

  // free resources
  //  if (CNRT_RET_SUCCESS != cnrtFree(consts_mlu)) {
  //    printf("%s:%d cnrtFree FAILED!\n, __FILE__, __LINE__");
  //    exit(-1);
  //  }
  //
  //  if (CNRT_RET_SUCCESS != cnrtFree(maskUV_mlu)) {
  //    printf("%s:%d cnrtFree FAILED!\n, __FILE__, __LINE__");
  //    exit(-1);
  //  }

  ecode = cnrtDestroyKernelParamsBuffer(params);
  CHECK_CNRT_RET(ecode, estr, "[ResizeAndConvert] cnrtDestroyKernelParamsBuffer FAILED. ERRCODE:" + to_string(ecode),
                 {}, -1);
  return _time;
}

