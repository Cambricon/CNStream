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

#ifndef EASYCODEC_INIT_TOOLS_HPP_
#define EASYCODEC_INIT_TOOLS_HPP_

#include <cncodec.h>
#include <cnrt.h>
#include <iostream>
#include <string>
#include "cxxutil/exception.h"
#include "cxxutil/logger.h"
#include "cxxutil/spinlock.h"

// static CN_VOID CncodecFatalHandler(CN_U32 err, CN_U64 u64UserData) {
// }
//
// static CN_VOID CncodecLogHandler(CN_LOG_LEVEL level, const char* msg) {
// }

namespace edk {

TOOLKIT_REGISTER_EXCEPTION(CncodecInitTool)
class CncodecInitTool {
 public:
  static CncodecInitTool* instance() {
    static CncodecInitTool instance;
    return &instance;
  }
  void init() {
    SpinLockGuard lk(lock_);
    if (!initialized_) {
      CNResult error_code = CN_MPI_Init();
      // CN_MPI_SetFatalCallback(::CncodecFatalHandler, 0);
      // CN_MPI_SetLogCallback(::CncodecLogHandler);
      if ((error_code != CN_SUCCESS)) {
        throw CncodecInitToolError(
            "Cncodec Initialize Tool Error : "
            "can't initialize, Error Code : " +
            std::to_string(error_code));
      }
      LOG(INFO, "Cncodec init success.");
      initialized_ = true;
    }
  }
  /*********************************
   * @brief Not threadsafe
   *********************************/
  CN_U32 CncodecDeviceId(CN_U32 mlu_dev_id) {
    CNResult error_code;
    CN_VDEC_CAPABILITY_S capability;
    if ((error_code = CN_MPI_VDEC_GetCapability(&capability)) != CN_SUCCESS) {
      throw CncodecInitToolError(
          "Decoder initialize failed, "
          "can't get codec device capability, "
          "Error Code : " +
          std::to_string(error_code));
    }
    const CN_U32 dev_num = capability.u32VdecDeviceNum;
    CN_U32 dev_id = 0;
    CN_U32 max_free_chns = 0;
    bool find_device = false;
    for (CN_U32 dindex = 0; dindex < dev_num; ++dindex) {
      CN_VDEC_DEVICE_CAPABILITY_S dev_info = capability.VdecDeviceList[dindex];
      if (dev_info.u32MluIndex == mlu_dev_id) {
        find_device = true;
        if (max_free_chns < dev_info.u32FreeChannels) {
          max_free_chns = dev_info.u32FreeChannels;
          dev_id = dev_info.u32DeviceID;
        }
      }
    }
    if (!find_device) {
      throw CncodecInitToolError("Device not found, device id: " + std::to_string(mlu_dev_id));
    }
    if (0 == max_free_chns) {
      throw CncodecInitToolError(
          "There is no enough resources to support"
          " such number of channels");
    }
    return dev_id;
  }

 private:
  CncodecInitTool() : initialized_(false) {}
  ~CncodecInitTool() {
    if (initialized_) {
      CNResult error_code = CN_MPI_Exit();
      if ((error_code != CN_SUCCESS)) {
        LOG(WARNING, "CN MPI Exit failed. Error Code: %d", error_code);
      }
    }
  }
  volatile bool initialized_;
  SpinLock lock_;
};  // class CncodecInitTool

}  // namespace edk

#endif  // EASYCODEC_INIT_TOOLS_HPP_
