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

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <fcntl.h>
#include <semaphore.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <memory>
#include <string>
#include <vector>

#include "client_handler.hpp"
#include "cnrt.h"
#include "cnstream_pipeline.hpp"
#include "module_ipc.hpp"
#include "server_handler.hpp"

namespace cnstream {

#define TEST_SHARED_MEM_SZIE 128
#define ALIGN_(addr, boundary) (((u32_t)(addr) + (boundary)-1) & ~((boundary)-1))

static const int width = 1280;
static const int height = 720;

static const int g_dev_id = 0;
static const int g_ddr_channel = 0;
static const char* fake_str = "this is moduleipc test.";

// parent processor
static void ClientProcess(ModuleParamSet param) {
  std::shared_ptr<ModuleIPC> client = std::make_shared<ModuleIPC>("client");
  CNS_CNRT_CHECK(cnrtInit(0));

  // fake string(this is moduleipc test) to frame and post to server
  size_t nbytes = width * height * 3;
  nbytes = ALIGN_(nbytes, 64 * 1024);  // align to 64kb
  void* frame_data = nullptr;
  CALL_CNRT_BY_CONTEXT(cnrtMalloc(&frame_data, nbytes), g_dev_id, g_ddr_channel);
  CALL_CNRT_BY_CONTEXT(cnrtMemset(frame_data, 0, nbytes), g_dev_id, g_ddr_channel);
  CALL_CNRT_BY_CONTEXT(cnrtMemcpy(frame_data, reinterpret_cast<void*>(const_cast<char*>(fake_str)), strlen(fake_str),
                                  CNRT_MEM_TRANS_DIR_HOST2DEV),
                       g_dev_id, g_ddr_channel);
  // open client
  client->Open(param);

  // fake normal data with test string, and process
  std::string stream_id = std::to_string(0);
  std::shared_ptr<CNFrameInfo> data = CNFrameInfo::Create(stream_id);
  if (data == nullptr) {
    std::cout << "frame create error\n";
    return;
  }
  data->frame.flags = 0;
  data->channel_idx = 0;
  data->frame.frame_id = 0;
  data->frame.timestamp = 0;
  data->frame.width = width;
  data->frame.height = height;
  data->frame.ptr_mlu[0] = frame_data;
  data->frame.ptr_mlu[1] = reinterpret_cast<void*>(reinterpret_cast<int64_t>(frame_data) + width * height);
  data->frame.stride[0] = width;
  data->frame.stride[1] = width;
  data->frame.ctx.ddr_channel = g_ddr_channel;
  data->frame.ctx.dev_id = g_dev_id;
  data->frame.ctx.dev_type = DevContext::DevType::MLU;
  data->frame.fmt = CN_PIXEL_FORMAT_YUV420_NV12;
  data->frame.CopyToSyncMem();
  client->Process(data);

  // fake eos data and process
  std::shared_ptr<CNFrameInfo> eos_data = CNFrameInfo::Create(stream_id);
  if (eos_data == nullptr) {
    std::cout << "frame create error\n";
    return;
  }

  eos_data->frame.flags = 1;
  eos_data->channel_idx = 0;
  eos_data->frame.frame_id = 0;
  eos_data->frame.timestamp = 0;
  client->Process(eos_data);

  std::shared_ptr<IPCClientHandler> client_handler =
      std::dynamic_pointer_cast<IPCClientHandler>(client->GetIPCHandler());
  // wait server post exit info
  while (!client_handler->GetServerState()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  client->Close();
  CALL_CNRT_BY_CONTEXT(cnrtFree(frame_data), g_dev_id, g_ddr_channel);
}

static void ServerProcess(ModuleParamSet param, std::string* recvd_string) {
  if (!recvd_string) return;
  cnrtInit(0);
  std::shared_ptr<CNFrameInfo> data = nullptr;
  void* frame_data = nullptr;
  size_t nbytes = width * height * 3;
  nbytes = ALIGN_(nbytes, 64 * 1024);  // align to 64kb
  frame_data = malloc(nbytes);
  memset(frame_data, 0, nbytes);

  std::shared_ptr<ModuleIPC> server = std::make_shared<ModuleIPC>("server");
  server->SetChannelCount(1);
  server->Open(param);
  auto handler = server->GetIPCHandler();
  std::shared_ptr<IPCServerHandler> server_handler = std::dynamic_pointer_cast<IPCServerHandler>(handler);
  bool recvd = false;
  while (!recvd) {
    FrameInfoPackage recv_pkg = server_handler->ReadReceivedData();
    if (!recv_pkg.stream_id.empty()) {
      recvd = true;
    } else {
      continue;
    }

    std::shared_ptr<CNFrameInfo> data = CNFrameInfo::Create(recv_pkg.stream_id);
    if (nullptr == data) {
      break;
    }

    handler->PackageToCNData(recv_pkg, data);
    if (data) {
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(frame_data, data->frame.data[0]->GetMutableMluData(), strlen(fake_str),
                                      CNRT_MEM_TRANS_DIR_DEV2HOST),
                           g_dev_id, g_ddr_channel);
      if (!(data->frame.flags & CN_FRAME_FLAG_EOS)) {
        server->PostFrameToReleaseMem(data);
        *recvd_string = reinterpret_cast<char*>(frame_data);
        recvd = true;
      }
    }
  }

  server->Close();
  free(frame_data);
}

TEST(ModuleIPC, Construct) {
  std::shared_ptr<Module> ipc = std::make_shared<ModuleIPC>("ipc-test");
  EXPECT_STREQ(ipc->GetName().c_str(), "ipc-test");
}

TEST(ModuleIPC, CheckParamSet) {
  std::shared_ptr<Module> ipc = std::make_shared<ModuleIPC>("ipc-test");
  ModuleParamSet param;
  param["invalid-param"] = "test";
  EXPECT_FALSE(ipc->CheckParamSet(param));
  param["ipc_type"] = "invalid_type";
  EXPECT_FALSE(ipc->CheckParamSet(param));
  param["ipc_type"] = "client";
  EXPECT_FALSE(ipc->CheckParamSet(param));
  param["socket_address"] = "test-check";
  EXPECT_FALSE(ipc->CheckParamSet(param));
  param["memmap_type"] = "mlu";
  EXPECT_TRUE(ipc->CheckParamSet(param));
  param["socket_address"] = "";
  EXPECT_TRUE(ipc->CheckParamSet(param));
  param["ipc_type"] = "server";
  param["socket_address"] = "test-check";
  EXPECT_TRUE(ipc->CheckParamSet(param));

  // test memmap_type
  param["ipc_type"] = "server";
  param["memmap_type"] = "mlu";
  param["device_id"] = "test";
  EXPECT_FALSE(ipc->CheckParamSet(param));
  param["device_id"] = "0";
  EXPECT_TRUE(ipc->CheckParamSet(param));
  param["max_cachedframe_size"] = "test";
  EXPECT_FALSE(ipc->CheckParamSet(param));
  param["max_cachedframe_size"] = "40";
  EXPECT_TRUE(ipc->CheckParamSet(param));
}

TEST(ModuleIPC, Open) {
  // master process do client, fork child process do server
  std::shared_ptr<Module> ipc = std::make_shared<ModuleIPC>("ipc-test");
  ModuleParamSet client_param;
  client_param["ipc_type"] = "invaild";
  EXPECT_FALSE(ipc->Open(client_param));
  client_param["ipc_type"] = "client";
  client_param["memmap_type"] = "invaild";
  EXPECT_FALSE(ipc->Open(client_param));
  client_param["ipc_type"] = "client";
  client_param["memmap_type"] = "mlu";

  client_param["socket_address"] = "";
  EXPECT_FALSE(ipc->Open(client_param));
  client_param["socket_address"] = "test-open";
  client_param["device_id"] = "0";

  ModuleParamSet server_param;
  server_param["ipc_type"] = "server";
  server_param["socket_address"] = "";
  server_param["memmap_type"] = "mlu";
  server_param["device_id"] = "0";
  EXPECT_FALSE(ipc->Open(server_param));
  server_param["socket_address"] = "test-open";

  // fork child process do server open
  pid_t pid = fork();
  if (pid < 0) {
    std::cout << "create child porcessor failed." << std::endl;
    exit(-1);
  } else if (!pid) {
    std::shared_ptr<ModuleIPC> server = std::make_shared<ModuleIPC>("server");
    EXPECT_TRUE(server->Open(server_param));
    server->Close();
    exit(0);
  } else if (pid > 0) {
    int status;
    std::shared_ptr<ModuleIPC> client = std::make_shared<ModuleIPC>("client");
    EXPECT_TRUE(client->Open(client_param));
    client->Close();
    wait(&status);
    ASSERT_EQ(0, WEXITSTATUS(status));
  }
}

TEST(ModuleIPC, Connect) {
  ModuleParamSet client_param;
  client_param["ipc_type"] = "client";
  client_param["socket_address"] = "test-connect";
  client_param["memmap_type"] = "cpu";

  ModuleParamSet server_param;
  server_param["ipc_type"] = "server";
  server_param["socket_address"] = "test-connect";
  server_param["memmap_type"] = "cpu";

  // fork child process do server open
  pid_t pid = fork();
  if (pid < 0) {
    std::cout << "create child porcessor failed." << std::endl;
    exit(-1);
  } else if (0 == pid) {
    std::shared_ptr<ModuleIPC> server = std::make_shared<ModuleIPC>("server");
    EXPECT_TRUE(server->Open(server_param));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server->GetIPCHandler()->Shutdown();
    server->Close();
    exit(0);
  } else if (pid > 0) {
    int status;
    std::shared_ptr<ModuleIPC> client = std::make_shared<ModuleIPC>("client");
    EXPECT_TRUE(client->Open(client_param));
    client->Close();
    wait(&status);
    ASSERT_EQ(0, WEXITSTATUS(status));
  }
}

TEST(ModuleIPC, SendData) {
  std::shared_ptr<ModuleIPC> ipc = std::make_shared<ModuleIPC>("server");
  ModuleParamSet server_param;
  server_param["ipc_type"] = "server";
  server_param["socket_address"] = "socket";
  server_param["memmap_type"] = "mlu";
  server_param["device_id"] = "0";

  std::shared_ptr<CNFrameInfo> data = CNFrameInfo::Create("0");
  data->channel_idx = INVALID_STREAM_IDX;
  EXPECT_FALSE(ipc->SendData(data));

  data->channel_idx = 0;
  EXPECT_FALSE(ipc->SendData(data));

  Pipeline pipeline("server-pipeline");
  ipc->SetContainer(&pipeline);
  EXPECT_FALSE(ipc->SendData(data));
}

TEST(ModuleIPC, Process) {
  ModuleParamSet client_param;
  client_param["ipc_type"] = "client";
  client_param["socket_address"] = "test-memmap_mlu";
  client_param["memmap_type"] = "cpu";

  ModuleParamSet server_param;
  server_param["ipc_type"] = "server";
  server_param["socket_address"] = "test-memmap_mlu";
  server_param["memmap_type"] = "cpu";

  int status;
  int* shared_memory;
  int shmid;
  sem_t* sem_id;
  const char* mname = "test_process2";
  const char* sem_name = "test_sem_process2";
  shmid = shm_open(mname, O_CREAT | O_RDWR, 0644);
  if (shmid == -1) {
    std::cout << "open shared memory failed\n";
  }

  if (ftruncate(shmid, TEST_SHARED_MEM_SZIE) == -1) {
    std::cout << "truncate shared_memory failed\n";
  }

  shared_memory =
      reinterpret_cast<int*>(mmap(NULL, TEST_SHARED_MEM_SZIE, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0));
  if (shared_memory == NULL) {
    std::cout << "mmap shared memory faild.\n";
  }

  // set shared_memory to 0
  memset(shared_memory, 0, TEST_SHARED_MEM_SZIE);

  sem_id = sem_open(sem_name, O_CREAT, 0644, 0);
  if (sem_id == SEM_FAILED) {
    std::cout << "sem open failed\n";
  }

  // fork child process do server process
  pid_t pid = fork();
  if (pid < 0) {
    std::cout << "create child porcessor failed." << std::endl;
    exit(-1);
  } else if (0 == pid) {
    std::string server_recvd;
    ServerProcess(server_param, &server_recvd);
    auto ptmp = reinterpret_cast<char*>(shared_memory);
    memcpy(ptmp, server_recvd.c_str(), server_recvd.length());
    sem_post(sem_id);
    exit(0);
  } else if (pid > 0) {
    ClientProcess(client_param);
    wait(&status);

    sem_wait(sem_id);
    auto ptmp = reinterpret_cast<char*>(shared_memory);
    std::string str_child(ptmp);
    EXPECT_EQ(fake_str, str_child);
    ASSERT_EQ(0, WEXITSTATUS(status));
  }

  munmap(shared_memory, TEST_SHARED_MEM_SZIE);
  close(shmid);
  shm_unlink(mname);

  sem_close(sem_id);
  sem_unlink(sem_name);
}
}  // namespace cnstream
