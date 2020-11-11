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

#include <glog/logging.h>
#include <string>

#include "cnrt.h"
#include "resize_kernel.h"

using std::string;
using std::to_string;

struct ResizeKernelParam {
  uint32_t s_row, s_col, d_row, d_col;
  uint32_t s_stride_y, s_stride_uv;
  uint32_t batch;
  cnrtKernelInitParam_t init_param = nullptr;
  uint32_t affinity;
};

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

int PrepareKernelParam(uint32_t s_row, uint32_t s_col, uint32_t src_stride_y, uint32_t src_stride_uv, uint32_t d_row,
                       uint32_t d_col, uint32_t batch, uint32_t channel_id, ResizeKernelParam** param, string* estr) {
  *param = new ResizeKernelParam;
  (*param)->s_row = s_row;
  (*param)->s_col = s_col;
  (*param)->s_stride_y = src_stride_y ? src_stride_y : s_col;
  (*param)->s_stride_uv = src_stride_uv ? src_stride_uv : s_col;
  (*param)->d_row = d_row;
  (*param)->d_col = d_col;
  (*param)->batch = batch;

  if (channel_id % 2 == 0) {
    cnrtSetCurrentChannel(CNRT_CHANNEL_TYPE_0);
    (*param)->affinity = 0x01;
  } else {
    cnrtSetCurrentChannel(CNRT_CHANNEL_TYPE_1);
    (*param)->affinity = 0x02;
  }

  VLOG(3) << "resize param: src_row(" << s_row << ") src_col(" << s_col << ") dst_row(" << d_row << ") dst_col("
          << d_col << ") src_stride_y(" << (*param)->s_stride_y << ") src_stride_uv(" << (*param)->s_stride_uv
          << ") batch(" << batch << ") channel_id(" << channel_id << ")";

  cnrtRet_t ret = cnrtCreateKernelInitParam(&(*param)->init_param);
  CHECK_CNRT_RET(ret, estr, "create kernel init param failed, error code: " + to_string(ret), {}, -1);
  ret = cnrtInitKernelMemory(reinterpret_cast<void*>(&MLUUnion1KernelResizeYuv420sp), (*param)->init_param);
  CHECK_CNRT_RET(ret, estr, "init kernel memory failed, error_code: " + to_string(ret), {}, -1);

  return 0;
}

void FreeKernelParam(ResizeKernelParam* param) {
  if (param) {
    if (param->init_param) {
      cnrtDestroyKernelInitParamAndMemory(param->init_param);
    }
    delete param;
  }
}

float InvokeResizeKernel(char** dst_y, char** dst_uv, char** srcY, char** srcUV, ResizeKernelParam* kparam,
                         cnrtFunctionType_t func_type, cnrtDim3_t dim, cnrtQueue_t queue, string* estr) {
  cnrtKernelParamsBuffer_t params;
  cnrtGetKernelParamsBuffer(&params);
  cnrtKernelParamsBufferAddParam(params, &kparam->s_row, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->s_col, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->s_stride_y, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->s_stride_uv, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &srcY, sizeof(char**));
  cnrtKernelParamsBufferAddParam(params, &srcUV, sizeof(char**));
  cnrtKernelParamsBufferAddParam(params, &kparam->d_row, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &kparam->d_col, sizeof(int));
  cnrtKernelParamsBufferAddParam(params, &dst_y, sizeof(char*));
  cnrtKernelParamsBufferAddParam(params, &dst_uv, sizeof(char*));
  cnrtKernelParamsBufferAddParam(params, &kparam->batch, sizeof(int));

  int ecode;

  if (func_type == CNRT_FUNC_TYPE_UNION1) {
    cnrtInvokeParam_t invoke_param;
    invoke_param.invoke_param_type = CNRT_INVOKE_PARAM_TYPE_0;
    invoke_param.cluster_affinity.affinity = &kparam->affinity;
    ecode = cnrtInvokeKernel_V3(reinterpret_cast<void*>(&MLUUnion1KernelResizeYuv420sp), kparam->init_param, dim,
                                params, func_type, queue, reinterpret_cast<void*>(&invoke_param));
  } else {
    ecode = cnrtInvokeKernel_V3(reinterpret_cast<void*>(&MLUUnion1KernelResizeYuv420sp), kparam->init_param, dim,
                                params, func_type, queue, NULL);
  }
  CHECK_CNRT_RET(ecode, estr, "[Resize] cnrtInvokeKernel FAILED. ERRCODE:" + to_string(ecode),
                 { cnrtDestroyKernelParamsBuffer(params); }, -1);

  ecode = cnrtDestroyKernelParamsBuffer(params);
  CHECK_CNRT_RET(ecode, estr, "[Resize] cnrtDestroyKernelParamsBuffer FAILED. ERRCODE:" + to_string(ecode), {}, -1);

  return 0;
}

float Resize(void** dstY, void** dstUV, void** srcY, void** srcUV, ResizeKernelParam* param,
             cnrtFunctionType_t func_type, cnrtDim3_t dim, cnrtQueue_t queue, string* estr) {
  return InvokeResizeKernel(reinterpret_cast<char**>(dstY), reinterpret_cast<char**>(dstUV),
                            reinterpret_cast<char**>(srcY), reinterpret_cast<char**>(srcUV), param, func_type, dim,
                            queue, estr);
}
