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
#include "cnstream_allocator.hpp"

#include <exception>
#include <memory>

#include "cnrt.h"

namespace cnstream {

class CnrtInit {
 public:
  CnrtInit() { cnrtInit(0); }
  ~CnrtInit() { cnrtDestroy(); }
};
static CnrtInit cnrt_init_;

MluDeviceGuard::MluDeviceGuard(int device_id) : device_id_(device_id) {
  // FIXME, check errors
  if (device_id < 0) {
    LOGE(Allocator) << "device id:  " << device_id << " is invalid";
    return;
  }
  cnrtDev_t dev;
  cnrtGetDeviceHandle(&dev, device_id_);
  cnrtSetCurrentDevice(dev);
}

MluDeviceGuard::~MluDeviceGuard() {}

// helper funcs
class CnAllocDeleter final {
 public:
  explicit CnAllocDeleter(std::shared_ptr<MemoryAllocator> allocator) : allocator_(allocator) {}

  void operator()(void *ptr) { allocator_->free(ptr); }

 private:
  std::shared_ptr<MemoryAllocator> allocator_;
};

std::shared_ptr<void> cnMemAlloc(size_t size, std::shared_ptr<MemoryAllocator> allocator) {
  if (allocator) {
    std::shared_ptr<void> ds(allocator->alloc(size), CnAllocDeleter(allocator));
    return ds;
  }
  return nullptr;
}

std::shared_ptr<void> cnCpuMemAlloc(size_t size) {
  std::shared_ptr<MemoryAllocator> allocator = std::make_shared<CpuAllocator>();
  return cnMemAlloc(size, allocator);
}

std::shared_ptr<void> cnMluMemAlloc(size_t size, int device_id) {
  std::shared_ptr<MemoryAllocator> allocator = std::make_shared<MluAllocator>(device_id);
  return cnMemAlloc(size, allocator);
}

// cpu Var-size allocator
void *CpuAllocator::alloc(size_t size, int timeout_ms) {
  size_t alloc_size = (size + 4095) & (~0xFFF);  // Align 4096
  // LOGI("Allocator") << "CpuAllocator::alloc  " << size << "\n";
  return static_cast<void *>(new (std::nothrow) unsigned char[alloc_size]);
}

void CpuAllocator::free(void *p) {
  unsigned char *ptr = static_cast<unsigned char *>(p);
  // LOGI("Allocator") << "CpuAllocator::free\n";
  delete[] ptr;
}

// mlu var-size allocator
void *MluAllocator::alloc(size_t size, int timeout_ms) {
  size_t alloc_size = (size + 4095) & (~0xFFF);  // Align 4096

  std::lock_guard<std::mutex> lk(mutex_);
  MluDeviceGuard guard(device_id_);
  void *mlu_ptr;
  if (cnrtMalloc(&mlu_ptr, alloc_size) != CNRT_RET_SUCCESS) {
    return nullptr;
  }

  // LOGI("Allocator") << "MluAllocator::alloc  " << size << "\n";
  return mlu_ptr;
}

void MluAllocator::free(void *p) {
  std::lock_guard<std::mutex> lk(mutex_);
  MluDeviceGuard guard(device_id_);
  cnrtFree(p);
  // LOGI("Allocator") << "MluAllocator::free\n";
}

}  // namespace cnstream
