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

#include <chrono>
#include <ctime>
#include <memory>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "connector.hpp"
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
          LOG(FATAL) << kind[id] << " is blocking! Thread: " << i << " is not end!" << std::endl;
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

  Connector* con = new Connector(1, 20);
  Conveyor conveyor(con, 20);

  CNFrameInfoPtr data = cnstream::CNFrameInfo::Create(std::to_string(0));

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

  delete con;
}

TEST(CoreConveyor, GetBufferSize) {
  uint32_t conveyor_num = 1;
  size_t conveyor_capacity = 20;
  Connector* connect = new Connector(conveyor_num, conveyor_capacity);
  Conveyor* conveyor = connect->GetConveyor(0);

  uint32_t store_num = rand_r(&seed_conveyor) % conveyor_capacity;
  for (uint32_t i = 0; i < store_num; ++i) {
    auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
    conveyor->PushDataBuffer(data);
  }
  EXPECT_EQ(conveyor->GetBufferSize(), store_num);
  delete connect;
}

TEST(CoreConveyor, PushPopDataBuffer) {
  uint32_t conveyor_num = 2;
  Connector* connect = new Connector(conveyor_num);
  Conveyor* conveyor = connect->GetConveyor(conveyor_num - 1);
  std::shared_ptr<CNFrameInfo> sdata = cnstream::CNFrameInfo::Create(std::to_string(0));
  conveyor->PushDataBuffer(sdata);
  auto rdata = conveyor->PopDataBuffer();
  EXPECT_EQ(sdata.get(), rdata.get());
  delete connect;
}

}  // namespace cnstream
