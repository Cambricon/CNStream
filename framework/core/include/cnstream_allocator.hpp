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
#ifndef CNSTREAM_ALLOCATOR_HPP_
#define CNSTREAM_ALLOCATOR_HPP_

#include <atomic>
#include <memory>
#include <new>
#include "cnstream_common.hpp"
#include "cnstream_logging.hpp"
#include "util/cnstream_queue.hpp"

/**
 *  @file cnstream_allocator.hpp
 *
 *  This file contains a declaration of the CNstream memory allocator.
 */
namespace cnstream {

class MluDeviceGuard : public NonCopyable {
 public:
  MluDeviceGuard(int device_id);
  ~MluDeviceGuard(); 
 
 private:
  int device_id_ = 0;
};

class MemoryAllocator : public NonCopyable {
 public:
  explicit MemoryAllocator(int device_id) : device_id_(device_id) {}
  virtual ~MemoryAllocator() = default;
  virtual void *alloc(size_t size, int timeout_ms = 0) = 0;
  virtual void free(void *p) = 0;
  int device_id() const { return device_id_;}
  void set_device_id(int device_id) { device_id_ = device_id;}
 protected:
  int device_id_ = -1;
  std::mutex mutex_;
};

class CpuAllocator : public MemoryAllocator {
 public:
  CpuAllocator() : MemoryAllocator(-1) {}
  ~CpuAllocator() = default;

  void *alloc(size_t size, int timeout_ms = 0) override;
  void free(void* p) override;
};

class MluAllocator : public MemoryAllocator {
 public:
  MluAllocator(int device_id = 0) : MemoryAllocator(device_id) {}
  ~MluAllocator() = default;

  void *alloc(size_t size, int timeout_ms = 0) override;
  void free(void* p) override;
};

// helper funcs
std::shared_ptr<void> cnMemAlloc(size_t size, std::shared_ptr<MemoryAllocator> allocator);
std::shared_ptr<void> cnCpuMemAlloc(size_t size);
std::shared_ptr<void> cnMluMemAlloc(size_t size, int device_id);


}  // namespace cnstream

#endif  // CNSTREAM_ALLOCATOR_HPP_
