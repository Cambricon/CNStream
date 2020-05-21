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
#include "easyplugin/resize_yuv_to_rgba.h"
#include "glog/logging.h"

#define PRINT_TIME 0

using std::string;
using std::to_string;

struct ResizeYuv2Rgba {
  int** src_wh_mlu_ptr = nullptr;
  int** src_wh_trans_ptr = nullptr;
  int** src_wh_cpu_ptr = nullptr;
  int** roi_rect_mlu_ptr = nullptr;
  int** roi_rect_trans_ptr = nullptr;
  int** roi_rect_cpu_ptr = nullptr;
  void* fill_color_mlu_ptr = nullptr;
  void** input_addrs = nullptr;
  void** output_addrs = nullptr;
  cnmlPluginResizeAndColorCvtParam_t param = nullptr;
  cnmlBaseOp_t op = nullptr;
  cnmlTensor_t* cnml_input_ptr = nullptr;
  cnmlTensor_t* cnml_output_ptr = nullptr;
  // enum
  cnmlCoreVersion_t version = CNML_MLU270;
#if PRINT_TIME
  cnrtNotifier_t event_begin = nullptr;
  cnrtNotifier_t event_end = nullptr;
#endif
};

bool CreateParam(const edk::MluResizeAttr& attr, ResizeYuv2Rgba* yuv2rgba, string* estr) {
  if (!yuv2rgba) {
    *estr = "[SetFillColor] yuv2rgba is nullptr";
    return false;
  }
  switch (attr.core_version) {
    case edk::CoreVersion::MLU100:
      yuv2rgba->version = CNML_MLU100;
      LOG(INFO) << "core version MLU100";
      break;
    case edk::CoreVersion::MLU220:
      yuv2rgba->version = CNML_MLU220;
      LOG(INFO) << "core version MLU220";
      break;
    case edk::CoreVersion::MLU270:
      yuv2rgba->version = CNML_MLU270;
      LOG(INFO) << "core version MLU270";
      break;
    default:
      LOG(ERROR) << "unsurpported core version" << std::endl;
      *estr = "unsurpported core version";
      return false;
  }

  ioParams mode;
  mode.color = cnmlPluginColorCvt_t(static_cast<int>(attr.color_mode));
  mode.datatype = cnmlPluginDataType_t(static_cast<int>(attr.data_mode));

  auto cnmlret = cnmlCreatePluginResizeYuvToRgbaOpParam_V2(&(yuv2rgba->param),
                                                           attr.dst_h,
                                                           attr.dst_w,
                                                           mode,
                                                           attr.batch_size,
                                                           attr.keep_aspect_ratio,
                                                           yuv2rgba->version);
  if (!edk::CnmlCheck(cnmlret, estr, "Create Plugin ResizeYuv2rgba Op param failed.")) {
    return false;
  }
  return true;
}

void FreeTensorPtr(ResizeYuv2Rgba* yuv2rgba) {
  if (yuv2rgba) {
    if (yuv2rgba->cnml_input_ptr) {
      free(yuv2rgba->cnml_input_ptr);
    }
    if (yuv2rgba->cnml_output_ptr) {
      free(yuv2rgba->cnml_output_ptr);
    }
  }
}

void InitTensorPtr(ResizeYuv2Rgba* yuv2rgba) {
  if (yuv2rgba) {
    auto param = yuv2rgba->param;
    yuv2rgba->cnml_input_ptr = reinterpret_cast<cnmlTensor_t*>(malloc(sizeof(cnmlTensor_t) * param->input_num));
    yuv2rgba->cnml_output_ptr = reinterpret_cast<cnmlTensor_t*>(malloc(sizeof(cnmlTensor_t) * param->output_num));
  }
}

bool DestroyTensor(ResizeYuv2Rgba* yuv2rgba, string* estr) {
  if (!yuv2rgba) {
    *estr = "[SetFillColor] yuv2rgba is nullptr";
    return false;
  }
  auto param = yuv2rgba->param;
  bool success = true;
  for (int i = 0; i < param->input_num; i++) {
    if (yuv2rgba->cnml_input_ptr[i]) {
      success = edk::CnmlCheck(cnmlDestroyTensor(&yuv2rgba->cnml_input_ptr[i]), estr, "Destroy input Tensor failed.");
    }
  }
  for (int i = 0; i < param->output_num; i++) {
    if (yuv2rgba->cnml_output_ptr[i]) {
      success = edk::CnmlCheck(cnmlDestroyTensor(&yuv2rgba->cnml_output_ptr[i]), estr, "Destroy output Tensor failed.");
    }
  }
  return success;
}

bool CreateTensor(ResizeYuv2Rgba* yuv2rgba, string* estr) {
  if (!yuv2rgba) {
    *estr = "[SetFillColor] yuv2rgba is nullptr";
    return false;
  }
  auto param = yuv2rgba->param;

  if (param->input_num != 5 || param->output_num != 1) {
    *estr = "Input number is not 5 or output number is not 1";
    return false;
  }

  int input_shape[5][4] = {{param->batchNum, 1, 1, 1},
                           {param->batchNum, 1, 1, 1},
                           {param->batchNum, 1, 1, 1},
                           {param->batchNum, 1, 1, 1},
                           {1, 1, 1, 3}};
  int output_shape[4] = {param->batchNum, 1, 1, param->channelOut * param->d_row * param->d_col};
  cnmlDataType_t in_dt[5] = {param->inputDT_MLU, param->inputDT_MLU, CNML_DATA_INT32, CNML_DATA_INT32, CNML_DATA_UINT8};

  for (uint8_t input_i = 0; input_i < 5; input_i++) {
    auto cnmlret = cnmlCreateTensor_V2(&(yuv2rgba->cnml_input_ptr[input_i]), CNML_TENSOR);
    if (!edk::CnmlCheck(cnmlret, estr, "Create input tensor failed.")) return false;

    cnmlret = cnmlSetTensorShape(yuv2rgba->cnml_input_ptr[input_i], 4, input_shape[input_i]);
    if (!edk::CnmlCheck(cnmlret, estr, "Set input tensor shape failed.")) return false;

    cnmlret = cnmlSetTensorDataType(yuv2rgba->cnml_input_ptr[input_i], in_dt[input_i]);
    if (!edk::CnmlCheck(cnmlret, estr, "Set input tensor data type failed.")) return false;
  }

  auto cnmlret = cnmlCreateTensor_V2(&(yuv2rgba->cnml_output_ptr[0]), CNML_TENSOR);
  if (!edk::CnmlCheck(cnmlret, estr, "Create output tensor failed.")) return false;

  cnmlret = cnmlSetTensorShape(yuv2rgba->cnml_output_ptr[0], 4, output_shape);
  if (!edk::CnmlCheck(cnmlret, estr, "Set output tensor shape failed.")) return false;

  cnmlret = cnmlSetTensorDataType(yuv2rgba->cnml_output_ptr[0], param->outputDT_MLU);
  if (!edk::CnmlCheck(cnmlret, estr, "Set output tensor shape failed.")) return false;

  return true;
}

bool CreateAndCompileOp(const int& core_limit, ResizeYuv2Rgba* yuv2rgba, string* estr) {
  if (!yuv2rgba) {
    *estr = "[SetFillColor] yuv2rgba is nullptr";
    return false;
  }
  auto param = yuv2rgba->param;
  InitTensorPtr(yuv2rgba);
  if (!CreateTensor(yuv2rgba, estr)) {
    return false;
  }
  auto cnmlret =
    cnmlCreatePluginResizeYuvToRgbaOp_V2(&(yuv2rgba->op), param, yuv2rgba->cnml_input_ptr, yuv2rgba->cnml_output_ptr);
  if (!edk::CnmlCheck(cnmlret, estr, "Create Plugin ResizeYuvToRgba Op failed.")) return false;

  cnmlret = cnmlCompileBaseOp(yuv2rgba->op, yuv2rgba->version, core_limit);
  if (!edk::CnmlCheck(cnmlret, estr, "Compile Plugin ResizeYuvToRgba Op failed.")) return false;

  return true;
}

bool FreeSrcWHMluPtr(ResizeYuv2Rgba* yuv2rgba, string* estr) {
  if (!yuv2rgba) {
    *estr = "[SetFillColor] yuv2rgba is nullptr";
    return false;
  }
  bool success = true;
  int batchsize = yuv2rgba->param->batchNum;
  for (int batch_i = 0; batch_i < batchsize; batch_i++) {
    if (yuv2rgba->src_wh_cpu_ptr[batch_i]) {
      free(yuv2rgba->src_wh_cpu_ptr[batch_i]);
    }
    if (yuv2rgba->src_wh_trans_ptr[batch_i]) {
      success = edk::CnrtCheck(cnrtFree(yuv2rgba->src_wh_trans_ptr[batch_i]), estr, "Free src_wh_trans_ptr failed.");
    }
  }

  if (yuv2rgba->src_wh_mlu_ptr) {
    success = edk::CnrtCheck(cnrtFree(yuv2rgba->src_wh_mlu_ptr), estr, "Free src_wh_mlu_ptr failed.");
  }

  if (yuv2rgba->src_wh_cpu_ptr) {
    free(yuv2rgba->src_wh_cpu_ptr);
  }
  if (yuv2rgba->src_wh_trans_ptr) {
    free(yuv2rgba->src_wh_trans_ptr);
  }

  return success;
}

bool InitSrcWHMluPtr(ResizeYuv2Rgba* yuv2rgba, string* estr) {
  if (!yuv2rgba) {
    *estr = "[SetFillColor] yuv2rgba is nullptr";
    return false;
  }
  int batchsize = yuv2rgba->param->batchNum;

  if (yuv2rgba->src_wh_cpu_ptr || yuv2rgba->src_wh_trans_ptr || yuv2rgba->src_wh_mlu_ptr) {
    *estr = "[SetFillColor] src wh pointer is existed";
    return false;
  }

  yuv2rgba->src_wh_cpu_ptr = reinterpret_cast<int**>(malloc(batchsize * sizeof(int*)));
  yuv2rgba->src_wh_trans_ptr = reinterpret_cast<int**>(malloc(batchsize * sizeof(int*)));

  for (int batch_i = 0; batch_i < batchsize; batch_i++) {
    yuv2rgba->src_wh_cpu_ptr[batch_i] = reinterpret_cast<int*>(malloc(2 * sizeof(int)));
    auto cnrtret = cnrtMalloc(reinterpret_cast<void**>(&yuv2rgba->src_wh_trans_ptr[batch_i]), 2 * sizeof(int));
    if (!edk::CnrtCheck(cnrtret, estr, "Malloc src_wh_trans_ptr failed.")) return false;
  }

  auto cnrtret = cnrtMalloc(reinterpret_cast<void**>(&yuv2rgba->src_wh_mlu_ptr), batchsize * sizeof(int*));
  if (!edk::CnrtCheck(cnrtret, estr, "Malloc src_wh_mlu_ptr failed.")) return false;

  return true;
}

bool SetSrcWHMluPtr(const edk::MluResizeAttr& attr, ResizeYuv2Rgba* yuv2rgba, string* estr) {
  if (!yuv2rgba) {
    *estr = "[SetFillColor] yuv2rgba is nullptr";
    return false;
  }
  int batchsize = yuv2rgba->param->batchNum;

  if (!yuv2rgba->src_wh_cpu_ptr) return false;

  for (int batch_i = 0; batch_i < batchsize; batch_i++) {
    yuv2rgba->src_wh_cpu_ptr[batch_i][0] = attr.src_stride;
    yuv2rgba->src_wh_cpu_ptr[batch_i][1] = attr.src_h;

    auto cnrtret = cnrtMemcpy(yuv2rgba->src_wh_trans_ptr[batch_i], yuv2rgba->src_wh_cpu_ptr[batch_i], 2 * sizeof(int),
                              CNRT_MEM_TRANS_DIR_HOST2DEV);
    if (!edk::CnrtCheck(cnrtret, estr, "Memcpy src_wh_trans_ptr failed.")) return false;
  }

  auto cnrtret = cnrtMemcpy(yuv2rgba->src_wh_mlu_ptr, yuv2rgba->src_wh_trans_ptr, batchsize * sizeof(int*),
                            CNRT_MEM_TRANS_DIR_HOST2DEV);
  if (!edk::CnrtCheck(cnrtret, estr, "Memcpy src_wh_mlu_ptr failed.")) return false;

  return true;
}

bool FreeRoiMluPtr(ResizeYuv2Rgba* yuv2rgba, string* estr) {
  if (!yuv2rgba) {
    *estr = "[SetFillColor] yuv2rgba is nullptr";
    return false;
  }
  bool success = true;
  int batchsize = yuv2rgba->param->batchNum;

  for (int batch_i = 0; batch_i < batchsize; batch_i++) {
    if (yuv2rgba->roi_rect_cpu_ptr[batch_i]) {
      free(yuv2rgba->roi_rect_cpu_ptr[batch_i]);
    }
    if (yuv2rgba->roi_rect_trans_ptr[batch_i]) {
      success = edk::CnrtCheck(cnrtFree(yuv2rgba->roi_rect_trans_ptr[batch_i]),
                               estr, "Free roi_rect_trans_ptr failed.");
    }
  }

  if (yuv2rgba->roi_rect_mlu_ptr) {
    success = edk::CnrtCheck(cnrtFree(yuv2rgba->roi_rect_mlu_ptr), estr, "Free roi_rect_mlu_ptr failed.");
  }

  if (yuv2rgba->roi_rect_cpu_ptr) {
    free(yuv2rgba->roi_rect_cpu_ptr);
  }
  if (yuv2rgba->roi_rect_trans_ptr) {
    free(yuv2rgba->roi_rect_trans_ptr);
  }

  return success;
}

bool InitRoiMluPtr(ResizeYuv2Rgba* yuv2rgba, string* estr) {
  if (!yuv2rgba) {
    *estr = "[SetFillColor] yuv2rgba is nullptr";
    return false;
  }
  int batchsize = yuv2rgba->param->batchNum;

  if (yuv2rgba->roi_rect_cpu_ptr || yuv2rgba->roi_rect_trans_ptr || yuv2rgba->roi_rect_mlu_ptr) {
    *estr = "[SetFillColor] roi rect pointer is existed";
    return false;
  }

  yuv2rgba->roi_rect_cpu_ptr = reinterpret_cast<int**>(malloc(batchsize * sizeof(int*)));
  yuv2rgba->roi_rect_trans_ptr = reinterpret_cast<int**>(malloc(batchsize * sizeof(int*)));

  for (int batch_i = 0; batch_i < batchsize; batch_i++) {
    yuv2rgba->roi_rect_cpu_ptr[batch_i] = reinterpret_cast<int*>(malloc(4 * sizeof(int)));
    auto cnrtret = cnrtMalloc(reinterpret_cast<void**>(&yuv2rgba->roi_rect_trans_ptr[batch_i]), 4 * sizeof(int));
    if (!edk::CnrtCheck(cnrtret, estr, "Malloc roi_rect_trans_ptr failed.")) return false;
  }

  auto cnrtret = cnrtMalloc(reinterpret_cast<void**>(&yuv2rgba->roi_rect_mlu_ptr), batchsize * sizeof(int*));
  if (!edk::CnrtCheck(cnrtret, estr, "Malloc roi_rect_mlu_ptr failed.")) return false;

  return true;
}

bool SetRoi(const edk::MluResizeAttr& attr, ResizeYuv2Rgba* yuv2rgba, string* estr) {
  if (!yuv2rgba) {
    *estr = "[SetFillColor] yuv2rgba is nullptr";
    return false;
  }
  int batchsize = yuv2rgba->param->batchNum;

  if (!yuv2rgba->roi_rect_cpu_ptr) return false;

  for (int batch_i = 0; batch_i < batchsize; batch_i++) {
    yuv2rgba->roi_rect_cpu_ptr[batch_i][0] = attr.crop_x;
    yuv2rgba->roi_rect_cpu_ptr[batch_i][1] = attr.crop_y;
    yuv2rgba->roi_rect_cpu_ptr[batch_i][2] = attr.crop_w;
    yuv2rgba->roi_rect_cpu_ptr[batch_i][3] = attr.crop_h;

    auto ret = cnrtMemcpy(yuv2rgba->roi_rect_trans_ptr[batch_i], yuv2rgba->roi_rect_cpu_ptr[batch_i], 4 * sizeof(int),
                              CNRT_MEM_TRANS_DIR_HOST2DEV);
    if (!edk::CnrtCheck(ret, estr, "Memcpy roi_rect_trans_ptr failed.")) return false;
  }

  auto cnrtret = cnrtMemcpy(yuv2rgba->roi_rect_mlu_ptr, yuv2rgba->roi_rect_trans_ptr, batchsize * sizeof(int*),
                            CNRT_MEM_TRANS_DIR_HOST2DEV);
  if (!edk::CnrtCheck(cnrtret, estr, "Memcpy roi_rect_mlu_ptr failed.")) return false;

  return true;
}

bool FreeFillColorMluPtr(ResizeYuv2Rgba* yuv2rgba, string* estr) {
  if (!yuv2rgba) {
    *estr = "[SetFillColor] yuv2rgba is nullptr";
    return false;
  }
  if (yuv2rgba->fill_color_mlu_ptr) {
    return edk::CnrtCheck(cnrtFree(yuv2rgba->fill_color_mlu_ptr), estr, "Free fill_color_mlu_ptr failed.");
  }
  return true;
}

bool InitFillColorMluPtr(ResizeYuv2Rgba* yuv2rgba, string* estr) {
  if (!yuv2rgba) {
    *estr = "[SetFillColor] yuv2rgba is nullptr";
    return false;
  }

  return edk::CnrtCheck(cnrtMalloc(reinterpret_cast<void**>(&yuv2rgba->fill_color_mlu_ptr), 3 * sizeof(uint8_t*)),
      estr, "Malloc fill_color_mlu_ptr failed.");
}

bool SetFillColor(const uint8_t& r, const uint8_t& g, const uint8_t& b, ResizeYuv2Rgba* yuv2rgba, string* estr) {
  if (!yuv2rgba) {
    *estr = "[SetFillColor] yuv2rgba is nullptr";
    return false;
  }
  bool success = true;
  uint8_t* fill_color_cpu_ptr = reinterpret_cast<uint8_t*>(malloc(3 * sizeof(uint8_t*)));
  fill_color_cpu_ptr[0] = r;
  fill_color_cpu_ptr[1] = g;
  fill_color_cpu_ptr[2] = b;

  auto cnrtret = cnrtMemcpy(yuv2rgba->fill_color_mlu_ptr, fill_color_cpu_ptr, 3 * sizeof(uint8_t*),
      CNRT_MEM_TRANS_DIR_HOST2DEV);
  success = edk::CnrtCheck(cnrtret, estr, "Memcpy roi_rect_mlu_ptr failed.");

  free(fill_color_cpu_ptr);
  return success;
}

void FreeIoAddrsPtr(ResizeYuv2Rgba* yuv2rgba) {
  if (yuv2rgba) {
    if (yuv2rgba->input_addrs) {
      free(yuv2rgba->input_addrs);
    }
    if (yuv2rgba->output_addrs) {
      free(yuv2rgba->output_addrs);
    }
  }
}

void InitIOAddrsPtr(ResizeYuv2Rgba* yuv2rgba) {
  if (yuv2rgba) {
    yuv2rgba->input_addrs = reinterpret_cast<void **>(malloc(sizeof(void*) * yuv2rgba->param->input_num));
    yuv2rgba->output_addrs = reinterpret_cast<void **>(malloc(sizeof(void*) * yuv2rgba->param->output_num));
  }
}

bool DestroyResizeYuv2Rgba(ResizeYuv2Rgba* yuv2rgba, string* estr) {
  bool success = true;
  if (yuv2rgba) {
#if PRINT_TIME
    if (yuv2rgba->event_begin) {
      success = edk::CnrtCheck(cnrtDestroyNotifier(&yuv2rgba->event_begin), estr, "Destroy event begin failed.");
    }
    if (yuv2rgba->event_end) {
      success = edk::CnrtCheck(cnrtDestroyNotifier(&yuv2rgba->event_end), estr, "Destroy event end failed.");
    }
#endif
    success = DestroyTensor(yuv2rgba, estr);
    FreeTensorPtr(yuv2rgba);

    success = FreeSrcWHMluPtr(yuv2rgba, estr);
    success = FreeRoiMluPtr(yuv2rgba, estr);
    success = FreeFillColorMluPtr(yuv2rgba, estr);

    FreeIoAddrsPtr(yuv2rgba);

    if (yuv2rgba->op) {
      success = edk::CnmlCheck(cnmlDestroyBaseOp(&(yuv2rgba->op)), estr, "Destroy resize yuv2rgba op failed.");
    }
    if (yuv2rgba->param) {
      success = edk::CnmlCheck(cnmlDestroyPluginResizeYuvToRgbaOpParam_V2(&yuv2rgba->param),
                       estr, "Destroy resize yuv2rgba param failed.");
    }
    delete yuv2rgba;
  }
  success = edk::CnmlCheck(cnmlExit(), estr, "Exit failed.");
  return success;
}

bool CreateResizeYuv2Rgba(const edk::MluResizeAttr& attr, ResizeYuv2Rgba** yuv2rgba_ptr, string* estr) {
  (*yuv2rgba_ptr) = new ResizeYuv2Rgba;

  if (!*yuv2rgba_ptr) {
    *estr = "Create ResizeYuv2Rgba pointer failed";
    return false;
  }
  if (!edk::CnmlCheck(cnmlInit(0), estr, "Init failed")) return false;

#if PRINT_TIME
  if (!edk::CnrtCheck(cnrtCreateNotifier(&(*yuv2rgba_ptr)->event_begin), estr, "create notifier event_begin failed.")) {
    return false;
  }
  if (!edk::CnrtCheck(cnrtCreateNotifier(&(*yuv2rgba_ptr)->event_end), estr, "create notifier event_end failed.")) {
    return false;
  }
#endif
  if (!CreateParam(attr, *yuv2rgba_ptr, estr)) return false;
  if (!CreateAndCompileOp(attr.core_number, *yuv2rgba_ptr, estr)) return false;

  if (!InitSrcWHMluPtr(*yuv2rgba_ptr, estr)) return false;
  if (!SetSrcWHMluPtr(attr, *yuv2rgba_ptr, estr)) return false;

  if (!InitRoiMluPtr(*yuv2rgba_ptr, estr)) return false;
  if (!SetRoi(attr, *yuv2rgba_ptr, estr)) return false;

  if (!InitFillColorMluPtr(*yuv2rgba_ptr, estr)) return false;
  if (!SetFillColor(attr.fill_color_r, attr.fill_color_g, attr.fill_color_b, *yuv2rgba_ptr, estr)) {
    return false;
  }

  InitIOAddrsPtr(*yuv2rgba_ptr);
  return true;
}

bool ComputeResizeYuv2Rgba(void* dst, void* src_y, void* src_uv, ResizeYuv2Rgba* yuv2rgba, cnrtQueue_t queue,
                           string* estr) {
  if (!yuv2rgba) {
    *estr = "[ComputeResizeYuv2Rgba] yuv2rgba is nullptr";
    return false;
  }
  yuv2rgba->input_addrs[0] = src_y;
  yuv2rgba->input_addrs[1] = src_uv;
  yuv2rgba->input_addrs[2] = reinterpret_cast<void*>(yuv2rgba->src_wh_mlu_ptr);
  yuv2rgba->input_addrs[3] = reinterpret_cast<void*>(yuv2rgba->roi_rect_mlu_ptr);
  yuv2rgba->input_addrs[4] = reinterpret_cast<void*>(yuv2rgba->fill_color_mlu_ptr);
  yuv2rgba->output_addrs[0] = dst;
#if PRINT_TIME
  cnrtPlaceNotifier(yuv2rgba->event_begin, queue);
  auto start_tp = std::chrono::high_resolution_clock::now();
#endif

  auto cnmlret = cnmlComputePluginResizeYuvToRgbaOpForward_V2(yuv2rgba->op,
                                                              yuv2rgba->param,
                                                              yuv2rgba->cnml_input_ptr,
                                                              yuv2rgba->input_addrs,
                                                              yuv2rgba->cnml_output_ptr,
                                                              yuv2rgba->output_addrs,
                                                              queue);
  if (!edk::CnmlCheck(cnmlret, estr, "Compute Plugin ResizeYuv2Rgba Op failed.")) {
    return false;
  }
#if PRINT_TIME
  cnrtPlaceNotifier(yuv2rgba->event_end, queue);
#endif
  int success = edk::CnrtCheck(cnrtSyncQueue(queue), estr, "Sync queue failed.");
#if PRINT_TIME
  auto end_tp = std::chrono::high_resolution_clock::now();

  float hw_time = 0.f;
  cnrtNotifierDuration(yuv2rgba->event_begin, yuv2rgba->event_end, &hw_time);
  std::cout << "hardware " << hw_time/1000.f << "ms" << std::endl;
  std::chrono::duration<double, std::milli> diff = end_tp - start_tp;
  std::cout << "software " << diff.count() << "ms" << std::endl;
#endif
  return success;
}
