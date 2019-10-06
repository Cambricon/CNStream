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
#include <gtest/gtest.h>
#include <chrono>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include "cnstream_frame.hpp"

#include "cnstream_syncmem.hpp"

static struct {
  void* cpu_ptr = NULL;
  void* mlu_ptr = NULL;
  bool used_cpu = false;
  bool used_mlu = false;
} g_last_data;

TEST(CoreSyncedMem, SyncedMem) {
  cnrtRet_t ret = cnrtInit(0);
  if (CNRT_RET_SUCCESS != ret) {
    LOG(WARNING) << "CnrtInit failed. error code:" << ret;
  }
  time_t t;
  t = time(NULL);
  // std::cout << std::put_time(std::localtime(&t), "%Y-%m-%d %H.%M.%S") << std::endl;

  std::default_random_engine random_engine(t);
  std::uniform_int_distribution<size_t> memory_random_number_generator(0, 64 * 1024);
  std::uniform_int_distribution<int> ddr_random_number_generator(0, 3);

  size_t size = memory_random_number_generator(random_engine);
  auto memory = std::make_shared<cnstream::CNSyncedMemory>(size);
  ASSERT_EQ(memory->GetSize(), size);

  std::vector<std::function<void()>> funcs;
  funcs.push_back([&] {
    cnstream::CNSyncedMemory::SyncedHead old_head = memory->GetHead();
    memory->GetCpuData();
    if (cnstream::CNSyncedMemory::HEAD_AT_MLU == old_head || cnstream::CNSyncedMemory::SYNCED == old_head)
      ASSERT_EQ(cnstream::CNSyncedMemory::SYNCED, memory->GetHead());
    else if (memory->GetSize() == 0)
      ASSERT_EQ(cnstream::CNSyncedMemory::UNINITIALIZED, memory->GetHead());
    else
      ASSERT_EQ(cnstream::CNSyncedMemory::HEAD_AT_CPU, memory->GetHead());
  });
  funcs.push_back([&] {
    cnstream::CNSyncedMemory::SyncedHead old_head = memory->GetHead();
    memory->GetMluData();
    if (cnstream::CNSyncedMemory::HEAD_AT_CPU == old_head || cnstream::CNSyncedMemory::SYNCED == old_head)
      ASSERT_EQ(cnstream::CNSyncedMemory::SYNCED, memory->GetHead());
    else if (memory->GetSize() == 0)
      ASSERT_EQ(cnstream::CNSyncedMemory::UNINITIALIZED, memory->GetHead());
    else
      ASSERT_EQ(cnstream::CNSyncedMemory::HEAD_AT_MLU, memory->GetHead());
  });
  funcs.push_back([&] {
    cnstream::CNSyncedMemory::SyncedHead old_head = memory->GetHead();
    memory->GetMutableCpuData();
    if (cnstream::CNSyncedMemory::HEAD_AT_MLU == old_head || cnstream::CNSyncedMemory::SYNCED == old_head)
      ASSERT_EQ(cnstream::CNSyncedMemory::SYNCED, memory->GetHead());
    else if (memory->GetSize() == 0)
      ASSERT_EQ(cnstream::CNSyncedMemory::UNINITIALIZED, memory->GetHead());
    else
      ASSERT_EQ(cnstream::CNSyncedMemory::HEAD_AT_CPU, memory->GetHead());
  });
  funcs.push_back([&] {
    cnstream::CNSyncedMemory::SyncedHead old_head = memory->GetHead();
    memory->GetMutableMluData();
    if (cnstream::CNSyncedMemory::HEAD_AT_CPU == old_head || cnstream::CNSyncedMemory::SYNCED == old_head)
      ASSERT_EQ(cnstream::CNSyncedMemory::SYNCED, memory->GetHead());
    else if (memory->GetSize() == 0)
      ASSERT_EQ(cnstream::CNSyncedMemory::UNINITIALIZED, memory->GetHead());
    else
      ASSERT_EQ(cnstream::CNSyncedMemory::HEAD_AT_MLU, memory->GetHead());
  });
  funcs.push_back([&] {
    if (g_last_data.used_cpu) {
      free(g_last_data.cpu_ptr);
      g_last_data.cpu_ptr = NULL;
      g_last_data.used_cpu = false;
    }
    if (g_last_data.used_mlu) {
      assert(CNRT_RET_SUCCESS == cnrtFree(g_last_data.mlu_ptr));
      g_last_data.mlu_ptr = NULL;
      g_last_data.used_mlu = false;
    }
    size_t size = memory_random_number_generator(random_engine);
    memory = std::make_shared<cnstream::CNSyncedMemory>(size);
    ASSERT_EQ(memory->GetSize(), size);
    ASSERT_EQ(cnstream::CNSyncedMemory::UNINITIALIZED, memory->GetHead());
  });
  funcs.push_back([&] {
    if (memory->GetSize() == 0) return;
    if (g_last_data.used_cpu) {
      free(g_last_data.cpu_ptr);
      g_last_data.cpu_ptr = NULL;
    }
    g_last_data.cpu_ptr = malloc(memory->GetSize());
    g_last_data.used_cpu = true;
    memory->SetCpuData(g_last_data.cpu_ptr);
    ASSERT_EQ(memory->GetHead(), cnstream::CNSyncedMemory::HEAD_AT_CPU);
    ASSERT_EQ(memory->own_cpu_data_, false);
  });
  funcs.push_back([&] {
    if (memory->GetSize() == 0) return;
    if (g_last_data.used_mlu) {
      assert(CNRT_RET_SUCCESS == cnrtFree(g_last_data.mlu_ptr));
      g_last_data.mlu_ptr = NULL;
    }
    memory->SetMluDevContext(0, ddr_random_number_generator(random_engine));
    CALL_CNRT_BY_CONTEXT(cnrtMalloc(&g_last_data.mlu_ptr, memory->GetSize()), memory->GetMluDevId(),
                         memory->GetMluDdrChnId());
    g_last_data.used_mlu = true;
    memory->SetMluData(g_last_data.mlu_ptr);
    ASSERT_EQ(memory->GetHead(), cnstream::CNSyncedMemory::HEAD_AT_MLU);
    ASSERT_EQ(memory->own_mlu_data_, false);
  });

  auto __start_test_time = std::chrono::high_resolution_clock::now();
  auto __last_time = __start_test_time;
  double __total_test_time = 0;  // ms
  size_t total_test_function_number = 0;
  std::uniform_int_distribution<> func_idx_random_number_generator(0, funcs.size() - 1);
  while (__total_test_time < 1000 * 2) {
    total_test_function_number++;
    funcs[func_idx_random_number_generator(random_engine)]();
    auto __now_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> __diff_time = __now_time - __last_time;
    if (__diff_time.count() >= 100 * 5) {
      __last_time = __now_time;
      std::cout << "[Test count] [" << total_test_function_number << "]" << std::endl;
    }
    std::chrono::duration<double, std::milli> __total_diff_time = __now_time - __start_test_time;
    __total_test_time = __total_diff_time.count();
  }

  std::cout << "[Total Test count] [" << total_test_function_number << "]" << std::endl;

  t = time(NULL);
  //  std::cout << std::put_time(std::localtime(&t), "%Y-%m-%d %H.%M.%S") << std::endl;
}
