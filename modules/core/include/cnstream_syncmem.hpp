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
 * This file contains a declaration of class CNSyncedMemory.
 */

#include <cstddef>

namespace cnstream {

/**
 * Alloc data on host.
 *
 * @param ptr Output data pointer.
 * @param size Size to allocate.
 */
void CNStreamMallocHost(void** ptr, size_t size);

/**
 * Free data allocated by CNStreamMallocHost.
 *
 * @param ptr The data address to be freed.
 */
inline void CNStreamFreeHost(void* ptr) { free(ptr); }

/*
  @attention
 */
/**
 * @class CNSyncedMemory
 *
 * CNSyncedMemory used to sync memory between CPU and MLU.
 *
 * @note CNSyncedMemory::Head() will always return CNSyncedMemory::UNINITIALIZED when size is 0.
 */
class CNSyncedMemory {
 public:
  /**
   * Constructor.
   */
  CNSyncedMemory();
  /**
   * Constructor.
   *
   * @param size Size of memory.
   */
  explicit CNSyncedMemory(size_t size);
  /**
   * Constructor.
   *
   * @param size Size of memory.
   * @param mlu_dev_id MLU device id, device id increasing from 0.
   * @param mlu_ddr_chn MLU DDR channel id, greater than or equal to 0 and less than 4. Specify which piece of ddr the
   * memory allocated on.
   */
  CNSyncedMemory(size_t size, int mlu_dev_id, int mlu_ddr_chn);
  ~CNSyncedMemory();
  /**
   * Get CPU data.
   *
   * @return Return CPU data pointer.
   *
   * @note If size is 0, always return nullptr.
   */
  const void* GetCpuData();
  /**
   * Set CPU data.
   *
   * @param data Data pointer on CPU.
   *
   * @return void.
   */
  void SetCpuData(void* data);
  /**
   * Get MLU data.
   *
   * @return Return MLU data pointer.
   *
   * @note If size is 0, always return nullptr.
   */
  const void* GetMluData();
  /**
   * Set MLU data.
   *
   * @param data Data pointer on MLU
   */
  void SetMluData(void* data);
  /**
   * Set MLU device context
   *
   * @param dev_id MLU device id, device id increasing from 0.
   * @param ddr_chn MLU ddr channel id, greater than or equal to 0 and less than 4. Specify which piece of ddr the
   * memory allocated on.
   *
   * @note Do this before all getter and setter.
   */
  void SetMluDevContext(int dev_id, int ddr_chn = 0);
  /**
   * Get MLU device id.
   *
   * @return Return MLU memory allocated on which device.
   */
  int GetMluDevId() const;
  /**
   * Get MLU ddr channel id.
   *
   * @return Return MLU memory allocated on which ddr channel.
   */
  int GetMluDdrChnId() const;
  /**
   * Get mutable CPU data.
   *
   * @return Return CPU data pointer.
   */
  void* GetMutableCpuData();
  /**
   * Get mutable MLU data.
   *
   * @return Return MLU data pointer.
   */
  void* GetMutableMluData();
  /**
   * Synced head.
   */
  enum SyncedHead {
    UNINITIALIZED,  ///> Means memory has not been allocated.
    HEAD_AT_CPU,    ///> Means data update to CPU and has not been synced to MLU yet.
    HEAD_AT_MLU,    ///> Means data update to MLU and has not been synced to CPU yet.
    SYNCED          ///> Means data has been synced to CPU and MLU.
  };
  /**
   * Get synced head.
   *
   * @return Return synced head.
   */
  SyncedHead GetHead() const { return head_; }
  /**
   * Get data bytes.
   *
   * @return Return data bytes.
   */
  size_t GetSize() const { return size_; }

 private:
  /**
   * Memory sync to CPU.
   */
  void ToCpu();
  /**
   * Memory sync to MLU.
   */
  void ToMlu();

  void* cpu_ptr_ = nullptr;  ///> CPU data pointer.
  void* mlu_ptr_ = nullptr;  ///> MLU data pointer.

#ifdef TEST
 public:
#endif
  /*
    Allocate memory by CNSyncedMemory if true.
   */
  bool own_cpu_data_ = false;  ///> Whether CPU data is allocated by SyncedMemory itself.
  bool own_mlu_data_ = false;  ///> Whether MLU data is allocated by SyncedMemory itself.

#ifdef TEST
 private:
#endif
  SyncedHead head_ = UNINITIALIZED;  ///> Identify which device is the data synchronized on.
  size_t size_ = 0;                  ///> Data size.

  int dev_id_ = 0;   ///> Ordinal MLU device id.
  int ddr_chn_ = 0;  ///> Ordinal MLU ddr channel id. [0, 4).

  DISABLE_COPY_AND_ASSIGN(CNSyncedMemory);
};  // class CNSyncedMemory

}  // namespace cnstream

#endif  // CNSTREAM_SYNCMEM_HPP_
