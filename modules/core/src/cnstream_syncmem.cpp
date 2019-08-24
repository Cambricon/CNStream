/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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
#include <glog/logging.h>
#include <cassert>

#include "cnstream_common.hpp"
#include "cnstream_syncmem.hpp"

namespace cnstream {

void CNStreamMallocHost(void** ptr, size_t size) {
  void* __ptr = malloc(size);
  LOG_IF(FATAL, nullptr == __ptr) << "Malloc memory on CPU failed, malloc size:" << size;
  *ptr = __ptr;
}

CNSyncedMemory::CNSyncedMemory() {}

CNSyncedMemory::CNSyncedMemory(size_t size) : size_(size) {}

CNSyncedMemory::CNSyncedMemory(size_t size, int mlu_dev_id, int mlu_ddr_chn)
    : size_(size), dev_id_(mlu_dev_id), ddr_chn_(mlu_ddr_chn) {}

CNSyncedMemory::~CNSyncedMemory() {
  if (0 == size_) return;
  if (cpu_ptr_ && own_cpu_data_) {
    free(cpu_ptr_);
  }
  if (mlu_ptr_ && own_mlu_data_) {
    CNS_CNRT_CHECK(cnrtFree(mlu_ptr_));
  }
}

inline void CNSyncedMemory::ToCpu() {
  if (0 == size_) return;
  switch (head_) {
    case UNINITIALIZED:
      CNStreamMallocHost(&cpu_ptr_, size_);
      memset(cpu_ptr_, 0, size_);
      head_ = HEAD_AT_CPU;
      own_cpu_data_ = true;
      break;
    case HEAD_AT_MLU:
      if (NULL == cpu_ptr_) {
        CNStreamMallocHost(&cpu_ptr_, size_);
        own_cpu_data_ = true;
      }
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(cpu_ptr_, mlu_ptr_, size_, CNRT_MEM_TRANS_DIR_DEV2HOST), dev_id_, ddr_chn_);
      head_ = SYNCED;
      break;
    case HEAD_AT_CPU:
    case SYNCED:
      break;
  }
}

inline void CNSyncedMemory::ToMlu() {
  if (0 == size_) return;
  switch (head_) {
    case UNINITIALIZED:
      CALL_CNRT_BY_CONTEXT(cnrtMalloc(&mlu_ptr_, size_), dev_id_, ddr_chn_);
      head_ = HEAD_AT_MLU;
      own_mlu_data_ = true;
      break;
    case HEAD_AT_CPU:
      if (NULL == mlu_ptr_) {
        CALL_CNRT_BY_CONTEXT(cnrtMalloc(&mlu_ptr_, size_), dev_id_, ddr_chn_);
        own_mlu_data_ = true;
      }
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(mlu_ptr_, cpu_ptr_, size_, CNRT_MEM_TRANS_DIR_HOST2DEV), dev_id_, ddr_chn_);
      head_ = SYNCED;
      break;
    case HEAD_AT_MLU:
    case SYNCED:
      break;
  }
}

const void* CNSyncedMemory::GetCpuData() {
  ToCpu();
  return const_cast<const void*>(cpu_ptr_);
}

void CNSyncedMemory::SetCpuData(void* data) {
  if (0 == size_) return;
  LOG_IF(FATAL, NULL == data) << "data is NULL.";
  if (own_cpu_data_) {
    CNStreamFreeHost(cpu_ptr_);
  }
  cpu_ptr_ = data;
  head_ = HEAD_AT_CPU;
  own_cpu_data_ = false;
}

const void* CNSyncedMemory::GetMluData() {
  ToMlu();
  return const_cast<const void*>(mlu_ptr_);
}

void CNSyncedMemory::SetMluData(void* data) {
  if (0 == size_) return;
  LOG_IF(FATAL, nullptr == data) << "data is NULL.";
  if (own_mlu_data_) {
    CALL_CNRT_BY_CONTEXT(cnrtFree(mlu_ptr_), dev_id_, ddr_chn_);
  }
  mlu_ptr_ = data;
  head_ = HEAD_AT_MLU;
  own_mlu_data_ = false;
}

void CNSyncedMemory::SetMluDevContext(int dev_id, int ddr_chn) {
  /*
    check device
   */
  cnrtDev_t dev;
  LOG_IF(FATAL, CNRT_RET_SUCCESS != cnrtGetDeviceHandle(&dev, dev_id)) << "Can not find device by id: " << dev_id;
  LOG_IF(FATAL, ddr_chn < 0 || ddr_chn >= 4) << "Invalid ddr channel [0,4) :" << ddr_chn;

  dev_id_ = dev_id;
  ddr_chn_ = ddr_chn;
}

int CNSyncedMemory::GetMluDevId() const { return dev_id_; }

int CNSyncedMemory::GetMluDdrChnId() const { return ddr_chn_; }

void* CNSyncedMemory::GetMutableCpuData() {
  ToCpu();
  return cpu_ptr_;
}

void* CNSyncedMemory::GetMutableMluData() {
  ToMlu();
  return mlu_ptr_;
}

}  // namespace cnstream
