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
#include "fps_stats.hpp"
#include <algorithm>
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include "glog/logging.h"

namespace cnstream {

FpsStats::FpsStats(const std::string& name) : Module(name) {
  stream_fps_ = new (std::nothrow) StreamFps[GetMaxStreamNumber()];
  LOG_IF(FATAL, nullptr == stream_fps_) << "FpsStats::FpsStats() new StreamFps failed";
  param_register_.SetModuleDesc("FpsStats is a module for show fps stats.");
}

FpsStats::~FpsStats() {
  Close();
  delete[] stream_fps_;
}

bool FpsStats::Open(ModuleParamSet paramSet) { return true; }

void FpsStats::Close() {}

int FpsStats::Process(std::shared_ptr<CNFrameInfo> data) {
  uint32_t stream_idx = data->channel_idx;
  if (stream_idx < GetMaxStreamNumber()) {
    std::unique_lock<std::mutex> lock(stream_fps_[stream_idx].mutex_);
    stream_fps_[stream_idx].update(data);
  } else {
    LOG(ERROR) << stream_idx << "Invalid Channel Idx";
    return -1;
  }
  return 0;
}

void FpsStats::ShowStatistics() {
  std::cout << "------------------------FpsStats::ShowStatistics------------------------" << std::endl;
  auto total_fps = 0.0f;
  for (uint32_t i = 0; i < GetMaxStreamNumber(); i++) {
    std::unique_lock<std::mutex> lock(stream_fps_[i].mutex_);
    if (stream_fps_[i].stream_id_.empty()) {
      continue;
    }
    std::cout << stream_fps_[i].stream_id_ << " -- fps: " << stream_fps_[i].fps();
    std::cout << ",frame_count : " << stream_fps_[i].frame_count_;
    std::cout << std::endl;
    total_fps += stream_fps_[i].fps();
  }
  std::cout << "Total fps:" << total_fps << std::endl;
}

bool FpsStats::CheckParamSet(const ModuleParamSet& paramSet) const {
  for (auto& it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOG(WARNING) << "[FpsStats] Unknown param: " << it.first;
    }
  }
  return true;
}

}  // namespace cnstream
