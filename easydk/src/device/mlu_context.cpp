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
#include <string>

#include "cxxutil/spinlock.h"
#include "internal/mlu_task_queue.h"

using std::string;
using std::to_string;

namespace edk {

#define MLU_CHANNEL_NUM 4

#define CHECK_CNRT_RET(err_code, msg)                                             \
  do {                                                                            \
    if (CNRT_RET_SUCCESS != err_code) {                                           \
      throw MluContextError(string(msg) + " error code: " + to_string(err_code)); \
    }                                                                             \
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

/**
 * @brief singleton for init cambricon runtime
 */
class CnrtInitTool {
 public:
  static CnrtInitTool* instance() {
    static CnrtInitTool instance;
    return &instance;
  }

  void init() {
    SpinLockGuard lk(lock_);
    if (!is_initialized_) {
      int err_code;
      err_code = cnrtInit(0);
      CHECK_CNRT_RET(err_code, "Init cambricon runtime failed.");
      unsigned int dev_cnt;
      err_code = cnrtGetDeviceCount(&dev_cnt);
      CHECK_CNRT_RET(err_code, "Get device count failed.");
      if (0 == dev_cnt) {
        throw MluContextError("No device found.");
      }
      LOG(INFO) << "Cambricon runtime init success.";
      is_initialized_ = true;
    }
  }

 private:
  CnrtInitTool() : is_initialized_(false) {}
  ~CnrtInitTool() {
    if (is_initialized_) cnrtDestroy();
  }
  std::atomic<bool> is_initialized_;
  SpinLock lock_;

  // disable copy and assign
  CnrtInitTool(const CnrtInitTool&) = delete;
  CnrtInitTool& operator=(const CnrtInitTool&) = delete;
};  // class CnrtInitTool

bool MluContext::CheckDeviceId(int id) {
  CnrtInitTool::instance()->init();
  cnrtDev_t dev;
  return CNRT_RET_SUCCESS == cnrtGetDeviceHandle(&dev, id);
}

void MluContext::ConfigureForThisThread() {
  static SpinLock m;
  static CoreVersion version_tmp = CoreVersion::MLU270;
  static bool has_core_version = false;
  CnrtInitTool::instance()->init();
  int err_code;
  cnrtDev_t dev;
  err_code = cnrtGetDeviceHandle(&dev, dev_id_);
  CHECK_CNRT_RET(err_code, "Get device failed.");
  err_code = cnrtSetCurrentDevice(dev);
  CHECK_CNRT_RET(err_code, "Set current device failed.");
  if (channel_id_ >= 0) {
    if (channel_id_ >= MLU_CHANNEL_NUM) {
      throw MluContextError("Only " + std::to_string(MLU_CHANNEL_NUM) +
                            " channel per mlu, channel id should less than " + std::to_string(MLU_CHANNEL_NUM));
    }
    cnrtChannelType_t channel = static_cast<cnrtChannelType_t>(channel_id_);
    err_code = cnrtSetCurrentChannel(channel);
    CHECK_CNRT_RET(err_code, "Set current channel failed.");
  }

  SpinLockGuard lk(m);
  if (!has_core_version) {
    cnrtDeviceInfo_t device_info;
    err_code = cnrtGetDeviceInfo(&device_info, dev_id_);
    CHECK_CNRT_RET(err_code, "Get device info failed.");
    switch (device_info.core_version) {
      case CNRT_MLU220: {
        version_tmp = CoreVersion::MLU220;
        LOG(INFO) << "Get Core Version MLU220";
        break;
      }
      case CNRT_MLU270: {
        version_tmp = CoreVersion::MLU270;
        LOG(INFO) << "Get Core Version MLU270";
        break;
      }
      default:
        throw MluContextError("Unsupport cnrt core version " +
                              std::to_string(static_cast<int>(device_info.core_version)));
    }
    has_core_version = true;
  }
  version_ = version_tmp;
}

}  // namespace edk
