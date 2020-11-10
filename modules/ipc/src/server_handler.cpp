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

#include "cnstream_frame.hpp"
#include "device/mlu_context.h"
#include "module_ipc.hpp"
#include "perf_manager.hpp"
#include "server_handler.hpp"

namespace cnstream {

IPCServerHandler::IPCServerHandler(const IPCType& type, ModuleIPC* ipc_module) : IPCHandler(type, ipc_module) {}

IPCServerHandler::~IPCServerHandler() { CloseSemphore(); }

bool IPCServerHandler::Open() {
  if (socket_address_.empty()) {
    LOG(ERROR) << "open server handler failed, socket address is empty.";
    return false;
  }

  if (!OpenSemphore()) return false;
  if (!server_handle_.Open(socket_address_)) {
    LOG(ERROR) << "open server handler failed, socket address: " << socket_address_;
    return false;
  }

  LOG(INFO) << "open server handler succeed, socket address: " << socket_address_;
  is_running_.store(true);
  listen_thread_ = std::thread(&IPCServerHandler::ListenConnections, this);
  if (!PostSemphore()) {
    LOG(WARNING) << "post semphore failed.";
    return false;
  }

  return true;
}

void IPCServerHandler::Close() {
  while (is_connected_.load() && send_pkgq_.Size() > 0) {
    std::this_thread::yield();
  }

  is_running_.store(false);
  if (listen_thread_.joinable()) {
    listen_thread_.join();
  }

  if (recv_thread_.joinable()) {
    recv_thread_.join();
  }

  for (auto& it : vec_process_thread_) {
    if (it.joinable()) it.join();
  }

  if (send_thread_.joinable()) {
    send_thread_.join();
  }

  server_handle_.Close();
}

void IPCServerHandler::Shutdown() { server_handle_.Shutdown(); }

void IPCServerHandler::RecvPackageLoop() {
  std::string recv_err_msg;
  size_t eos_chn_cnt = 0;
  while (is_running_.load()) {
    if (server_handle_.RecvData(recv_buf_, sizeof(recv_buf_)) != sizeof(recv_buf_)) {
      recv_err_msg = "server receive message error";
      LOG(ERROR) << recv_err_msg;
      server_handle_.Close();
      is_connected_.store(false);
      ipc_module_->PostEvent(EventType::EVENT_ERROR, recv_err_msg);
      break;
    }
    FrameInfoPackage recv_pkg;
    std::string recv_str(recv_buf_);
    memset(recv_buf_, 0, sizeof(recv_buf_));
    if (!ParseStringToPackage(recv_str, &recv_pkg)) {
      LOG(WARNING) << "server receive parse error";
      continue;
    }

    switch (recv_pkg.pkg_type) {
      case PKG_DATA:
        if (recv_pkg.stream_id.empty()) {
          break;
        }

#ifdef UNIT_TEST
        if (unit_test) {
          recv_pkg_.Push(recv_pkg);
          unit_test = false;
        }
#endif
        vec_recv_dataq_[recv_pkg.stream_idx % SEND_THREAD_NUM]->Push(recv_pkg);
        if (recv_pkg.flags & CN_FRAME_FLAG_EOS) {
          eos_chn_cnt++;
          if (eos_chn_cnt == ipc_module_->GetStreamCount()) {
            LOG(INFO) << "Server received all eos.";
            return;
          }
        }
        break;

      case PKG_ERROR:
        recv_err_msg = "Server receive error info from communicate process, process id: " + std::to_string(getpid());
        ipc_module_->PostEvent(EventType::EVENT_ERROR, recv_err_msg);
        return;

      default:
        LOG(WARNING) << "server receive message type error!";
        break;
    }
  }
}

void IPCServerHandler::SendPackageLoop() {
  while (is_running_.load() && is_connected_.load()) {
    FrameInfoPackage send_pkg;
    if (!send_pkgq_.WaitAndTryPop(send_pkg, std::chrono::milliseconds(10))) {
      continue;
    }

    std::string send_str;
    SerializeToString(send_pkg, &send_str);
    memset(send_buf_, 0, sizeof(send_buf_));
    memcpy(send_buf_, send_str.c_str(), send_str.length());
    if (server_handle_.SendData(send_buf_, sizeof(send_buf_)) != sizeof(send_buf_)) {
      LOG(WARNING) << " server send message to client failed.";
    }
  }
}

void IPCServerHandler::ListenConnections() {
  while (is_running_.load()) {
    int client_fd = server_handle_.Accept();
    if (-1 == client_fd) {
      LOG(ERROR) << "server listening, client connect failed.";
    } else {
      LOG(INFO) << "server listening, client connect succeed.";
      is_connected_.store(true);
      send_thread_ = std::thread(&IPCServerHandler::SendPackageLoop, this);
      recv_thread_ = std::thread(&IPCServerHandler::RecvPackageLoop, this);
      // start send data thread for each stream_idx
      for (size_t thr_idx = 0; thr_idx < SEND_THREAD_NUM; thr_idx++) {
        vec_recv_dataq_.emplace_back(new ThreadSafeQueue<FrameInfoPackage>);
        vec_process_thread_.push_back(std::thread(&IPCServerHandler::ProcessFrameInfoPackage, this, thr_idx));
      }

      server_handle_.CloseListen();
      CloseSemphore();
      break;
    }
  }
}

void IPCServerHandler::ProcessFrameInfoPackage(size_t thread_idx) {
  // get data from received data queue, unpack to cndata(in CNFrameInfo format), and send to pipeline.
  while (is_running_.load() && is_connected_.load()) {
#ifdef UNIT_TEST
    if (unit_test) continue;
#endif
    FrameInfoPackage recv_pkg;
    if (!vec_recv_dataq_[thread_idx]->WaitAndTryPop(recv_pkg, std::chrono::milliseconds(10))) {
      continue;
    }

    if (recv_pkg.stream_id.empty()) continue;
    std::shared_ptr<CNFrameInfo> data;
    while (true) {
      data = CNFrameInfo::Create(recv_pkg.stream_id);
      if (data.get() != nullptr) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    this->PackageToCNData(recv_pkg, data);

    auto perf_manager_ = ipc_module_->GetPerfManager(recv_pkg.stream_id);
    if (!data->IsEos() && (nullptr != perf_manager_)) {
      std::string thread_name = "cn-" + ipc_module_->GetName() + "-" + NumToFormatStr(thread_idx, 2);
      perf_manager_->Record(false, PerfManager::GetDefaultType(), ipc_module_->GetName(), recv_pkg.timestamp);
      perf_manager_->Record(
          PerfManager::GetDefaultType(), PerfManager::GetPrimaryKey(), std::to_string(recv_pkg.timestamp),
          ipc_module_->GetName() + PerfManager::GetThreadSuffix(), "'" + thread_name + "'");
    }

    ipc_module_->SendData(data);
  }
}

#ifdef UNIT_TEST
FrameInfoPackage IPCServerHandler::ReadReceivedData() {
  FrameInfoPackage pkg;
  while (true) {
    if (!recv_pkg_.WaitAndTryPop(pkg, std::chrono::milliseconds(10))) {
      continue;
    }
    return pkg;
  }
}

#endif

}  //  namespace cnstream
