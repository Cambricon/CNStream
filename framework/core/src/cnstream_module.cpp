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
#include "cnstream_module.hpp"

#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "cnstream_eventbus.hpp"
#include "cnstream_pipeline.hpp"

namespace cnstream {

#ifdef UNIT_TEST
static SpinLock module_id_spinlock_;
static uint64_t module_id_mask_ = 0;
static size_t _GetId() {
  SpinLockGuard guard(module_id_spinlock_);
  for (size_t i = 0; i < sizeof(module_id_mask_) * 8; i++) {
    if (!(module_id_mask_ & ((uint64_t)1 << i))) {
      module_id_mask_ |= (uint64_t)1 << i;
      return i;
    }
  }
  return INVALID_MODULE_ID;
}
static void _ReturnId(size_t id_) {
  SpinLockGuard guard(module_id_spinlock_);
  if (id_ < 0 || id_ >= sizeof(module_id_mask_) * 8) {
    return;
  }
  module_id_mask_ &= ~(1 << id_);
}
#endif

Module::~Module() {
  RwLockReadGuard guard(container_lock_);
  if (container_) {
    container_->ReturnModuleIdx(id_);
  } else {
#ifdef UNIT_TEST
    _ReturnId(id_);
#endif
  }
}

void Module::SetContainer(Pipeline* container) {
  if (container) {
    {
      RwLockWriteGuard guard(container_lock_);
      container_ = container;
    }
    GetId();
  } else {
    RwLockWriteGuard guard(container_lock_);
    container_ = nullptr;
    id_ = INVALID_MODULE_ID;
  }
}

size_t Module::GetId() {
  if (id_ == INVALID_MODULE_ID) {
    RwLockReadGuard guard(container_lock_);
    if (container_) {
      id_ = container_->GetModuleIdx();
    } else {
#ifdef UNIT_TEST
      id_ = _GetId();
#endif
    }
  }
  return id_;
}

bool Module::PostEvent(EventType type, const std::string& msg) {
  Event event;
  event.type = type;
  event.message = msg;
  event.module_name = name_;

  RwLockReadGuard guard(container_lock_);
  if (container_) {
    return container_->GetEventBus()->PostEvent(event);
  } else {
    LOG(WARNING) << "[" << GetName() << "] module's container is not set";
    return false;
  }
}

int Module::DoProcess(std::shared_ptr<CNFrameInfo> data) {
  RecordTime(data, false);
  if (!HasTransmit()) {
    int ret = 0;
    /* Process() for normal module does not need to handle EOS*/
    if (!data->IsEos()) {
      ret = Process(data);
      RecordTime(data, true);
    }
    RwLockReadGuard guard(container_lock_);
    if (container_) {
      if (container_->ProvideData(this, data) != true) {
        return -1;
      }
    }
    return ret;
  }
  return Process(data);
}

bool Module::TransmitData(std::shared_ptr<CNFrameInfo> data) {
  if (!HasTransmit()) {
    return true;
  }
  RecordTime(data, true);

  RwLockReadGuard guard(container_lock_);
  if (container_) {
    return container_->ProvideData(this, data);
  }
#ifdef UNIT_TEST
  else {  // NOLINT
    output_frame_queue_.Push(data);
  }
#endif
  return false;
}

void Module::RecordTime(std::shared_ptr<CNFrameInfo> data, bool is_finished) {
  std::shared_ptr<PerfManager> manager = GetPerfManager(data->stream_id);
  if (!data->IsEos() && manager) {
    manager->Record(is_finished, PerfManager::GetDefaultType(), this->GetName(), data->timestamp);
    if (!is_finished) {
      manager->Record(PerfManager::GetDefaultType(), PerfManager::GetPrimaryKey(), std::to_string(data->timestamp),
                      this->GetName() + "_th", "'" + GetThreadName(pthread_self()) + "'");
    }
  }
}

std::shared_ptr<PerfManager> Module::GetPerfManager(const std::string& stream_id) {
  std::unordered_map<std::string, std::shared_ptr<PerfManager>> managers;
  RwLockReadGuard guard(container_lock_);
  if (container_) {
    managers = container_->GetPerfManagers();
    if (managers.find(stream_id) != managers.end()) {
      return managers[stream_id];
    }
  }
  return nullptr;
}

#ifdef UNIT_TEST
std::shared_ptr<CNFrameInfo> Module::GetOutputFrame() {
  std::shared_ptr<CNFrameInfo> output_frame = nullptr;
  output_frame_queue_.WaitAndTryPop(output_frame, std::chrono::milliseconds(100));
  return output_frame;
}
#endif

ModuleFactory* ModuleFactory::factory_ = nullptr;

}  // namespace cnstream
