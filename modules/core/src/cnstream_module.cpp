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
#include <unordered_map>

#include "cnstream_eventbus.hpp"
#include "cnstream_pipeline.hpp"
#include "threadsafe_queue.hpp"

namespace cnstream {

CNSpinLock Module::module_id_spinlock_;
uint64_t Module::module_id_mask_ = 0;

uint32_t GetMaxModuleNumber() {
  /*maxModuleIdNum is sizeof(module_id_mask_) * 8  (bytes->bits)*/
  return sizeof(uint64_t) * 8;
}

size_t Module::GetId() {
  CNSpinLockGuard guard(module_id_spinlock_);
  if (id_ != INVALID_MODULE_ID) {
    return id_;
  }
  for (size_t i = 0; i < sizeof(module_id_mask_) * 8; i++) {
    if (!(module_id_mask_ & ((uint64_t)1 << i))) {
      module_id_mask_ |= (uint64_t)1 << i;
      id_ = i;
      return i;
    }
  }
  id_ = INVALID_MODULE_ID;
  return INVALID_MODULE_ID;
}

void Module::ReturnId() {
  CNSpinLockGuard guard(module_id_spinlock_);
  if (id_ < 0 || id_ >= sizeof(module_id_mask_) * 8) {
    return;
  }
  module_id_mask_ &= ~(1 << id_);
  id_ = INVALID_MODULE_ID;
}

bool Module::PostEvent(EventType type, const std::string& msg) const {
  Event event;
  event.type = type;
  event.message = msg;
  event.module = this;
  if (container_) {
    return container_->GetEventBus()->PostEvent(event);
  } else {
    LOG(WARNING) << "[" << GetName() << "] module's container is not set";
    return false;
  }
}

int Module::DoProcess(std::shared_ptr<CNFrameInfo> data) {
  return Process(data);
}

bool Module::TransmitData(std::shared_ptr<CNFrameInfo> data) {
  if (HasTransmit()) {
    if (container_) {
      return container_->ProvideData(this, data);
    } else {
      output_frame_queue_.Push(data);
    }
  }
  return false;
}

std::shared_ptr<CNFrameInfo> Module::GetOutputFrame()  {
  std::shared_ptr<CNFrameInfo> output_frame = nullptr;
  output_frame_queue_.WaitAndTryPop(output_frame, std::chrono::milliseconds(100));
  return output_frame;
}

void Module::SetPerfManagers(const std::unordered_map<std::string, std::shared_ptr<PerfManager>>& perf_managers) {
  perf_managers_ = perf_managers;
}

std::shared_ptr<PerfManager> Module::GetPerfManager(const std::string& stream_id) {
  if (perf_managers_.find(stream_id) != perf_managers_.end()) {
    return perf_managers_[stream_id];
  }
  return nullptr;
}

ModuleFactory* ModuleFactory::factory_ = nullptr;

}  // namespace cnstream
