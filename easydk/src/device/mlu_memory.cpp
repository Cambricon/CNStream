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
#include <glog/logging.h>

#include <memory>
#include <string>

#include "device/mlu_memory.h"

namespace edk {

#define CHECK_CNRT_RET(err_code, msg)                                                    \
  if (CNRT_RET_SUCCESS != err_code) {                                                    \
    throw MluMemoryError(std::string(msg) + " error code: " + std::to_string(err_code)); \
  }

MluMemory::MluMemory(size_t memory_size, int device_id) : len_(memory_size), device_id_(device_id) {
  if (!memory_size) {
    throw MluMemoryError("memory cannot be empty");
  }
}

MluMemory::MluMemory(void* mlu_memory, size_t memory_size, Deleter d, int device_id)
    : data_(mlu_memory), len_(memory_size), deleter_(d), device_id_(device_id) {
  if (!mlu_memory || !memory_size) {
    throw MluMemoryError("memory cannot be empty");
  }
}

MluMemory::~MluMemory() {
  if (data_) {
    deleter_(data_, len_);
  }
  data_ = nullptr;
  len_ = 0;
}

void MluMemory::LazyMalloc() {
  CHECK(len_);
  if (!data_) {
    cnrtRet_t error_code;
    VLOG(4) << "Alloc memory on MLU in " << len_ << " bytes";
    error_code = cnrtMalloc(&data_, len_);
    CHECK_CNRT_RET(error_code, "Mlu malloc failed.");
    deleter_ = [](void* data, int device_id) {
      VLOG(4) << "Free memory on MLU";
      cnrtRet_t ret = cnrtFree(data);
      if (CNRT_RET_SUCCESS != ret) {
        LOG(ERROR) << "free memory failed, error code: " << ret;
      }
    };
  }
}

void* MluMemory::MutableData() {
  LazyMalloc();
  return data_;
}

const void* MluMemory::Data() const noexcept { return data_; }

void MluMemory::CopyFrom(void* cpu_src, size_t memory_size) {
  if (memory_size != len_) {
    throw MluMemoryError("copy: memory size not match");
  }
  LazyMalloc();
  if (data_) {
    cnrtRet_t error_code;
    VLOG(5) << "copy memory from host to device in size " << memory_size << ", dst: " << data_ << ", src: " << cpu_src;
    error_code = cnrtMemcpy(data_, cpu_src, memory_size, CNRT_MEM_TRANS_DIR_HOST2DEV);
    CHECK_CNRT_RET(error_code, "Memcpy host to device failed.");
  } else {
    throw MluMemoryError("Copy failed since malloc failed");
  }
}

void MluMemory::CopyTo(void* cpu_dst, size_t memory_size) const {
  if (memory_size != len_) {
    throw MluMemoryError("copy: memory size not match");
  }
  if (data_) {
    cnrtRet_t error_code;
    VLOG(5) << "copy memory from device to host in size " << memory_size << ", dst: " << cpu_dst << ", src: " << data_;
    error_code = cnrtMemcpy(cpu_dst, data_, memory_size, CNRT_MEM_TRANS_DIR_DEV2HOST);
    CHECK_CNRT_RET(error_code, "Memcpy host to device failed.");
  } else {
    throw MluMemoryError("no data, won't copy");
  }
}

void MluMemory::CopyFrom(const MluMemory& mlu_src) {
  if (mlu_src.MemorySize() != len_) {
    throw MluMemoryError("copy: memory size not match");
  }
  LazyMalloc();
  if (mlu_src.data_) {
    cnrtRet_t error_code;
    VLOG(5) << "copy memory from device to device in size " << len_ << ", dst: " << data_
            << ", src: " << mlu_src.Data();
    error_code = cnrtMemcpy(data_, const_cast<void*>(mlu_src.Data()), len_, CNRT_MEM_TRANS_DIR_DEV2DEV);
    CHECK_CNRT_RET(error_code, "Memcpy device to device failed.");
  } else {
    throw MluMemoryError("no data, won't copy");
  }
}

}  // namespace edk
