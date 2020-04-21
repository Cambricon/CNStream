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

#ifndef MODULES_IPC_SERVER_HANDLER_HPP_
#define MODULES_IPC_SERVER_HANDLER_HPP_

#include <atomic>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "cnsocket.hpp"
#include "ipc_handler.hpp"

namespace cnstream {

#define SEND_THREAD_NUM 4  // define threads num for sending data to pipeline
using CNPackageQueue = std::unique_ptr<ThreadSafeQueue<FrameInfoPackage>>;

/**
 * @brief for IPCServerHandler, inherited from IPCHandler.
 */
class IPCServerHandler : public IPCHandler {
 public:
  /**
   *  @brief  Constructed function.
   *  @param  Name : type - define client or server handler
   *  @return None
   */
  explicit IPCServerHandler(const IPCType& type, ModuleIPC* ipc_module);

  /**
   *  @brief  Destructor function.
   *  @return None
   */
  virtual ~IPCServerHandler();

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
  bool Send() override { return false; }

  /**
   *  @brief  Process FrameInfoPackage with separate thread.
   *  @return Void.
   */
  void SendPackageLoop() override;

#ifdef UNIT_TEST
  FrameInfoPackage ReadReceivedData();
#endif

 private:
  /**
   *  @brief  Listen connections for server
   *  @return Void.
   */
  void ListenConnections();

  /**
   *  @brief  Process received frame info package, convert FrameInfoPackage to CNFrameInfo and send to pipeline for each
   * channel.
   *  @return Void.
   */
  void ProcessFrameInfoPackage(size_t channel_idx);

 private:
  CNServer server_handle_;                       // server socket handle
  std::thread listen_thread_;                    // thread for listening connection
  std::thread recv_thread_;                      // thread for listening received data
  std::thread send_thread_;                      // thread for sending data
  std::atomic<bool> is_running_{false};          // flag to identify thread state
  std::atomic<bool> is_connected_{false};        // flag to identify connection state
  std::vector<CNPackageQueue> vec_recv_dataq_;   // queues to storage revceived data
  std::vector<std::thread> vec_process_thread_;  // threads for processing received data
#ifdef UNIT_TEST
  ThreadSafeQueue<FrameInfoPackage> recv_pkg_;  // received pkg queue for unit_test
  bool unit_test = true;
#endif
};

}  //  namespace cnstream

#endif
