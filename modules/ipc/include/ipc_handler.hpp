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

#ifndef MODULES_IPC_HANDLER_HPP_
#define MODULES_IPC_HANDLER_HPP_

/**
 *  This file contains a declaration of class IPCHandler
 */

#include <semaphore.h>
#include <cmath>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "cnstream_frame.hpp"
#include "data_type.hpp"
#include "threadsafe_queue.hpp"

namespace cnstream {

#define SOCK_BUFSIZE 512  // buffer size

/**
 * An enumerated type that is used to identify the frame info package type transmitting between processores.
 */
enum PkgType {
  PKG_INVALID = -1,     ///< invalid package type
  PKG_DATA = 0,         ///< data package
  PKG_RELEASE_MEM = 1,  ///< package with release shared memory info
  PKG_EXIT = 2,         ///< package with exit info
  PKG_ERROR = 3         ///< package with error info
};

/**
 * The structure holding process info and frame info transmitting between processores.
 */
typedef struct {
  PkgType pkg_type;                           ///< package type
  uint32_t channel_idx = INVALID_STREAM_IDX;  ///< The index of the channel, stream_index
  std::string stream_id;                      ///< The data stream aliases where this frame is located to.
  size_t flags = 0;                           ///< The mask for this frame, ``CNFrameFlag``.
  int64_t frame_id;                           ///< The frame index that incremented from 0.
  int64_t timestamp;                          ///< The time stamp of this frame.
  CNDataFormat fmt;                           ///< The format of the frame.
  int width;                                  ///< The width of the frame.
  int height;                                 ///< The height of the frame.
  int stride[CN_MAX_PLANES];                  ///< The strides of the frame.
  void* ptr_mlu[CN_MAX_PLANES];               ///< The MLU data addresses for planes.
  DevContext ctx;                             ///< The device context of this frame.
  MemMapType mem_map_type;                    ///< memory map/shared type.
  void* mlu_mem_handle;                       ///< The MLU memory handle for mlu data.
} FrameInfoPackage;

class ModuleIPC;

/**
 * @brief for IPCHandler, base class definition
 */
class IPCHandler {
 public:
  /**
   *  @brief  Constructed function.
   *  @param  Name : type - define client or server handler
   *  @return None
   */
  explicit IPCHandler(const IPCType& type, ModuleIPC* ipc_module) : ipc_type_(type), ipc_module_(ipc_module) {}

  /**
   *  @brief  Destructor function.
   *  @return None
   */
  virtual ~IPCHandler() {}

  /**
   *  @brief  Open resources.
   *  @return Return true if open resources successfully, otherwise, return false.
   */
  virtual bool Open() = 0;

  /**
   *  @brief  Close resources.
   *  @return Void.
   */
  virtual void Close() = 0;

  /**
   *  @brief  Shutdown connection between processores.
   *  @return Void.
   */
  virtual void Shutdown() = 0;

  /**
   *  @brief  Receive FrameInfoPackage with separate thread.
   *  @return Void.
   */
  virtual void RecvPackageLoop() = 0;

  /**
   *  @brief  Send FrameInfoPackage.
   *  @return Return true if send FrameInfoPackage successfully, otherwise, return false.
   */
  virtual bool Send() = 0;

  /**
   *  @brief  Process FrameInfoPackage with separate thread.
   *  @return Void.
   */
  virtual void SendPackageLoop() = 0;

  /**
  *  @brief  Prepare FrameInfoPackage to send_buf or send_package queue.
  *  @return Void.
  */
  void PreparePackageToSend(const PkgType& type, const std::shared_ptr<CNFrameInfo> data);

  /**
  *  @brief  Trans data from FrameInfoPackage to CNFrameInfo data.
  *  @return Void.
  */
  void PackageToCNData(const FrameInfoPackage& pkg, std::shared_ptr<CNFrameInfo> data);

  /**
   *  @brief  Get IPCHandler type.
   *  @return Return type of this ipc handler.
   */
  inline IPCType GetType() { return ipc_type_; }

  /**
   *  @brief  Get memory map type between processes.
   *  @return Return memory map type used in this ipc handler.
   */
  inline MemMapType GetMemMapType() { return memmap_type_; }

  /**
  *  @brief  Set max cached processed frame map size.
  *  @return Void.
  */
  inline void SetMaxCachedFrameSize(const uint32_t size) { max_cachedframe_size_ = size; }

  /**
   *  @brief  Set communication socket address.
   *  @return Void.
   */
  inline void SetSocketAddress(const std::string& socket_address) { socket_address_ = socket_address; }

  /**
   *  @brief  Set memory map type.
   *  @return Void.
   */
  inline void SetMemmapType(const MemMapType& type) { memmap_type_ = type; }

  /**
   *  @brief  Set device id.
   *  @return Void.
   */
  inline void SetDeviceId(const int device_id) {
    dev_ctx_.dev_id = device_id;
    dev_ctx_.dev_type = DevContext::MLU;    // by default: if set device id, use MLU  device type
  }

 protected:
  /**
  *  @brief Open semphore to sync processors.
  *  @return Return true if open semphore successfully, otherwise, return false.
  */
  bool OpenSemphore();

  /**
   *  @brief  Close semphore.
   *  @return Void.
   */
  void CloseSemphore();

  /**
  *  @brief  Post semphore to sync processors.
  *  @return Void.
  */
  void PostSemphore();

  /**
  *  @brief Wait semphore to sync processors.
  *  @return Void.
  */
  void WaitSemphore();

 protected:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  /**
  *  @brief  Parse string to FrameInfoPackage.
  *  @return Return true if parse successfully, otherwise, return false.
  */
  bool ParseStringToPackage(const std::string& str, FrameInfoPackage* pkg);

  /**
  *  @brief  Serialize package to string.
  *  @return Return true if serialize package successfully, otherwise, return false.
  */
  bool SerializeToString(const FrameInfoPackage& pkg, std::string* str);

 protected:
  IPCType ipc_type_ = IPC_INVALID;               // type of this ipc handler
  ModuleIPC* ipc_module_ = nullptr;              // ipc module
  std::string socket_address_;                   // communication socket adress
  MemMapType memmap_type_ = MEMMAP_CPU;          // memory map type, with cpu by default
  char recv_buf_[SOCK_BUFSIZE];                  // receive buffer
  char send_buf_[SOCK_BUFSIZE];                  // send buffer
  ThreadSafeQueue<FrameInfoPackage> send_pkgq_;  // queue for package to send
  uint32_t max_cachedframe_size_ = 40;           // max size for cached processed frame map
  DevContext dev_ctx_;                           // device context info for server.

 private:
  sem_t* sem_id_ = nullptr;   // semaphore id
  bool sem_created_ = false;  // identify if semaphore is created in this instance
  std::mutex mem_map_mutex_;  // mutex to memory map
};

}  //  namespace cnstream

#endif
