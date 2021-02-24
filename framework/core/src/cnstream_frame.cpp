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

std::mutex CNFrameInfo::stream_count_lock_;
std::unordered_map<std::string, int> CNFrameInfo::stream_count_map_;

static std::mutex s_eos_lock_;
static std::unordered_map<std::string, std::atomic<bool>> s_stream_eos_map_;

static std::mutex s_remove_lock_;
static std::unordered_map<std::string, bool> s_stream_removed_map_;

int CNFrameInfo::flow_depth_ = 0;

void SetFlowDepth(int flow_depth) { CNFrameInfo::flow_depth_ = flow_depth; }
int GetFlowDepth() { return CNFrameInfo::flow_depth_; }

bool CheckStreamEosReached(const std::string &stream_id, bool sync) {
  if (sync) {
    while (1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      std::lock_guard<std::mutex> guard(s_eos_lock_);
      auto iter = s_stream_eos_map_.find(stream_id);
      if (iter != s_stream_eos_map_.end()) {
        if (iter->second == true) {
          s_stream_eos_map_.erase(iter);
          // LOGI(CORE) << "check stream eos reached, stream_id =  " << stream_id;
          return true;
        }
      } else {
        return false;
      }
    }
    return false;
  } else {
    std::lock_guard<std::mutex> guard(s_eos_lock_);
    auto iter = s_stream_eos_map_.find(stream_id);
    if (iter != s_stream_eos_map_.end()) {
      if (iter->second == true) {
        s_stream_eos_map_.erase(iter);
        return true;
      }
    }
    return false;
  }
}

void SetStreamRemoved(const std::string &stream_id, bool value) {
  std::lock_guard<std::mutex> guard(s_remove_lock_);
  auto iter = s_stream_removed_map_.find(stream_id);
  if (iter != s_stream_removed_map_.end()) {
    if (value != true) {
      s_stream_removed_map_.erase(iter);
      return;
    }
    iter->second = true;
  } else {
    s_stream_removed_map_[stream_id] = value;
  }
  // LOGI(CORE) << "_____SetStreamRemoved " << stream_id << ":" << s_stream_removed_map_[stream_id];
}

bool IsStreamRemoved(const std::string &stream_id) {
  std::lock_guard<std::mutex> guard(s_remove_lock_);
  auto iter = s_stream_removed_map_.find(stream_id);
  if (iter != s_stream_removed_map_.end()) {
    // LOGI(CORE) << "_____IsStreamRemoved " << stream_id << ":" << s_stream_removed_map_[stream_id];
    return s_stream_removed_map_[stream_id];
  }
  return false;
}

std::shared_ptr<CNFrameInfo> CNFrameInfo::Create(const std::string& stream_id, bool eos,
                                                 std::shared_ptr<CNFrameInfo> payload) {
  if (stream_id == "") {
    LOGE(CORE) << "CNFrameInfo::Create() stream_id is empty string.";
    return nullptr;
  }
  std::shared_ptr<CNFrameInfo> ptr(new (std::nothrow) CNFrameInfo());
  if (!ptr) {
    LOGE(CORE) << "CNFrameInfo::Create() new CNFrameInfo failed.";
    return nullptr;
  }
  ptr->stream_id = stream_id;
  ptr->payload = payload;
  if (eos) {
    ptr->flags |= cnstream::CN_FRAME_FLAG_EOS;
    if (!ptr->payload) {
      std::lock_guard<std::mutex> guard(s_eos_lock_);
      s_stream_eos_map_[stream_id] = false;
    }
    return ptr;
  }

  if (flow_depth_ > 0) {
    std::lock_guard<std::mutex> guard(stream_count_lock_);
    auto iter = stream_count_map_.find(stream_id);
    if (iter == stream_count_map_.end()) {
      stream_count_map_[stream_id] = 1;
      // LOGI(CORE) << "CNFrameInfo::Create() insert stream_id: " << stream_id;
    } else {
      int count = stream_count_map_[stream_id];
      if (count >= flow_depth_) {
        return nullptr;
      }
      stream_count_map_[stream_id] = count + 1;
      // LOGI(CORE) << "CNFrameInfo::Create() add count stream_id " << stream_id << ":" << count;
    }
  }
  return ptr;
}

CNFrameInfo::~CNFrameInfo() {
  if (this->IsEos()) {
    if (!this->payload) {
      std::lock_guard<std::mutex> guard(s_eos_lock_);
      s_stream_eos_map_[stream_id] = true;
    }
    return;
  }
  /*if (frame.ctx.dev_type == DevContext::INVALID) {
    return;
  }*/
  if (flow_depth_ > 0) {
    std::lock_guard<std::mutex> guard(stream_count_lock_);
    auto iter = stream_count_map_.find(stream_id);
    if (iter != stream_count_map_.end()) {
      int count = iter->second;
      --count;
      if (count <= 0) {
        stream_count_map_.erase(iter);
        // LOGI(CORE) << "CNFrameInfo::~CNFrameInfo() erase stream_id " << frame.stream_id;
      } else {
        iter->second = count;
        // LOGI(CORE) << "CNFrameInfo::~CNFrameInfo() update stream_id " << frame.stream_id << " : " << count;
      }
    } else {
      LOGE(CORE) << "Invaid stream_id, please check\n";
    }
  }
}

void CNFrameInfo::SetModulesMask(uint64_t mask) {
  std::lock_guard<std::mutex> lk(mask_lock_);
  modules_mask_ = mask;
}

uint64_t CNFrameInfo::MarkPassed(Module* module) {
  std::lock_guard<std::mutex> lk(mask_lock_);
  modules_mask_ |= (uint64_t)1 << module->GetId();
  return modules_mask_;
}

uint64_t CNFrameInfo::GetModulesMask() {
  std::lock_guard<std::mutex> lk(mask_lock_);
  return modules_mask_;
}

}  // namespace cnstream
