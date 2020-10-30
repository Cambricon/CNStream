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
#include <string.h>
#include <sys/time.h>
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

#include "resize_yuv2rgba_kernel.h"
#include "resize_yuv2rgba_macro.h"

using std::string;
using std::to_string;

void getResizedConvertWorkspaceSize(int* roi_rect_cpu_ptr,
                                    int dst_row,
                                    int dst_col,
                                    int batch_num,
                                    int keep_aspect_ratio,
                                    size_t* workspace_size){
  size_t temp_size = 0;

  // mult space
  temp_size += batch_num * sizeof(int);

  // mask pointer space
  temp_size += batch_num * 2 * sizeof(half*);

  // weight pointer space
  temp_size += batch_num * 2 * sizeof(half*);

  // copy filter pointer space
  temp_size += batch_num * sizeof(int8_t*);

  float dst_aspect_ratio = float(dst_col) / dst_row;

  int mult_list[batch_num];
  int src_roi_x_list[batch_num];
  int src_roi_w_list[batch_num];
  int dst_roi_w_list[batch_num];

  for(int batch_iter = 0; batch_iter < batch_num; batch_iter++) {
    int cur_roi_x = roi_rect_cpu_ptr[batch_iter * 4 + 0];
    int cur_roi_w = roi_rect_cpu_ptr[batch_iter * 4 + 2];
    int cur_roi_h = roi_rect_cpu_ptr[batch_iter * 4 + 3];

    int cur_roi_w_ = (cur_roi_x % 2 + cur_roi_w + 1) / 2 * 2;

    int dst_roi_w = dst_col;

    float src_aspect_ratio = float(cur_roi_w) / cur_roi_h;
    if (keep_aspect_ratio && (src_aspect_ratio != dst_aspect_ratio)) {
      if (src_aspect_ratio <= dst_aspect_ratio) {
        dst_roi_w = std::round(dst_row * src_aspect_ratio);
      }
    }

#ifdef ZERO_COORDINATE
    int mult = (int)(cur_roi_w < dst_roi_w) * ceil(dst_roi_w / cur_roi_w)
                + (int)(cur_roi_w >= dst_roi_w);
#else
    int mult = (int)(cur_roi_w < dst_roi_w) * (ceil(1.5 * dst_roi_w / cur_roi_w + 0.5) - 1)
                + (int)(cur_roi_w >= dst_roi_w);
#endif

    mult_list[batch_iter] = mult;
    src_roi_x_list[batch_iter] = cur_roi_x;
    src_roi_w_list[batch_iter] = cur_roi_w;
    dst_roi_w_list[batch_iter] = dst_roi_w;

    size_t mask_size = 2 * mult * cur_roi_w_ * 4 * sizeof(half);
    size_t weight_size = 2 * dst_roi_w * 4 * sizeof(half);

    for(int prev_batch = 0; prev_batch < batch_iter; prev_batch++) {
      if ((cur_roi_x == src_roi_x_list[prev_batch]) &&
          (cur_roi_w == src_roi_w_list[prev_batch]) &&
          (dst_roi_w == dst_roi_w_list[prev_batch])) {
        mask_size = 0;
        weight_size = 0;
        break;
      }
    }

    size_t copy_filter_size = 0;
    if ((mult >= 0) && (mult < MULT_LIMIT)) {
      copy_filter_size = LT_NUM * mult * LT_NUM * sizeof(int8_t);

      for(int prev_batch = 0; prev_batch < batch_iter; prev_batch++) {
        if (mult == mult_list[prev_batch]) {
          copy_filter_size = 0;
          break;
        }
      }
    }

    temp_size += mask_size + weight_size + copy_filter_size;
  }

  *workspace_size = temp_size;
}

void prepareMaskAndWeights(void* cpu_workspace,
                           void* workspace,
                           int* roi_rect_cpu_ptr,
                           int dst_row,
                           int dst_col,
                           int batch_num,
                           int keep_aspect_ratio) {
  // cpu pointer offset
  int* mult_cpu_ptr = (int*)(cpu_workspace);
  half** mask_pointer_cpu_ptr = (half**)(mult_cpu_ptr + batch_num);
  half** weight_pointer_cpu_ptr = (half**)(mask_pointer_cpu_ptr + batch_num * 2);
  int8_t** copy_filter_pointer_cpu_ptr = (int8_t**)(weight_pointer_cpu_ptr + batch_num * 2);
  void* cur_cpu_ptr = (void*)(copy_filter_pointer_cpu_ptr + batch_num);

  // mlu gdram pointer offset
  int* mult_mlu_ptr = (int*)(workspace);
  half** mask_pointer_mlu_ptr = (half**)(mult_mlu_ptr + batch_num);
  half** weight_pointer_mlu_ptr = (half**)(mask_pointer_mlu_ptr + batch_num * 2);
  int8_t** copy_filter_pointer_mlu_ptr = (int8_t**)(weight_pointer_mlu_ptr + batch_num * 2);
  void* cur_mlu_ptr = (void*)(copy_filter_pointer_mlu_ptr + batch_num);

  // batch loop
  float dst_aspect_ratio = float(dst_col) / dst_row;

  int src_roi_x_list[batch_num];
  int src_roi_w_list[batch_num];
  int dst_roi_w_list[batch_num];

  for(int batch_iter = 0; batch_iter < batch_num; batch_iter++) {
    // compute dst roi size
    int cur_roi_x = roi_rect_cpu_ptr[batch_iter * 4 + 0];
    int cur_roi_w = roi_rect_cpu_ptr[batch_iter * 4 + 2];
    int cur_roi_h = roi_rect_cpu_ptr[batch_iter * 4 + 3];

    int cur_roi_w_ = (cur_roi_x % 2 + cur_roi_w + 1) / 2 * 2;

    int dst_roi_w = dst_col;

    float src_aspect_ratio = float(cur_roi_w) / cur_roi_h;
    if (keep_aspect_ratio && (src_aspect_ratio != dst_aspect_ratio)) {
      if (src_aspect_ratio <= dst_aspect_ratio) {
        dst_roi_w = std::round(dst_row * src_aspect_ratio);
      }
    }

    // compute w scale
    float cur_scale_w = float(cur_roi_w) / dst_roi_w;

    // compute mult
#ifdef ZERO_COORDINATE
    int mult = (int)(cur_roi_w < dst_roi_w) * ceil(dst_roi_w / cur_roi_w)
                + (int)(cur_roi_w >= dst_roi_w);
#else
    int mult = (int)(cur_roi_w < dst_roi_w) * (ceil(1.5 * dst_roi_w / cur_roi_w + 0.5) - 1)
                + (int)(cur_roi_w >= dst_roi_w);
#endif

    // set mult to cpu addr
    mult_cpu_ptr[batch_iter] = mult;

    // reuse computed mask and weight or compute current mask and weight
    src_roi_x_list[batch_iter] = cur_roi_x;
    src_roi_w_list[batch_iter] = cur_roi_w;
    dst_roi_w_list[batch_iter] = dst_roi_w;

    bool repeated = false;

    for(int prev_batch = 0; prev_batch < batch_iter; prev_batch++) {
      if (((cur_roi_x % 2) == (src_roi_x_list[prev_batch] % 2)) &&
          (cur_roi_w == src_roi_w_list[prev_batch]) &&
          (dst_roi_w == dst_roi_w_list[prev_batch])) {
        mask_pointer_cpu_ptr[batch_iter * 2] =
          mask_pointer_cpu_ptr[prev_batch * 2];
        mask_pointer_cpu_ptr[batch_iter * 2 + 1] =
          mask_pointer_cpu_ptr[prev_batch * 2 + 1];

        weight_pointer_cpu_ptr[batch_iter * 2] =
          weight_pointer_cpu_ptr[prev_batch * 2];
        weight_pointer_cpu_ptr[batch_iter * 2 + 1] =
          weight_pointer_cpu_ptr[prev_batch * 2 + 1];

        repeated = true;
        break;
      }
    }

    if (!repeated) {
      // cpu pointer offset
      half* cur_mask_left_cpu_ptr = (half*)(cur_cpu_ptr);
      half* cur_mask_right_cpu_ptr = (half*)(cur_mask_left_cpu_ptr + mult * cur_roi_w_ * 4);

      half* cur_weight_left_cpu_ptr = (half*)(cur_mask_right_cpu_ptr + mult * cur_roi_w_ * 4);
      half* cur_weight_right_cpu_ptr = (half*)(cur_weight_left_cpu_ptr + dst_roi_w * 4);

      cur_cpu_ptr = (half*)(cur_weight_right_cpu_ptr + dst_roi_w * 4);

      // mlu pointer offset
      half* cur_mask_left_mlu_ptr = (half*)(cur_mlu_ptr);
      half* cur_mask_right_mlu_ptr = (half*)(cur_mask_left_mlu_ptr + mult * cur_roi_w_ * 4);

      half* cur_weight_left_mlu_ptr = (half*)(cur_mask_right_mlu_ptr + mult * cur_roi_w_ * 4);
      half* cur_weight_right_mlu_ptr = (half*)(cur_weight_left_mlu_ptr + dst_roi_w * 4);

      cur_mlu_ptr = (half*)(cur_weight_right_mlu_ptr + dst_roi_w * 4);

#ifdef ZERO_COORDINATE
      float src_w_iter_base = 0.0f;
#else
      float src_w_iter_base = 0.5 * cur_scale_w - 0.5;
#endif
      float src_w_iter = src_w_iter_base;
      int src_w_iter_int = -1;
      int src_w_iter_int_prev = -1;

      float left_weight = 0.0;
      float right_weight = 0.0;

      int mask_left_index = 0;
      int mask_right_index = 0;

      for (int dst_w_iter = 0; dst_w_iter < dst_roi_w; dst_w_iter++) {
        src_w_iter = dst_w_iter * cur_scale_w + src_w_iter_base;
        src_w_iter = src_w_iter < 0 ? 0 : src_w_iter;
        src_w_iter = src_w_iter > (cur_roi_w - 1) ? cur_roi_w - 1 : src_w_iter;
        src_w_iter_int = floor(src_w_iter);

        // compute mask data
        if (src_w_iter_int == src_w_iter_int_prev) {
          mask_left_index++;
        } else {
          mask_left_index = (src_w_iter_int + cur_roi_x % 2) * mult;
        }
        mask_right_index = mask_left_index + mult;

        cur_mask_left_cpu_ptr[mask_left_index * 4] = 1;
        cur_mask_left_cpu_ptr[mask_left_index * 4 + 1] = 1;
        cur_mask_left_cpu_ptr[mask_left_index * 4 + 2] = 1;
        cur_mask_left_cpu_ptr[mask_left_index * 4 + 3] = 1;

        if (mask_right_index < (cur_roi_w + cur_roi_x % 2) * mult) {
          cur_mask_right_cpu_ptr[mask_right_index * 4] = 1;
          cur_mask_right_cpu_ptr[mask_right_index * 4 + 1] = 1;
          cur_mask_right_cpu_ptr[mask_right_index * 4 + 2] = 1;
          cur_mask_right_cpu_ptr[mask_right_index * 4 + 3] = 1;
        }

        // compute weight data
        right_weight = src_w_iter - src_w_iter_int;
        left_weight = 1.0 - right_weight;

        cur_weight_left_cpu_ptr[dst_w_iter * 4] = left_weight;
        cur_weight_left_cpu_ptr[dst_w_iter * 4 + 1] = left_weight;
        cur_weight_left_cpu_ptr[dst_w_iter * 4 + 2] = left_weight;
        cur_weight_left_cpu_ptr[dst_w_iter * 4 + 3] = left_weight;

        cur_weight_right_cpu_ptr[dst_w_iter * 4] = right_weight;
        cur_weight_right_cpu_ptr[dst_w_iter * 4 + 1] = right_weight;
        cur_weight_right_cpu_ptr[dst_w_iter * 4 + 2] = right_weight;
        cur_weight_right_cpu_ptr[dst_w_iter * 4 + 3] = right_weight;

        // update data for next iter
        src_w_iter_int_prev = src_w_iter_int;
      }

      // set mlu pointer addr
      mask_pointer_cpu_ptr[batch_iter * 2] = cur_mask_left_mlu_ptr;
      mask_pointer_cpu_ptr[batch_iter * 2 + 1] = cur_mask_right_mlu_ptr;

      weight_pointer_cpu_ptr[batch_iter * 2] = cur_weight_left_mlu_ptr;
      weight_pointer_cpu_ptr[batch_iter * 2 + 1] = cur_weight_right_mlu_ptr;
    }

    // reuse computed copy filter or compute current copy filter
    if ((mult >= 0) && (mult < MULT_LIMIT)) {
      repeated = false;

      for(int prev_batch = 0; prev_batch < batch_iter; prev_batch++) {
        if (mult == mult_cpu_ptr[prev_batch]) {
          copy_filter_pointer_cpu_ptr[batch_iter] =
            copy_filter_pointer_cpu_ptr[prev_batch];

          repeated = true;
          break;
        }
      }

      if (!repeated) {
        // cpu pointer offset
        int8_t* cur_copy_filter_cpu_ptr = (int8_t*)(cur_cpu_ptr);

        cur_cpu_ptr = (half*)(cur_copy_filter_cpu_ptr + LT_NUM * mult * LT_NUM);

        // mlu pointer offset
        int8_t* cur_copy_filter_mlu_ptr = (int8_t*)(cur_mlu_ptr);

        cur_mlu_ptr = (half*)(cur_copy_filter_mlu_ptr + LT_NUM * mult * LT_NUM);

        // lt data
        for (int lt_i = 0; lt_i < LT_NUM; lt_i++) {
          for (int idx_i = 0; idx_i < (LT_NUM * mult / LT_NUM); idx_i++) {
            int origin_idx = lt_i + idx_i * LT_NUM;
            int real_idx = lt_i * (LT_NUM * mult / LT_NUM) + idx_i;

            int data_offset = origin_idx / (mult * 4) * 4 + origin_idx % 4;

            cur_copy_filter_cpu_ptr[real_idx * LT_NUM + data_offset] = 1;
          }
        }

        copy_filter_pointer_cpu_ptr[batch_iter] = cur_copy_filter_mlu_ptr;
      }
    }
  }
}

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
  half* consts = (half*)malloc((2 * CI * CO + CO) * sizeof(half));
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
        consts[idx * LT_NUM + lt + 2 * CI * CO] = half(  // bias
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
  
  (*param)->kernel_func = reinterpret_cast<void*>(&ResizeYuvToRgbaKernel);
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
                       int **src_whs, int** src_rois_mlu, int* src_rois_cpu,
                       KernelParam* kparam, cnrtFunctionType_t func_type,
                       cnrtDim3_t dim, cnrtQueue_t queue, int dev_type,
                       string* estr) {

  // malloc device space for workspace
  size_t workspace_size;

  getResizedConvertWorkspaceSize(src_rois_cpu,
                                 kparam->d_row,
                                 kparam->d_col,
                                 kparam->batchNum,
                                 kparam->keep_aspect_ratio,
                                 &workspace_size);

  void* cpu_workspace = (void*)malloc(workspace_size);
  memset(cpu_workspace, 0, workspace_size);

  void* workspace;

  int ecode = cnrtMalloc((void**)&workspace, workspace_size);
  CHECK_CNRT_RET(ecode, estr,
                 "[ResizeAndConvert] cnrtMalloc workspace FAILED. ERRCODE:" + to_string(ecode),
                 { free(cpu_workspace); }, -1);

  prepareMaskAndWeights(cpu_workspace, workspace,
                        src_rois_cpu, kparam->d_row, kparam->d_col,
                        kparam->batchNum, kparam->keep_aspect_ratio);

  ecode = cnrtMemcpy(workspace, cpu_workspace, workspace_size,
                     CNRT_MEM_TRANS_DIR_HOST2DEV);
  CHECK_CNRT_RET(ecode, estr,
                 "[ResizeAndConvert] cnrtMemcpy workspace FAILED. ERRCODE:" + to_string(ecode),
                 { free(cpu_workspace); cnrtFree(workspace); }, -1);

  int* mult_mlu_ptr = (int*)(workspace);
  half** mask_pointer_mlu_ptr = (half**)(mult_mlu_ptr + kparam->batchNum);
  half** weight_pointer_mlu_ptr = (half**)(mask_pointer_mlu_ptr + kparam->batchNum * 2);
  int8_t** copy_filter_pointer_mlu_ptr = (int8_t**)(weight_pointer_mlu_ptr + kparam->batchNum * 2);

  cnrtKernelParamsBuffer_t params;
  cnrtGetKernelParamsBuffer(&params);
  cnrtKernelParamsBufferAddParam(params, &dst, sizeof(half*));
  cnrtKernelParamsBufferAddParam(params, reinterpret_cast<void*>(&y_plane_addrs), sizeof(half*));
  cnrtKernelParamsBufferAddParam(params, reinterpret_cast<void*>(&uv_plane_addrs), sizeof(half*));
  cnrtKernelParamsBufferAddParam(params, reinterpret_cast<void*>(&src_whs), sizeof(int**));
  cnrtKernelParamsBufferAddParam(params, reinterpret_cast<void*>(&src_rois_mlu), sizeof(int**));
  cnrtKernelParamsBufferAddParam(params, &kparam->fill_color, sizeof(half*));
  cnrtKernelParamsBufferAddParam(params, &kparam->yuvFilter, sizeof(half*));
  cnrtKernelParamsBufferAddParam(params, &kparam->yuvBias, sizeof(half*));
  cnrtKernelParamsBufferAddParam(params, &mult_mlu_ptr, sizeof(int*));
  cnrtKernelParamsBufferAddParam(params, &mask_pointer_mlu_ptr, sizeof(half**));
  cnrtKernelParamsBufferAddParam(params, &weight_pointer_mlu_ptr, sizeof(half**));
  cnrtKernelParamsBufferAddParam(params, &copy_filter_pointer_mlu_ptr, sizeof(int8_t**));
  // cnrtKernelParamsBufferAddParam(params, &kparam->maskUV_mlu, sizeof(half *));
  cnrtKernelParamsBufferAddParam(params, &kparam->d_row, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->d_col, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->input2half, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->output2uint, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->batchNum, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->keep_aspect_ratio, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->padMethod, sizeof(int));

  ecode = cnrtInvokeKernel_V3(kparam->kernel_func, kparam->init_param, dim, params, func_type, queue, NULL);

  CHECK_CNRT_RET(ecode, estr,
                 "[ResizeAndConvert] cnrtInvokeKernel FAILED. ERRCODE:" + to_string(ecode),
                 { free(cpu_workspace); cnrtFree(workspace); cnrtDestroyKernelParamsBuffer(params); }, -1);

  free(cpu_workspace);
  cnrtFree(workspace);
  ecode = cnrtDestroyKernelParamsBuffer(params);
  CHECK_CNRT_RET(ecode, estr, "[ResizeAndConvert] cnrtDestroyKernelParamsBuffer FAILED. ERRCODE:" + to_string(ecode),
                 {}, -1);
  return 0;
}

