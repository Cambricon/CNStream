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

#include "cnstream_logging.hpp"

#include <chrono>
#include <ctime>
#include <memory>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "conveyor.hpp"

namespace cnstream {

uint32_t seed_conveyor;

int flag[80];
const char* kind[80];

void ThreadFuncPushDataBuf(Conveyor* conveyor, CNFrameInfoPtr data, int id) {
  kind[id] = "pushDataBuf";
  conveyor->PushDataBuffer(data);
  flag[id]++;
}

void ThreadFuncPopDataBuf(Conveyor* conveyor, int id) {
  kind[id] = "popdataBuf";
  conveyor->PopDataBuffer();
  flag[id]++;
}

void ThreadFuncState(int id) {
  int cnt = 0;
  while (1) {
    bool f = true;
    std::this_thread::sleep_for(std::chrono::duration<double, std::ratio<1>>(1));
    for (int i = 0; i < id; i++) {
      if (flag[i] == 0) {
        if (cnt > 10) {
          LOGF(COREUNITEST) << kind[id] << " is blocking! Thread: " << i << " is not end!" << std::endl;
        }
        std::cout << kind[id] << " is blocking! Thread: " << i << " is not end!" << std::endl;
        f = false;
      }
    }
    if (f) break;
    cnt++;
  }
}

TEST(CoreConveyor, MultiThreadPushPop) {
  int data_buf_num = 0;
  std::thread* thread_ids[80];
  memset(flag, 0, sizeof(flag));

  Conveyor conveyor(20);
  CNFrameInfoPtr data = CNFrameInfo::Create(std::to_string(0));

  int id = 0;
  seed_conveyor = (uint32_t)time(0);
  srand(time(nullptr));

  while (id < 30) {
    switch (rand_r(&seed_conveyor) % 2) {
      case 0:
        thread_ids[id] = new std::thread(ThreadFuncPushDataBuf, &conveyor, data, id);
        data_buf_num++;
        break;
      case 1:
        thread_ids[id] = new std::thread(ThreadFuncPopDataBuf, &conveyor, id);
        data_buf_num--;
        break;
      default:
        break;
    }
    id++;
  }

  while (data_buf_num < 0) {
    thread_ids[id] = new std::thread(ThreadFuncPushDataBuf, &conveyor, data, id);
    data_buf_num++;
    id++;
  }

  thread_ids[id] = new std::thread(ThreadFuncState, id - 1);
  id++;

  for (int i = 0; i < id; i++) thread_ids[i]->join();
  for (int i = 0; i < id; i++) delete thread_ids[i];
}

TEST(CoreConveyor, GetBufferSize) {
  size_t conveyor_capacity = 20;
  Conveyor* conveyor = new Conveyor(conveyor_capacity);

  uint32_t store_num = rand_r(&seed_conveyor) % conveyor_capacity;
  for (uint32_t i = 0; i < store_num; ++i) {
    auto data = CNFrameInfo::Create(std::to_string(0));
    conveyor->PushDataBuffer(data);
  }
  EXPECT_EQ(conveyor->GetBufferSize(), store_num);
  delete conveyor;
}

TEST(CoreConveyor, PushPopDataBuffer) {
  uint32_t conveyor_num = 2;
  Conveyor* conveyor = new Conveyor(conveyor_num);
  std::shared_ptr<CNFrameInfo> sdata = CNFrameInfo::Create(std::to_string(0));
  conveyor->PushDataBuffer(sdata);
  auto rdata = conveyor->PopDataBuffer();
  EXPECT_EQ(sdata.get(), rdata.get());
  delete conveyor;
}

TEST(CoreConveyor, PushDataFull) {
  size_t max_size = 10;
  Conveyor* conveyor = new Conveyor(max_size);
  // When data queue is full, conveyor will drop one data from the front.
  for (uint32_t i = 0; i < max_size + 1; i++) {
    std::shared_ptr<CNFrameInfo> sdata = CNFrameInfo::Create(std::to_string(0));
    conveyor->PushDataBuffer(sdata);
  }
  delete conveyor;
}

TEST(CoreConveyor, PopAllData) {
  size_t max_size = 10;
  Conveyor* conveyor = new Conveyor(max_size);
  std::vector<std::shared_ptr<CNFrameInfo>> sdata_vec;
  std::vector<std::shared_ptr<CNFrameInfo>> rdata_vec;
  // When data queue is full, conveyor will drop one data from the front.
  for (uint32_t i = 0; i < max_size + 1; i++) {
    std::shared_ptr<CNFrameInfo> sdata = CNFrameInfo::Create(std::to_string(0));
    sdata_vec.push_back(sdata);
    conveyor->PushDataBuffer(sdata);
  }
  rdata_vec = conveyor->PopAllDataBuffer();

  EXPECT_EQ(rdata_vec.size(), max_size);
  for (uint32_t i = 0; i < max_size; i++) {
    EXPECT_EQ(sdata_vec[i], rdata_vec[i]);
  }
  delete conveyor;
}

}  // namespace cnstream
