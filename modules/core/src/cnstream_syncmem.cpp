/*
All modification made by Cambricon Corporation: © 2018--2019 Cambricon Corporation
All rights reserved.
All other contributions:
Copyright (c) 2014--2018, the respective contributors
All rights reserved.
For the list of contributors go to https://github.com/BVLC/caffe/blob/master/CONTRIBUTORS.md
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <cnrt.h>
#include <glog/logging.h>

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
