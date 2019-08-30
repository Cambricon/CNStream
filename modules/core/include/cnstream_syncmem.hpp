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

#ifndef CNSTREAM_SYNCMEM_HPP_
#define CNSTREAM_SYNCMEM_HPP_
#include <cstddef>

namespace cnstream {

void CNStreamMallocHost(void** ptr, size_t size);

inline void CNStreamFreeHost(void* ptr) { free(ptr); }

/*
  @attention
    CNSyncedMemory::Head() will always return CNSyncedMemory::UNINITIALIZED when size is 0.
 */
class CNSyncedMemory {
 public:
  CNSyncedMemory();
  explicit CNSyncedMemory(size_t size);
  CNSyncedMemory(size_t size, int mlu_dev_id, int mlu_ddr_chn);
  ~CNSyncedMemory();
  const void* GetCpuData();
  void SetCpuData(void* data);
  const void* GetMluData();
  void SetMluData(void* data);
  void SetMluDevContext(int dev_id, int ddr_chn = 0);
  int GetMluDevId() const;
  int GetMluDdrChnId() const;
  void* GetMutableCpuData();
  void* GetMutableMluData();
  enum SyncedHead { UNINITIALIZED, HEAD_AT_CPU, HEAD_AT_MLU, SYNCED };
  SyncedHead GetHead() const { return head_; }
  size_t GetSize() const { return size_; }

 private:
  void ToCpu();
  void ToMlu();

  void* cpu_ptr_ = nullptr;
  void* mlu_ptr_ = nullptr;

#ifdef TEST
 public:
#endif
  /*
    Allocate memory by CNSyncedMemory if true.
   */
  bool own_cpu_data_ = false;
  bool own_mlu_data_ = false;

#ifdef TEST
 private:
#endif
  /*
    Memory description
   */
  SyncedHead head_ = UNINITIALIZED;
  size_t size_ = 0;

  /*
    Mlu device context
   */
  int dev_id_ = 0;
  int ddr_chn_ = 0;

  DISABLE_COPY_AND_ASSIGN(CNSyncedMemory);
};  // class CNSyncedMemory

}  // namespace cnstream

#endif  // CNSTREAM_SYNCMEM_HPP_
