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

#ifndef MODULES_IPC_CLIENT_HANDLER_HPP_
#define MODULES_IPC_CLIENT_HANDLER_HPP_

#include <cmath>
#include <map>
#include <memory>
#include <string>

#include "cnsocket.hpp"
#include "ipc_handler.hpp"

namespace cnstream {

/**
 * @brief for IPCClientHandler, inherited from IPCHandler.
 */
class IPCClientHandler : public IPCHandler {
 public:
  /**
   *  @brief  Constructed function.
   *  @param  Name : type - define client or server handler
   *  @return None
   */
  explicit IPCClientHandler(const IPCType& type, ModuleIPC* ipc_module);

  /**
   *  @brief  Destructor function.
   *  @return None
   */
  virtual ~IPCClientHandler();

  /**
   *  @brief  Open resources.
   *  @return Return true if open resources successfully, otherwise, return false.
   */
  bool Open() override;

  /**
   *  @brief  Close resources.
   *  @return Void.
   */
  void Close() override;

  /**
   *  @brief  Shutdown connection between processes.
   *  @return Void.
   */
  void Shutdown() override;

  /**
   *  @brief  Receive FrameInfoPackage with separate thread.
   *  @return Void.
   */
  void RecvPackageLoop() override;

  /**
   *  @brief  Send FrameInfoPackage.
   *  @return Return true if send FrameInfoPackage successfully, otherwise, return false.
   */
  bool Send() override;

  /**
   *  @brief  Process FrameInfoPackage with separate thread.
   *  @return Void.
   */
  void SendPackageLoop() override{};

  /**
   *  @brief  Cache processed data.
   *  @return Return true if cache processed data successfully, otherwise, return false.
   */
  bool CacheProcessedData(std::shared_ptr<CNFrameInfo> data);

#ifdef UNIT_TEST
  /**
   *  @brief  Get communicate server state.
   *  @return Return true if communicate server is closed, otherwise, return false.
   */
  bool GetServerState() { return server_closed_.load(); }
#endif

 private:
  /**
   *  @brief  Free shared memory for each frame in cached processed frame.
   *  @return Void.
   */
  void FreeSharedMemory();

 private:
  CNClient client_handle_;                           // client socket handle
  std::thread recv_thread_;                          // thread for receving message to process
  std::thread process_thread_;                       // thread for releasing shared memory used in frame
  std::atomic<bool> server_closed_{true};            // flag to identify if server is closed
  std::atomic<bool> is_running_{false};              // flag to identify if thread is running
  std::atomic<bool> is_connected_{false};            // flag to identify connection state
  std::mutex mutex_;                                 // mutex for processd frames map read/write
  ThreadSafeQueue<FrameInfoPackage> recv_releaseq_;  // message queue with frameinfo to release
  std::map<std::string, std::shared_ptr<CNFrameInfo>>
      processed_frames_map_;                     // frames which is processed, and wait to release memory
  std::condition_variable framesmap_full_cond_;  // condition variable for processed frames map size
};

}  //  namespace cnstream

#endif
