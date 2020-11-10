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

#include <memory>
#include <string>
#include <utility>

#include "client_handler.hpp"
#include "cnstream_frame.hpp"
#include "device/mlu_context.h"
#include "module_ipc.hpp"

namespace cnstream {

IPCClientHandler::IPCClientHandler(const IPCType& type, ModuleIPC* ipc_module) : IPCHandler(type, ipc_module) {}

IPCClientHandler::~IPCClientHandler() { CloseSemphore(); }

bool IPCClientHandler::Open() {
  if (socket_address_.empty()) {
    LOG(ERROR) << "client connect to server, socket address is empty.";
    return false;
  }

  if (!OpenSemphore()) return false;
  while (!WaitSemphore()) {
    LOG(WARNING) << "wait semphore failed, continue.";
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  if (!client_handle_.Open(socket_address_)) {
    LOG(ERROR) << "client connect to server failed, unix address: " << socket_address_;
    return false;
  }

  LOG(INFO) << "client connect to server succeed, unix address: " << socket_address_;
  server_closed_.store(false);
  is_running_.store(true);
  is_connected_.store(true);
  recv_thread_ = std::thread(&IPCClientHandler::RecvPackageLoop, this);
  process_thread_ = std::thread(&IPCClientHandler::FreeSharedMemory, this);
  CloseSemphore();
  return true;
}

void IPCClientHandler::Close() {
  // client wait server to close, always do this.
  while (!server_closed_.load() && is_connected_.load()) {
    std::this_thread::yield();
  }

  is_running_.store(false);
  if (recv_thread_.joinable()) {
    recv_thread_.join();
  }
  if (process_thread_.joinable()) {
    process_thread_.join();
  }

  client_handle_.Close();

  // clear all cached processed data, and free all shared memory
  if (processed_frames_map_.size() > 0) {
    for (auto& it : processed_frames_map_) {
      auto data = it.second;
      CNDataFramePtr frame = cnstream::GetCNDataFramePtr(data);
      frame->ReleaseSharedMem(memmap_type_, data->stream_id);
    }
  }
}

void IPCClientHandler::Shutdown() { client_handle_.Shutdown(); }

void IPCClientHandler::RecvPackageLoop() {
  std::string recv_err_msg;
  while (is_running_.load()) {
    if (client_handle_.RecvData(recv_buf_, sizeof(recv_buf_)) != sizeof(recv_buf_)) {
      recv_err_msg = "client receive message error";
      LOG(ERROR) << recv_err_msg;
      client_handle_.Close();
      is_connected_.store(false);
      ipc_module_->PostEvent(EventType::EVENT_ERROR, recv_err_msg);
      break;
    }
    FrameInfoPackage recv_pkg;
    std::string recv_str(recv_buf_);
    memset(recv_buf_, 0, sizeof(recv_buf_));
    if (!ParseStringToPackage(recv_str, &recv_pkg)) {
      LOG(WARNING) << "client parse error.";
    }

    switch (recv_pkg.pkg_type) {
      case PkgType::PKG_DATA:
        break;

      case PkgType::PKG_ERROR:
        server_closed_.store(true);
        recv_err_msg = "Client receive error info from communicate process, process id: " + std::to_string(getpid());
        ipc_module_->PostEvent(EventType::EVENT_ERROR, recv_err_msg);
        return;

      case PkgType::PKG_EXIT:
        server_closed_.store(true);
        return;

      case PkgType::PKG_RELEASE_MEM:
        if (recv_pkg.stream_id.empty()) break;
        recv_releaseq_.Push(recv_pkg);
        break;

      default:
        break;
    }
  }
}

bool IPCClientHandler::Send() {
  if (is_connected_.load()) {
    if (client_handle_.SendData(send_buf_, sizeof(send_buf_)) != sizeof(send_buf_)) {
      LOG(WARNING) << " client send message to server failed.";
      return false;
    }
  }
  return true;
}

void IPCClientHandler::FreeSharedMemory() {
  // get data from received data queue, find CNFrameInfo data in processed map, and release shared memory in CNDataFrame
  while (is_running_.load()) {
    FrameInfoPackage recv_pkg;
    if (!recv_releaseq_.WaitAndTryPop(recv_pkg, std::chrono::milliseconds(20))) continue;

    if (recv_pkg.stream_id.empty()) continue;
    std::unique_lock<std::mutex> lock(mutex_);
    // get data from processed map, release it's shared memory, and pop data from processed map
    std::string key = "stream_id_" + recv_pkg.stream_id + "_frame_id_" + std::to_string(recv_pkg.frame_id);
    auto iter = processed_frames_map_.find(key);
    if (iter != processed_frames_map_.end()) {
      CNDataFramePtr frame = cnstream::GetCNDataFramePtr(iter->second);
      frame->ReleaseSharedMem(memmap_type_, iter->second->stream_id);
      processed_frames_map_.erase(iter);
      framesmap_full_cond_.notify_one();
    } else {
      LOG(FATAL) << "frame need to release can not find by key: " << key;
    }
  }
}

bool IPCClientHandler::CacheProcessedData(std::shared_ptr<CNFrameInfo> data) {
  while (is_running_.load() && is_connected_.load()) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (framesmap_full_cond_.wait_for(lock, std::chrono::milliseconds(10),
                                      [&] { return (processed_frames_map_.size() < max_cachedframe_size_); })) {
      CNDataFramePtr frame = cnstream::GetCNDataFramePtr(data);
      std::string key = "stream_id_" + data->stream_id + "_frame_id_" + std::to_string(frame->frame_id);
      processed_frames_map_.insert(make_pair(key, data));
      return true;
    }
  }

  return false;
}

}  //  namespace cnstream
