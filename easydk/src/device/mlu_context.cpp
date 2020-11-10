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

#include "device/mlu_context.h"

#include <cnrt.h>
#include <glog/logging.h>

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

#include "cxxutil/spinlock.h"
#include "internal/mlu_task_queue.h"

using std::string;
using std::to_string;

namespace edk {

#define MLU_CHANNEL_NUM 4

#define CHECK_CNRT_RET(err_code, msg)                                                            \
  do {                                                                                           \
    if (CNRT_RET_SUCCESS != err_code) {                                                          \
      THROW_EXCEPTION(Exception::INTERNAL, string(msg) + " error code: " + to_string(err_code)); \
    }                                                                                            \
  } while (0)

MluTaskQueue::~MluTaskQueue() {
  if (queue) {
    LOG(INFO) << "Destroy MLU task queue";
    cnrtRet_t ret = cnrtDestroyQueue(queue);
    if (ret != CNRT_RET_SUCCESS) {
      LOG(ERROR) << "Destroy cnrtQueue failed, error code: " << ret;
    }
  }
}

MluTaskQueue_t CreateTaskQueue() {
  MluTaskQueue_t q = std::make_shared<MluTaskQueue>();
  cnrtRet_t ret = cnrtCreateQueue(&q->queue);
  CHECK_CNRT_RET(ret, "Create cnrtQueue failed.");
  return q;
}

void Sync(MluTaskQueue_t q) {
  CHECK(q) << "task queue is empty!";
  CHECK(q->queue) << "task queue is uninitialized!";
  CHECK_CNRT_RET(cnrtSyncQueue(q->queue), "Sync queue failed.");
}

namespace _cnrt_init_tool {
/**
 * @brief singleton for init cambricon runtime
 */
class CnrtInitTool {
 public:
  CnrtInitTool() : is_initialized_(false) {}

  ~CnrtInitTool() {
    if (is_initialized_) {
      LOG(INFO) << "Cambricon runtime destroy";
      cnrtDestroy();
    }
  }

  void Init() {
    SpinLockGuard lk(lock_);
    if (!is_initialized_) {
      cnrtRet_t err_code;
      err_code = cnrtInit(0);
      CHECK_CNRT_RET(err_code, "Init cambricon runtime failed.");
      uint32_t dev_cnt;
      err_code = cnrtGetDeviceCount(&dev_cnt);
      CHECK_CNRT_RET(err_code, "Get device count failed.");
      if (0 == dev_cnt) {
        THROW_EXCEPTION(Exception::UNAVAILABLE, "No device found.");
      }
      LOG(INFO) << "Cambricon runtime init success.";
      is_initialized_ = true;
    }
  }

 private:
  std::atomic<bool> is_initialized_;
  SpinLock lock_;

  // disable copy and assign
  CnrtInitTool(const CnrtInitTool&) = delete;
  CnrtInitTool& operator=(const CnrtInitTool&) = delete;
};  // class CnrtInitTool
static CnrtInitTool cnrt_init_tool;
}  // namespace _cnrt_init_tool

bool MluContext::CheckDeviceId(int id) {
  _cnrt_init_tool::cnrt_init_tool.Init();
  cnrtDev_t dev;
  return CNRT_RET_SUCCESS == cnrtGetDeviceHandle(&dev, id);
}

uint32_t MluContext::GetDeviceNum() {
  _cnrt_init_tool::cnrt_init_tool.Init();
  uint32_t dev_cnt;
  cnrtRet_t err_code = cnrtGetDeviceCount(&dev_cnt);
  CHECK_CNRT_RET(err_code, "Get device count failed.");
  return dev_cnt;
}

void MluContext::BindDevice() {
  _cnrt_init_tool::cnrt_init_tool.Init();
  int err_code;
  cnrtDev_t dev;
  err_code = cnrtGetDeviceHandle(&dev, dev_id_);
  CHECK_CNRT_RET(err_code, "Get device failed.");
  err_code = cnrtSetCurrentDevice(dev);
  CHECK_CNRT_RET(err_code, "Set current device failed.");
  if (channel_id_ >= 0) {
    if (channel_id_ >= MLU_CHANNEL_NUM) {
      THROW_EXCEPTION(Exception::INVALID_ARG, "Only " + std::to_string(MLU_CHANNEL_NUM) +
                                                  " channel per mlu, channel id should less than " +
                                                  std::to_string(MLU_CHANNEL_NUM));
    }
    cnrtChannelType_t channel = static_cast<cnrtChannelType_t>(channel_id_);
    err_code = cnrtSetCurrentChannel(channel);
    CHECK_CNRT_RET(err_code, "Set current channel failed.");
  }
}

void MluContext::ConfigureForThisThread() {
  BindDevice();
}

CoreVersion MluContext::GetCoreVersion() {
  _cnrt_init_tool::cnrt_init_tool.Init();
  static std::mutex m;
  CoreVersion version;
  cnrtDeviceInfo_t device_info;
  std::unique_lock<std::mutex> lk(m);
  auto err_code = cnrtGetDeviceInfo(&device_info, dev_id_);
  lk.unlock();
  CHECK_CNRT_RET(err_code, "Get device info failed.");
  switch (device_info.core_version) {
    case CNRT_MLU220: {
      version = CoreVersion::MLU220;
      VLOG(3) << "Get Core Version MLU220";
      break;
    }
    case CNRT_MLU270: {
      version = CoreVersion::MLU270;
      VLOG(3) << "Get Core Version MLU270";
      break;
    }
    default:
      THROW_EXCEPTION(Exception::INTERNAL,
                      "Unsupport cnrt core version " + std::to_string(static_cast<int>(device_info.core_version)));
  }
  return version;
}

}  // namespace edk
