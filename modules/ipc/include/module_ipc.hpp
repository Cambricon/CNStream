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

#ifndef MODULES_IPC_HPP_
#define MODULES_IPC_HPP_
/**
 *  This file contains a declaration of class ModuleIPC
 */

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "cnstream_core.hpp"
#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"
#include "ipc_handler.hpp"

namespace cnstream {
/**
 * @brief for inter-process communication, will use in pair, with client-server format.
 */

class ModuleIPC : public Module, public ModuleCreator<ModuleIPC> {
 public:
  /**
   *  @brief  Generate ModuleIPC
   *
   *  @param  Name : Module name
   *
   *  @return None
   */
  explicit ModuleIPC(const std::string& name);

  /**
   * @brief Called by pipeline when pipeline start.
   *
   * @param paramSet: parameters for this module.
   *
   * @return whether module open succeed.
   */
  bool Open(ModuleParamSet paramSet) override;

  /**
   * @brief Called by pipeline when pipeline end.
   *
   * @return void.
   */
  void Close() override;

  /**
   * @brief Do for each data frame, pack cnframe data, and post from one process to another.
   *
   * @param data : Pointer to the frame info.
   *
   * @return whether post data to communicate processor succeed.
   *
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

  /**
   * @brief Check ParamSet for this module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Return true if this API run successfully. Otherwise, return false.
   */
  bool CheckParamSet(const ModuleParamSet& paramSet) const override;

  virtual ~ModuleIPC();

  /**
   * @brief Provide data for it's pipeline, when ModuleIPC act as server.
   * @return Return true if provide data to pipeline successfully. Otherwise, return false.
   */
  bool SendData(std::shared_ptr<CNFrameInfo> frame_data);

  /**
   * @brief Set channel count, when ModuleIPC act as server.
   * @param chn_cnt, channel count.
   * @return void.
   */
  inline void SetStreamCount(size_t chn_cnt) {
    chn_cnt_.store(chn_cnt);
  }

  /**
   * @brief Get channel count, when ModuleIPC act as server.
   * @param void.
   * @return channel count.
   */
  inline size_t GetStreamCount() {
    return chn_cnt_.load();
  }

#ifdef UNIT_TEST
 public:  // NOLINT
  /**
   * @brief Get ipc_handler_, for unit test.
   * @param void.
   * @return IPCHandler pointer.
   */
  std::shared_ptr<IPCHandler> GetIPCHandler() { return ipc_handler_; }
#else
 private:  // NOLINT
#endif
  std::shared_ptr<IPCHandler> ipc_handler_ =
      nullptr;  // ipc handler, may act as client or server, which depends on the parameter configuration

  /**
   * @brief When ModuleIPC act as server, post frame info to communicate process to release shared memory,.
   *
   * @param Pointer to the frame info.
   *
   * @return void.
   */
  void PostFrameToReleaseMem(std::shared_ptr<CNFrameInfo> data);
  std::atomic<size_t> chn_cnt_{0};  // channel counts
};                      // class ModuleIPC

}  // namespace cnstream
#endif
