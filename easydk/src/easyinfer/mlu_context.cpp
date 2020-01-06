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
#include <iostream>
#include <string>

#include "cxxutil/logger.h"
#include "cxxutil/spinlock.h"
#include "easyinfer/mlu_context.h"

namespace edk {

#define MLU_CHANNEL_NUM 4

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
      if (CNRT_RET_SUCCESS != (err_code = cnrtInit(0))) {
        throw MluContextError(
            "Init cambricon runtime failed."
            " error code " +
            std::to_string(err_code));
      }
      unsigned int dev_cnt;
      if (CNRT_RET_SUCCESS != (err_code = cnrtGetDeviceCount(&dev_cnt))) {
        throw MluContextError(
            "Get device count failed."
            " error code " +
            std::to_string(err_code));
      }
      if (0 == dev_cnt) {
        throw MluContextError("No device found.");
      }
      LOG(INFO, "Cambricon runtime init success.");
      is_initialized_ = true;
    }
  }

 private:
  CnrtInitTool() : is_initialized_(false) {}
  ~CnrtInitTool() {
    if (is_initialized_) cnrtDestroy();
  }
  volatile bool is_initialized_;
  SpinLock lock_;

  // disable copy and assign
  CnrtInitTool(const CnrtInitTool&) = delete;
  CnrtInitTool& operator=(const CnrtInitTool&) = delete;
};  // class CnrtInitTool

int MluContext::DeviceId() const { return dev_id_; }

void MluContext::SetDeviceId(int id) { dev_id_ = id; }

int MluContext::ChannelId() const { return channel_id_; }

void MluContext::SetChannelId(int id) { channel_id_ = id; }

void MluContext::ConfigureForThisThread() const {
  CnrtInitTool::instance()->init();
  int err_code;
  cnrtDev_t dev;
  if (CNRT_RET_SUCCESS != (err_code = cnrtGetDeviceHandle(&dev, dev_id_))) {
    throw MluContextError(
        "Get device failed."
        " error code " +
        std::to_string(err_code));
  }
  if (CNRT_RET_SUCCESS != (err_code = cnrtSetCurrentDevice(dev))) {
    throw MluContextError(
        "Set current device failed."
        " error code " +
        std::to_string(err_code));
  }
  if (channel_id_ >= 0) {
    if (channel_id_ >= MLU_CHANNEL_NUM) {
      throw MluContextError("Only " + std::to_string(MLU_CHANNEL_NUM) +
                            " channel per mlu, channel id should less than " + std::to_string(MLU_CHANNEL_NUM));
    }
    cnrtChannelType_t channel = static_cast<cnrtChannelType_t>(channel_id_);
    if (CNRT_RET_SUCCESS != (err_code = cnrtSetCurrentChannel(channel))) {
      throw MluContextError("Set current channel failed. error code " + std::to_string(err_code));
    }
  }
}

}  // namespace edk
