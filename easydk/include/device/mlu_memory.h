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

/**
 * @file mlu_memory.h
 *
 * This file contains a declaration of the MluMemoryOp class.
 */

#ifndef EDK_MLU_MEMORY_H_
#define EDK_MLU_MEMORY_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include "cxxutil/exception.h"

namespace edk {

TOOLKIT_REGISTER_EXCEPTION(MluMemory);

/**
 * @brief MluMemory is a MLU memory helper class.
 * @note It provides a easy way to manage memory on MLU.
 */
class MluMemory {
 public:
  using Deleter = std::function<void(void *memory, int device_id)>;

  /**
   * @brief Construct a new Mlu Memory object
   *
   * @param memory_size[in] Memory size in bytes
   * @param device_id[in] memory on which device
   */
  explicit MluMemory(size_t memory_size, int device_id = 0);

  /**
   * @brief Construct a new Mlu Memory object with raw MLU memory
   *
   * @param mlu_memory[in] raw pointer
   * @param memory_size[in] Memory size in bytes
   * @param d[in] A function to handle memory when destruct
   * @param device_id[in] memory on which device
   */
  MluMemory(void *mlu_memory, size_t memory_size, Deleter d, int device_id = 0);

  /**
   * @brief A destructor
   */
  ~MluMemory();

  /**
   * @brief Get mutable raw pointer
   *
   * @return raw pointer
   */
  void *MutableData();

  /**
   * @brief Get const raw pointer
   *
   * @return raw pointer
   */
  const void *Data() const noexcept;

  /**
   * @brief Get size of MLU memory
   *
   * @return memory size in bytes
   */
  size_t MemorySize() const noexcept { return len_; }

  /**
   * @brief Get device id
   *
   * @return device id
   */
  int DeviceId() const noexcept { return device_id_; }

  /**
   * @brief Copy data from host to device
   *
   * @param cpu_src[in] Copy source, data on CPU
   * @param memory_size[in] Memory size in bytes
   */
  void CopyFrom(void *cpu_src, size_t memory_size);

  /**
   * @brief Copy data from device to device
   *
   * @param mlu_src[in] Copy source, data on MLU
   */
  void CopyFrom(const MluMemory &mlu_src);

  /**
   * @brief Copy data from device to host
   *
   * @param cpu_dst[in] Copy destination, memory on CPU
   * @param memory_size[in] Memory size in bytes
   */
  void CopyTo(void *cpu_dst, size_t memory_size) const;

 private:
  MluMemory() = delete;
  // disable copy and assign
  MluMemory(const MluMemory &) = delete;
  MluMemory &operator=(const MluMemory &) = delete;
  void LazyMalloc();

  void *data_ = nullptr;
  size_t len_ = 0;
  Deleter deleter_;

  int device_id_ = 0;
};  // class MluMemory

/**
 * @brief MluMemoryPool is a MLU memory helper class.
 * @note It provides a easy way to manage memory on MLU.
 */
class MluMemoryPool {
 public:
  /**
   * @brief Construct a new Mlu Memory Pool object
   *
   * @param memory_size[in] Memory size in bytes
   * @param buffer_num[in] number of memory cached in pool
   * @param device_id[in] memory on which device
   */
  MluMemoryPool(size_t memory_size, size_t buffer_num, int device_id = 0);

  /**
   * @brief A destructor
   * @note wait until all MluMemory requested is released
   */
  ~MluMemoryPool();

  /**
   * @brief Request MluMemory from pool, wait for timeout_ms if pool is empty
   *
   * @param timeout_ms[in] wait timeout in milliseconds
   * @return a shared pointer to MluMemory
   */
  std::shared_ptr<MluMemory> RequestMemory(int timeout_ms = -1) noexcept;

  /**
   * @brief Get size of MLU memory
   *
   * @return memory size in bytes
   */
  size_t MemorySize() const noexcept { return memory_size_; }

  /**
   * @brief Get how many pieces of MLU memory cached
   *
   * @return number of memory cached
   */
  size_t BufferNum() const noexcept { return buffer_num_; }

 private:
  std::queue<void *> cache_;
  std::mutex q_mutex_;
  std::condition_variable empty_cond_;
  size_t memory_size_;
  size_t buffer_num_;
  int device_id_;
  std::atomic<bool> running_{false};
};

}  // namespace edk

#endif  // EDK_MLU_MEMORY_H_
