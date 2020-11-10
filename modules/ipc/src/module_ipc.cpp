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
#include "module_ipc.hpp"

#include <iostream>
#include <memory>
#include <string>

#include "client_handler.hpp"
#include "device/mlu_context.h"
#include "server_handler.hpp"

namespace cnstream {

std::shared_ptr<IPCHandler> CreateIPCHandler(const IPCType& type, ModuleIPC* ipc_module) {
  if (!ipc_module) return nullptr;

  if (type == IPC_CLIENT) {
    auto client = std::make_shared<IPCClientHandler>(type, ipc_module);
    return std::static_pointer_cast<IPCHandler>(client);
  } else if (type == IPC_SERVER) {
    auto server = std::make_shared<IPCServerHandler>(type, ipc_module);
    return std::static_pointer_cast<IPCHandler>(server);
  }

  return nullptr;
}
class ModuleIpcPrivate {};

ModuleIPC::ModuleIPC(const std::string& name) : Module(name) {
  param_register_.SetModuleDesc("ModuleIPC is a module for ipc support with socket.");
  param_register_.Register("ipc_type", "Identify ModuleIPC actor as client or server.");
  param_register_.Register("memmap_type", "Identify memory map type inter process communication.");
  param_register_.Register("socket_address", "Identify socket communicate path.");
  // only support the same device with client when use mlu mem map
  param_register_.Register("device_id", "Identify device id for server processor.");
  param_register_.Register("max_cachedframe_size",
                           "Identify max size of cached processed frame with shared memory for client.");
}

bool ModuleIPC::Open(ModuleParamSet paramSet) {
  if (!CheckParamSet(paramSet)) return false;

  IPCType type = IPC_INVALID;
  if (paramSet["ipc_type"] == "client") {
    type = IPC_CLIENT;
    hasTransmit_.store(true);
  } else if (paramSet["ipc_type"] == "server") {
    type = IPC_SERVER;
    hasTransmit_.store(true);
  } else {
    LOG(ERROR) << "[ModuleIPC], ipc_type must be client or server.";
    return false;
  }

  ipc_handler_ = CreateIPCHandler(type, this);
  if (nullptr == ipc_handler_) {
    LOG(ERROR) << "[ModuleIPC], create ipc handler failed\n";
    return false;
  }

  ipc_handler_->SetSocketAddress(paramSet["socket_address"]);
  if (type == IPC_CLIENT && paramSet.find("max_cachedframe_size") != paramSet.end()) {
    ipc_handler_->SetMaxCachedFrameSize(std::stoi(paramSet["max_cachedframe_size"]));
  }
  if (paramSet.find("device_id") != paramSet.end()) {
    ipc_handler_->SetDeviceId(std::stoi(paramSet["device_id"]));
  }

  if (paramSet["memmap_type"] == "cpu") {
    ipc_handler_->SetMemmapType(MEMMAP_CPU);
  } else if (paramSet["memmap_type"] == "mlu") {
    ipc_handler_->SetMemmapType(MEMMAP_MLU);
  } else {
    LOG(ERROR) << "[ModuleIPC], memmap_type is invalid.";
    return false;
  }

  if (!ipc_handler_->Open()) {
    LOG(ERROR) << "[ModuleIPC], open ipc handler failed\n";
    return false;
  }

  if (container_ && IPC_SERVER == type) {
    container_->RegistIPCFrameDoneCallBack(std::bind(&ModuleIPC::PostFrameToReleaseMem, this, std::placeholders::_1));
  }

  return true;
}

void ModuleIPC::Close() {
  if (ipc_handler_) {
    // send package with exit info to client
    if (IPC_SERVER == ipc_handler_->GetType()) {
      ipc_handler_->PreparePackageToSend(PkgType::PKG_EXIT, nullptr);
    }

    ipc_handler_->Close();
  }
}

int ModuleIPC::Process(std::shared_ptr<CNFrameInfo> data) {
  if (!data) return -1;
  if (ipc_handler_->GetType() != IPC_CLIENT) return -1;

  auto handler = std::dynamic_pointer_cast<IPCClientHandler>(ipc_handler_);
  if (!data->IsEos()) {
    CNDataFramePtr frame = cnstream::GetCNDataFramePtr(data);
    frame->CopyToSharedMem(handler->GetMemMapType(), data->stream_id);
    handler->CacheProcessedData(data);
  }

  handler->PreparePackageToSend(PkgType::PKG_DATA, data);
  handler->Send();
  this->TransmitData(data);
  return 0;
}

ModuleIPC::~ModuleIPC() {}

bool ModuleIPC::CheckParamSet(const ModuleParamSet& paramSet) const {
  bool ret = true;
  ParametersChecker checker;
  // ParametersChecker checker;
  for (auto& it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOG(WARNING) << "[ModuleIPC] Unknown param: " << it.first;
    }
  }
  if (paramSet.find("ipc_type") == paramSet.end()) {
    LOG(ERROR) << "[ModuleIPC], must set ipc_type. ";
    ret = false;
  }

  if (paramSet.find("memmap_type") == paramSet.end()) {
    LOG(ERROR) << "[ModuleIPC], must set memmap_type for memory shared.";
    ret = false;
  }

  if (paramSet.find("socket_address") == paramSet.end()) {
    LOG(ERROR) << "[ModuleIPC], must set socket_address.";
    ret = false;
  }

  if (paramSet.find("device_id") == paramSet.end()) {
    LOG(WARNING) << "[ModuleIPC], device id is not set, will use device info in CNFrameInfo.";
  }

  std::string err_msg;
  if (!checker.IsNum({"device_id", "max_cachedframe_size"}, paramSet, err_msg)) {
    LOG(ERROR) << err_msg;
    ret = false;
  }

  return ret;
}

bool ModuleIPC::SendData(std::shared_ptr<CNFrameInfo> frame_data) {
  if (frame_data->GetStreamIndex() == INVALID_STREAM_IDX) {
    LOG(ERROR) << "CNFrameInfo->stream_idx not initialized";
    return false;
  }
  TransmitData(frame_data);
  return false;
}

void ModuleIPC::PostFrameToReleaseMem(std::shared_ptr<CNFrameInfo> data) {
  // post frame info to communicate process(client), to release shared memory
  if (IPC_SERVER == ipc_handler_->GetType() && !data->IsEos()) {
    CNDataFramePtr frame = cnstream::GetCNDataFramePtr(data);
    frame->UnMapSharedMem(ipc_handler_->GetMemMapType());
    ipc_handler_->PreparePackageToSend(PkgType::PKG_RELEASE_MEM, data);
  }
}

}  // namespace cnstream
