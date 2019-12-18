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

#ifndef KCF_H_
#define KCF_H_

#include "cnrt.h"

typedef unsigned short half;

struct KCFHandle {
  half* dft_mat;
  half* cos_table;
  half threshold;
  int* mlu_buffer;
  int* cpu_buffer;
  half* args;
  half* scale;
  cnrtQueue_t pQueue;
};

struct __Rect {
  int x;
  int y;
  int width;
  int height;
  float score;
  int label;
};

void kcf_init(KCFHandle* handle, cnrtQueue_t queue, float threshold);
void kcf_destroy(KCFHandle* handle);
void kcf_initKernel(KCFHandle* handle, half* frame, half* rois_mlu, __Rect* out_roi, int* p_roi_num);
void kcf_updateKernel(KCFHandle* handle, half* frame, __Rect* out_roi, int roi_num);

#endif
