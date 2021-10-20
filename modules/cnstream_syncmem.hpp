/*********************************************************************************************************
 * All modification made by Cambricon Corporation: © 2018--2019 Cambricon Corporation
 * All rights reserved.
 * All other contributions:
 * Copyright (c) 2014--2018, the respective contributors
 * All rights reserved.
 * For the list of contributors go to https://github.com/BVLC/caffe/blob/master/CONTRIBUTORS.md
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Intel Corporation nor the names of its contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************************************************/

#ifndef CNSTREAM_SYNCMEM_HPP_
#define CNSTREAM_SYNCMEM_HPP_

/**
 * @file cnstream_syncmem.hpp
 *
 * This file contains a declaration of the CNSyncedMemory class.
 */

#include <cstddef>
#include <mutex>

#include "cnrt.h"
#include "cnstream_common.hpp"
#include "cnstream_logging.hpp"

#define CNS_CNRT_CHECK(__EXPRESSION__)                                                                        \
  do {                                                                                                        \
    cnrtRet_t ret = (__EXPRESSION__);                                                                         \
    LOGF_IF(FRAME, CNRT_RET_SUCCESS != ret) << "Call [" << #__EXPRESSION__ << "] failed, error code: " << ret; \
  } while (0)

#define CALL_CNRT_BY_CONTEXT(__EXPRESSION__, __DEV_ID__, __DDR_CHN__)          \
  do {                                                                         \
    int dev_id = (__DEV_ID__);                                                 \
    cnrtDev_t dev;                                                             \
    cnrtChannelType_t ddr_chn = static_cast<cnrtChannelType_t>((__DDR_CHN__)); \
    CNS_CNRT_CHECK(cnrtGetDeviceHandle(&dev, dev_id));                         \
    CNS_CNRT_CHECK(cnrtSetCurrentDevice(dev));                                 \
    if (ddr_chn >= 0)                                                          \
      CNS_CNRT_CHECK(cnrtSetCurrentChannel(ddr_chn));                          \
    CNS_CNRT_CHECK(__EXPRESSION__);                                            \
  } while (0)

namespace cnstream {

/**
 * @class CNSyncedMemory
 *
 * @brief CNSyncedMemory is a class synchronizing memory between CPU and MLU.
 *
 * If the data on MLU is the latest, the data on CPU should be synchronized before processing the data on CPU.
 * Vice versa, if the data on CPU is the latest, the data on MLU should be synchronized before processing
 * the data on MLU.
 *
 * @note CNSyncedMemory::Head() always returns SyncedHead::UNINITIALIZED when memory size is 0.
 */
class CNSyncedMemory : private NonCopyable {
 public:
  /**
   * @brief Constructor to construct synchronized memory object.
   *
   * @param[in] size The size of the memory.
   *
   * @return No return value.
   */
  explicit CNSyncedMemory(size_t size);
  /**
   * @brief Constructor to construct synchronized memory object.
   *
   * @param[in] size The size of the memory.
   * @param[in] mlu_dev_id MLU device ID that is incremented from 0.
   * @param[in] mlu_ddr_chn The MLU DDR channel that is greater than or equal to 0, and is less
   *                    than 4. It specifies which piece of DDR channel the memory allocated on.
   *
   * @return No return value.
   */
  explicit CNSyncedMemory(size_t size, int mlu_dev_id, int mlu_ddr_chn = -1);
  /**
   * @brief Destructor to destruct synchronized memory object.
   *
   * @return No return value.
   */
  ~CNSyncedMemory();
  /**
   * @brief Gets the CPU data.
   *
   * @param No return value.
   *
   * @return Returns the CPU data pointer.
   *
   * @note If the size is 0, nullptr is always returned.
   */
  const void* GetCpuData();
  /**
   * @brief Sets the CPU data.
   *
   * @param[in] data The data pointer on CPU.
   *
   * @return Void.
   */
  void SetCpuData(void* data);
  /**
   * @brief Gets the MLU data.
   *
   * @return Returns the MLU data pointer.
   *
   * @note If the size is 0, nullptr is always returned.
   */
  const void* GetMluData();
  /**
   * @brief Sets the MLU data.
   *
   * @param[out] data The data pointer on MLU.
   *
   * @return No return value.
   */
  void SetMluData(void* data);
  /**
   * @brief Sets the MLU device context.
   *
   * @param[in] dev_id The MLU device ID that is incremented from 0.
   * @param[in] ddr_chn The MLU DDR channel ID that is greater than or equal to 0, and less than
   *                4. It specifies which piece of DDR channel the memory is allocated on.
   *
   * @return No return value.
   *
   * @note You need to call this API before all getters and setters.
   */
  void SetMluDevContext(int dev_id, int ddr_chn = -1);
  /**
   * @brief Gets the MLU device ID.
   *
   * @return Returns the ID of the device that the MLU memory is allocated on.
   */
  int GetMluDevId() const;
  /**
   * @brief Gets the channel ID of the MLU DDR.
   *
   * @return Returns the DDR channel ID that the MLU memory is allocated on.
   */
  int GetMluDdrChnId() const;
  /**
   * @brief Gets the mutable CPU data.
   *
   * @return Returns the CPU data pointer.
   */
  void* GetMutableCpuData();
  /**
   * @brief Gets the mutable MLU data.
   *
   * @return Returns the MLU data pointer.
   */
  void* GetMutableMluData();
  /**
   * @enum SyncedHead
   *
   * @brief An enumerator describing the synchronization status.
   */
  enum class SyncedHead {
    UNINITIALIZED,  ///< The memory is not allocated.
    HEAD_AT_CPU,    ///< The data is updated to CPU but is not synchronized to MLU yet.
    HEAD_AT_MLU,    ///< The data is updated to MLU but is not synchronized to CPU yet.
    SYNCED          ///< The data is synchronized to both CPU and MLU.
  };
  /**
   * @brief Gets synchronization status.
   *
   * @return Returns synchronization status .
   *
   * @see SyncedHead.
   */
  SyncedHead GetHead() const { return head_; }
  /**
   * @brief Gets data bytes.
   *
   * @return Returns data bytes.
   */
  size_t GetSize() const { return size_; }

#ifndef UNIT_TEST
 private:  // NOLINT
#endif
  /**
   * Allocates memory by ``CNSyncedMemory`` if a certain condition is true.
   */
  bool own_cpu_data_ = false;  ///< Whether CPU data is allocated by ``SyncedMemory``.
  bool own_mlu_data_ = false;  ///< Whether MLU data is allocated by ``SyncedMemory``.

 private:
  /**
   * Synchronizes the memory data to CPU.
   */
  void ToCpu();
  /**
   * Synchronizes the memory data to MLU.
   */
  void ToMlu();

  void* cpu_ptr_ = nullptr;  ///< CPU data pointer.
  void* mlu_ptr_ = nullptr;  ///< MLU data pointer.
  SyncedHead head_ = SyncedHead::UNINITIALIZED;  ///< Identifies which device data is synchronized on.
  size_t size_ = 0;                  ///< The data size.

  int dev_id_ = -1;   ///< Ordinal MLU device ID.
  int ddr_chn_ = -1;  ///< Ordinal MLU DDR channel ID. The value should be [0, 4).

  mutable std::mutex mutex_;
};  // class CNSyncedMemory

}  // namespace cnstream

#endif  // CNSTREAM_SYNCMEM_HPP_
