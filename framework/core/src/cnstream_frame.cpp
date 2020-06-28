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
#include <unordered_map>

#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

SpinLock CNFrameInfo::spinlock_;
std::unordered_map<std::string, int> CNFrameInfo::stream_count_map_;
int CNFrameInfo::flow_depth_ = 0;

void SetFlowDepth(int flow_depth) { CNFrameInfo::flow_depth_ = flow_depth; }
int GetFlowDepth() { return CNFrameInfo::flow_depth_; }

std::shared_ptr<CNFrameInfo> CNFrameInfo::Create(const std::string& stream_id, bool eos) {
  if (stream_id == "") {
    LOG(ERROR) << "CNFrameInfo::Create() stream_id is empty string.";
    return nullptr;
  }
  std::shared_ptr<CNFrameInfo> ptr(new (std::nothrow) CNFrameInfo());
  if (!ptr) {
    LOG(ERROR) << "CNFrameInfo::Create() new CNFrameInfo failed.";
    return nullptr;
  }
  ptr->stream_id = stream_id;
  if (eos) {
    ptr->flags |= cnstream::CN_FRAME_FLAG_EOS;
    return ptr;
  }

  if (flow_depth_ > 0) {
    SpinLockGuard guard(spinlock_);
    auto iter = stream_count_map_.find(stream_id);
    if (iter == stream_count_map_.end()) {
      int count = 1;
      stream_count_map_[stream_id] = count;
      // LOG(INFO) << "CNFrameInfo::Create() insert stream_id: " << stream_id;
    } else {
      int count = stream_count_map_[stream_id];
      if (count >= flow_depth_) {
        return nullptr;
      }
      stream_count_map_[stream_id] = count + 1;
      // LOG(INFO) << "CNFrameInfo::Create() add count stream_id " << stream_id << ":" << count;
    }
  }
  return ptr;
}

CNFrameInfo::~CNFrameInfo() {
  if (flags & CN_FRAME_FLAG_EOS) {
    return;
  }
  /*if (frame.ctx.dev_type == DevContext::INVALID) {
    return;
  }*/
  if (flow_depth_ > 0) {
    SpinLockGuard guard(spinlock_);
    auto iter = stream_count_map_.find(stream_id);
    if (iter != stream_count_map_.end()) {
      int count = iter->second;
      --count;
      if (count <= 0) {
        stream_count_map_.erase(iter);
        // LOG(INFO) << "CNFrameInfo::~CNFrameInfo() erase stream_id " << frame.stream_id;
      } else {
        iter->second = count;
        // LOG(INFO) << "CNFrameInfo::~CNFrameInfo() update stream_id " << frame.stream_id << " : " << count;
      }
    } else {
      LOG(ERROR) << "Invaid stream_id, please check\n";
    }
  }
}

uint64_t CNFrameInfo::SetModuleMask(Module* module, Module* current) {
  SpinLockGuard guard(mask_lock_);
  auto iter = module_mask_map_.find(module->GetId());
  if (iter != module_mask_map_.end()) {
    iter->second |= (uint64_t)1 << current->GetId();
  } else {
    module_mask_map_[module->GetId()] = (uint64_t)1 << current->GetId();
  }
  return module_mask_map_[module->GetId()];
}

uint64_t CNFrameInfo::GetModulesMask(Module* module) {
  SpinLockGuard guard(mask_lock_);
  auto iter = module_mask_map_.find(module->GetId());
  if (iter != module_mask_map_.end()) {
    return iter->second;
  }
  return 0;
}

void CNFrameInfo::ClearModuleMask(Module* module) {
  SpinLockGuard guard(mask_lock_);
  auto iter = module_mask_map_.find(module->GetId());
  if (iter != module_mask_map_.end()) {
    iter->second = 0;
  }
}

uint64_t CNFrameInfo::AddEOSMask(Module* module) {
  SpinLockGuard guard(eos_lock_);
  eos_mask |= (uint64_t)1 << module->GetId();
  return eos_mask;
}

}  // namespace cnstream
