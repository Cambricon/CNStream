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
 *  This file contains a declaration of the CNStream memory allocator.
 */
namespace cnstream {

/**
 * @class IDataDeallocator
 *
 * @brief IDataDeallocator is an abstract class of deallocator for the CNDecoder buffer.
 */
class IDataDeallocator {
 public:
  /*!
   * @brief Destructs the base object.
   *
   * @return No return value.
   */
  virtual ~IDataDeallocator() {}
};

/*!
 * @class MluDeviceGuard
 *
 * @brief MluDeviceGuard is a class for setting current thread's device handler.
 */
class MluDeviceGuard : public NonCopyable {
 public:
  /*!
   * @brief Sets the device handler with the given device ordinal.
   *
   * @param[in] device_id The device ordinal to retrieve.
   *
   * @return No return value.
   */
  explicit MluDeviceGuard(int device_id);
  /*!
   * @brief Destructs an object.
   *
   * @return No return value.
   */
  ~MluDeviceGuard();

 private:
  int device_id_ = 0;
};

/*!
 * @brief Allocates CPU memory with the given size.
 *
 * @param[in] size The size needs to be allocated.
 *
 * @return The shared pointer to the allocated memory.
 *
 * @note Because of CNCodec's constraints, the given size will be aligned up to 4096 inside this
 *       function before doing allocation.
 */
std::shared_ptr<void> cnCpuMemAlloc(size_t size);
/*!
 * @brief Allocates MLU memory with the given size at specific device .
 *
 * @param[in] size The size needs to be allocated.
 * @param[in] device_id The device ordinal.
 *
 * @return The shared pointer to the allocated memory.
 *
 * @note Because of CNCodec's constraints, the given size will be aligned up to 4096 inside this
 *       function before doing allocation.
 */
std::shared_ptr<void> cnMluMemAlloc(size_t size, int device_id);


}  // namespace cnstream

#endif  // CNSTREAM_ALLOCATOR_HPP_
