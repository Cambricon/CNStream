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
#include <string.h>
#include "cnstream_logging.hpp"

#include "cnstream_common.hpp"
#include "cnstream_syncmem.hpp"

namespace cnstream {

/**
 * Allocates data on a host.
 *
 * @param ptr Outputs data pointer.
 * @param size The size of the data to be allocated.
 */
static void CNStreamMallocHost(void** ptr, size_t size) {
  void* __ptr = malloc(size);
  LOGF_IF(FRAME, nullptr == __ptr) << "Malloc memory on CPU failed, malloc size:" << size;
  *ptr = __ptr;
}


/**
 * Frees the data allocated by ``CNStreamMallocHost``.
 *
 * @param ptr The data address to be freed.
 */
static void CNStreamFreeHost(void* ptr) {
  free(ptr);
}

CNSyncedMemory::CNSyncedMemory(size_t size) : size_(size) {}

CNSyncedMemory::CNSyncedMemory(size_t size, int mlu_dev_id, int mlu_ddr_chn)
    : size_(size), dev_id_(mlu_dev_id), ddr_chn_(mlu_ddr_chn) {}

CNSyncedMemory::~CNSyncedMemory() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (0 == size_) return;
  if (cpu_ptr_ && own_cpu_data_) {
    free(cpu_ptr_);
  }
  if (mlu_ptr_ && own_mlu_data_) {
    // set device id before call cnrt functions, or CNRT_RET_ERR_EXISTS will be returned from cnrt function
    CALL_CNRT_BY_CONTEXT(cnrtFree(mlu_ptr_), dev_id_, ddr_chn_);
  }
}

inline void CNSyncedMemory::ToCpu() {
  if (0 == size_) return;
  switch (head_) {
    case SyncedHead::UNINITIALIZED:
      CNStreamMallocHost(&cpu_ptr_, size_);
      memset(cpu_ptr_, 0, size_);
      head_ = SyncedHead::HEAD_AT_CPU;
      own_cpu_data_ = true;
      break;
    case SyncedHead::HEAD_AT_MLU:
      if (NULL == cpu_ptr_) {
        CNStreamMallocHost(&cpu_ptr_, size_);
        own_cpu_data_ = true;
      }
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(cpu_ptr_, mlu_ptr_, size_, CNRT_MEM_TRANS_DIR_DEV2HOST), dev_id_, ddr_chn_);
      head_ = SyncedHead::SYNCED;
      break;
    case SyncedHead::HEAD_AT_CPU:
    case SyncedHead::SYNCED:
      break;
  }
}

inline void CNSyncedMemory::ToMlu() {
  if (0 == size_) return;
  switch (head_) {
    case SyncedHead::UNINITIALIZED:
      CALL_CNRT_BY_CONTEXT(cnrtMalloc(&mlu_ptr_, size_), dev_id_, ddr_chn_);
      head_ = SyncedHead::HEAD_AT_MLU;
      own_mlu_data_ = true;
      break;
    case SyncedHead::HEAD_AT_CPU:
      if (NULL == mlu_ptr_) {
        CALL_CNRT_BY_CONTEXT(cnrtMalloc(&mlu_ptr_, size_), dev_id_, ddr_chn_);
        own_mlu_data_ = true;
      }
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(mlu_ptr_, cpu_ptr_, size_, CNRT_MEM_TRANS_DIR_HOST2DEV), dev_id_, ddr_chn_);
      head_ = SyncedHead::SYNCED;
      break;
    case SyncedHead::HEAD_AT_MLU:
    case SyncedHead::SYNCED:
      break;
  }
}

const void* CNSyncedMemory::GetCpuData() {
  std::lock_guard<std::mutex> lock(mutex_);
  ToCpu();
  return const_cast<const void*>(cpu_ptr_);
}


void CNSyncedMemory::SetCpuData(void* data) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (0 == size_) return;
  LOGF_IF(FRAME, NULL == data) << "data is NULL.";
  if (own_cpu_data_) {
    CNStreamFreeHost(cpu_ptr_);
  }
  cpu_ptr_ = data;
  head_ = SyncedHead::HEAD_AT_CPU;
  own_cpu_data_ = false;
}

const void* CNSyncedMemory::GetMluData() {
  std::lock_guard<std::mutex> lock(mutex_);
  ToMlu();
  return const_cast<const void*>(mlu_ptr_);
}

void CNSyncedMemory::SetMluData(void* data) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (0 == size_) return;
  LOGF_IF(FRAME, nullptr == data) << "data is NULL.";
  if (own_mlu_data_) {
    CALL_CNRT_BY_CONTEXT(cnrtFree(mlu_ptr_), dev_id_, ddr_chn_);
  }
  mlu_ptr_ = data;
  head_ = SyncedHead::HEAD_AT_MLU;
  own_mlu_data_ = false;
}

void CNSyncedMemory::SetMluDevContext(int dev_id, int ddr_chn) {
  std::lock_guard<std::mutex> lock(mutex_);
  /*
    check device
   */
  cnrtDev_t dev;
  LOGF_IF(FRAME, CNRT_RET_SUCCESS != cnrtGetDeviceHandle(&dev, dev_id)) << "Can not find device by id: " << dev_id;
  // LOGF_IF(FRAME, ddr_chn < 0 || ddr_chn >= 4) << "Invalid ddr channel [0,4) :" << ddr_chn;

  dev_id_ = dev_id;
  ddr_chn_ = ddr_chn;
}

int CNSyncedMemory::GetMluDevId() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dev_id_;
}

int CNSyncedMemory::GetMluDdrChnId() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return ddr_chn_;
}

void* CNSyncedMemory::GetMutableCpuData() {
  std::lock_guard<std::mutex> lock(mutex_);
  ToCpu();
  head_ = SyncedHead::HEAD_AT_CPU;
  return cpu_ptr_;
}

void* CNSyncedMemory::GetMutableMluData() {
  std::lock_guard<std::mutex> lock(mutex_);
  ToMlu();
  head_ = SyncedHead::HEAD_AT_MLU;
  return mlu_ptr_;
}

}  // namespace cnstream
