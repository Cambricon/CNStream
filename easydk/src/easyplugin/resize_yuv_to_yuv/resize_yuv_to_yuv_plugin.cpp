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

#include <sys/time.h>
#include <chrono>
#include <ctime>
#include <iostream>
#include <string>
#include "cnml.h"
#include "cnplugin.h"
#include "cnrt.h"
#include "easyplugin/resize_yuv_to_yuv.h"

#define PRINT_TIME 0

using std::string;
using std::to_string;

struct ResizeYuv2Yuv {
  void** input_addrs = nullptr;
  void** output_addrs = nullptr;
  cnmlPluginResizeAndColorCvtParam_t param = nullptr;
  cnmlBaseOp_t op = nullptr;
  cnmlTensor_t* cnml_input_ptr = nullptr;
  cnmlTensor_t* cnml_output_ptr = nullptr;
  // enum
  int batch_size = 1;
  cnmlCoreVersion_t version = CNML_MLU270;
#if PRINT_TIME
  cnrtNotifier_t event_begin = nullptr;
  cnrtNotifier_t event_end = nullptr;
#endif
};

bool CreateParam(const edk::MluResizeAttr& attr, ResizeYuv2Yuv* yuv2yuv, string* estr) {
  if (!yuv2yuv) {
    *estr = "[CreateParam] yuv2yuv is nullptr";
    return false;
  }
  switch (attr.core_version) {
    case edk::CoreVersion::MLU220:
      yuv2yuv->version = CNML_MLU220;
      std::cout << "core version MLU220" <<std::endl;
      break;
    case edk::CoreVersion::MLU270:
      yuv2yuv->version = CNML_MLU270;
      std::cout << "core version MLU270" <<std::endl;
      break;
    default:
      std::cout << "unsurpported core version" << std::endl;
      *estr = "unsurpported core version";
      return false;
  }
  yuv2yuv->batch_size = attr.batch_size;
  ioParams mode;
  auto cnmlret = cnmlCreatePluginResizeYuvToYuvOpParam(&(yuv2yuv->param),
                                                       attr.src_h,
                                                       attr.src_w,
                                                       attr.dst_h,
                                                       attr.dst_w,
                                                       mode,
                                                       yuv2yuv->version);
  if (!edk::CnmlCheck(cnmlret, estr, "Create Plugin ResizeYuv2Yuv Op param failed.")) {
    return false;
  }
  return true;
}

void FreeTensorPtr(ResizeYuv2Yuv* yuv2yuv) {
  if (yuv2yuv) {
    if (yuv2yuv->cnml_input_ptr) {
      free(yuv2yuv->cnml_input_ptr);
    }
    if (yuv2yuv->cnml_output_ptr) {
      free(yuv2yuv->cnml_output_ptr);
    }
  }
}

void InitTensorPtr(ResizeYuv2Yuv* yuv2yuv) {
  if (yuv2yuv) {
    auto param = yuv2yuv->param;
    yuv2yuv->cnml_input_ptr = reinterpret_cast<cnmlTensor_t*>(malloc(sizeof(cnmlTensor_t) * param->input_num));
    yuv2yuv->cnml_output_ptr = reinterpret_cast<cnmlTensor_t*>(malloc(sizeof(cnmlTensor_t) * param->output_num));
  }
}

bool DestroyTensor(ResizeYuv2Yuv* yuv2yuv, string* estr) {
  if (!yuv2yuv) {
    *estr = "[DestroyTensor] yuv2yuv is nullptr";
    return false;
  }
  auto param = yuv2yuv->param;
  bool success = true;
  for (int i = 0; i < param->input_num; i++) {
    if (yuv2yuv->cnml_input_ptr[i]) {
      success = edk::CnmlCheck(cnmlDestroyTensor(&yuv2yuv->cnml_input_ptr[i]), estr, "Destroy input Tensor failed.");
    }
  }
  for (int i = 0; i < param->output_num; i++) {
    if (yuv2yuv->cnml_output_ptr[i]) {
      success = edk::CnmlCheck(cnmlDestroyTensor(&yuv2yuv->cnml_output_ptr[i]), estr, "Destroy output Tensor failed.");
    }
  }
  return success;
}

bool CreateTensor(ResizeYuv2Yuv* yuv2yuv, string* estr) {
  if (!yuv2yuv) {
    *estr = "[CreateTensor] yuv2yuv is nullptr";
    return false;
  }
  auto param = yuv2yuv->param;

  if (param->input_num != 2 || param->output_num != 2) {
    *estr = "Input number or output number is not 2. Input num: " + to_string(param->input_num) +
            " Output num: " + to_string(param->input_num);
    return false;
  }

  int shape[4] = {yuv2yuv->batch_size, 1, 1, 1};
  cnmlDataType_t dt = CNML_DATA_INT32;

  for (uint8_t i = 0; i < param->input_num; i++) {
    // input tensor
    auto cnmlret = cnmlCreateTensor_V2(&(yuv2yuv->cnml_input_ptr[i]), CNML_TENSOR);
    if (!edk::CnmlCheck(cnmlret, estr, "Create input tensor failed.")) return false;
    cnmlret = cnmlSetTensorShape(yuv2yuv->cnml_input_ptr[i], 4, shape);
    if (!edk::CnmlCheck(cnmlret, estr, "Set input tensor shape failed.")) return false;
    cnmlret = cnmlSetTensorDataType(yuv2yuv->cnml_input_ptr[i], dt);
    if (!edk::CnmlCheck(cnmlret, estr, "Set input tensor data type failed.")) return false;
    // output tensor
    cnmlret = cnmlCreateTensor_V2(&(yuv2yuv->cnml_output_ptr[i]), CNML_TENSOR);
    if (!edk::CnmlCheck(cnmlret, estr, "Create output tensor failed.")) return false;
    cnmlret = cnmlSetTensorShape(yuv2yuv->cnml_output_ptr[i], 4, shape);
    if (!edk::CnmlCheck(cnmlret, estr, "Set output tensor shape failed.")) return false;
    cnmlret = cnmlSetTensorDataType(yuv2yuv->cnml_output_ptr[i], dt);
    if (!edk::CnmlCheck(cnmlret, estr, "Set output tensor shape failed.")) return false;
  }

  return true;
}

bool CreateAndCompileOp(const int& core_limit, ResizeYuv2Yuv* yuv2yuv, string* estr) {
  if (!yuv2yuv) {
    *estr = "[CreateAndCompileOp] yuv2yuv is nullptr";
    return false;
  }
  auto param = yuv2yuv->param;
  InitTensorPtr(yuv2yuv);
  if (!CreateTensor(yuv2yuv, estr)) {
    return false;
  }
  auto cnmlret =
    cnmlCreatePluginResizeYuvToYuvOp(&(yuv2yuv->op), param, yuv2yuv->cnml_input_ptr, yuv2yuv->cnml_output_ptr);
  if (!edk::CnmlCheck(cnmlret, estr, "Create Plugin ResizeYuvToYuv Op failed.")) return false;

  cnmlret = cnmlCompileBaseOp(yuv2yuv->op, yuv2yuv->version, core_limit);
  if (!edk::CnmlCheck(cnmlret, estr, "Compile Plugin ResizeYuvToYuv Op failed.")) return false;

  return true;
}

void FreeIoAddrsPtr(ResizeYuv2Yuv* yuv2yuv) {
  if (yuv2yuv) {
    if (yuv2yuv->input_addrs) {
      free(yuv2yuv->input_addrs);
    }
    if (yuv2yuv->output_addrs) {
      free(yuv2yuv->output_addrs);
    }
  }
}

void InitIOAddrsPtr(ResizeYuv2Yuv* yuv2yuv) {
  if (yuv2yuv) {
    yuv2yuv->input_addrs = reinterpret_cast<void **>(malloc(sizeof(void*) * yuv2yuv->param->input_num));
    yuv2yuv->output_addrs = reinterpret_cast<void **>(malloc(sizeof(void*) * yuv2yuv->param->output_num));
  }
}

bool DestroyResizeYuv2Yuv(ResizeYuv2Yuv* yuv2yuv, string* estr) {
  bool success = true;
  if (yuv2yuv) {
#if PRINT_TIME
    if (yuv2yuv->event_begin) {
      success = edk::CnrtCheck(cnrtDestroyNotifier(&yuv2yuv->event_begin), estr, "Destroy event begin failed.");
    }
    if (yuv2yuv->event_end) {
      success = edk::CnrtCheck(cnrtDestroyNotifier(&yuv2yuv->event_end), estr, "Destroy event end failed.");
    }
#endif
    success = DestroyTensor(yuv2yuv, estr);
    FreeTensorPtr(yuv2yuv);

    FreeIoAddrsPtr(yuv2yuv);

    if (yuv2yuv->op) {
      success = edk::CnmlCheck(cnmlDestroyBaseOp(&(yuv2yuv->op)), estr, "Destroy resize yuv2yuv op failed.");
    }
    if (yuv2yuv->param) {
      success = edk::CnmlCheck(cnmlDestroyPluginResizeYuvToYuvOpParam(&yuv2yuv->param),
                          estr, "Destroy resize yuv2yuv param failed.");
    }
    delete yuv2yuv;
  }
  success = edk::CnmlCheck(cnmlExit(), estr, "Exit failed.");
  return success;
}

bool CreateResizeYuv2Yuv(const edk::MluResizeAttr& attr, ResizeYuv2Yuv** yuv2yuv_ptr, string* estr) {
  (*yuv2yuv_ptr) = new ResizeYuv2Yuv;

  if (!*yuv2yuv_ptr) {
    *estr = "Create ResizeYuv2Yuv pointer failed";
    return false;
  }

  if (!edk::CnmlCheck(cnmlInit(0), estr, "Init failed")) return false;

#if PRINT_TIME
  if (!edk::CnrtCheck(cnrtCreateNotifier(&(*yuv2yuv_ptr)->event_begin), estr, "create notifier event_begin failed.")) {
    return false;
  }
  if (!edk::CnrtCheck(cnrtCreateNotifier(&(*yuv2yuv_ptr)->event_end), estr, "create notifier event_end failed.")) {
    return false;
  }
#endif

  if (!CreateParam(attr, *yuv2yuv_ptr, estr)) return false;
  if (!CreateAndCompileOp(attr.core_number, *yuv2yuv_ptr, estr)) return false;

  InitIOAddrsPtr(*yuv2yuv_ptr);
  return true;
}

bool ComputeResizeYuv2Yuv(void* dst_y, void* dst_uv, void* src_y, void* src_uv, ResizeYuv2Yuv* yuv2yuv,
                          cnrtQueue_t queue, string* estr) {
  if (!yuv2yuv) {
    *estr = "[ComputeResizeYuv2Yuv] yuv2yuv is nullptr";
    return false;
  }
  yuv2yuv->input_addrs[0] = src_y;
  yuv2yuv->input_addrs[1] = src_uv;
  yuv2yuv->output_addrs[0] = dst_y;
  yuv2yuv->output_addrs[1] = dst_uv;
#if PRINT_TIME
  cnrtPlaceNotifier(yuv2yuv->event_begin, queue);
  auto start_tp = std::chrono::high_resolution_clock::now();
#endif
  auto cnmlret = cnmlComputePluginResizeYuvToYuvOpForward(yuv2yuv->op,
                                                          yuv2yuv->param,
                                                          yuv2yuv->cnml_input_ptr,
                                                          yuv2yuv->input_addrs,
                                                          yuv2yuv->cnml_output_ptr,
                                                          yuv2yuv->output_addrs,
                                                          queue);
  if (!edk::CnmlCheck(cnmlret, estr, "Compute Plugin ResizeYuv2Yuv Op failed.")) {
    return false;
  }
#if PRINT_TIME
  cnrtPlaceNotifier(yuv2yuv->event_end, queue);
#endif
  // sync queue
  int success = edk::CnrtCheck(cnrtSyncQueue(queue), estr, "Sync queue failed.");
#if PRINT_TIME
  auto end_tp = std::chrono::high_resolution_clock::now();
  float hw_time = 0.f;
  cnrtNotifierDuration(yuv2yuv->event_begin, yuv2yuv->event_end, &hw_time);
  std::cout << "hardware " << hw_time/1000.f << "ms" << std::endl;
  std::chrono::duration<double, std::milli> diff = end_tp - start_tp;
  std::cout << "software " << diff.count() << "ms" << std::endl;
#endif
  return success;
}
