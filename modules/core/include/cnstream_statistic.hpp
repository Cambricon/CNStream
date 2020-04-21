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

#ifndef CNSTREAM_STATISTIC_HPP_
#define CNSTREAM_STATISTIC_HPP_

/**
 * @file cnstream_statistic.hpp
 *
 * This file contains a declaration of the Statistics class.
 */

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "cnstream_common.hpp"
#include "cnstream_frame.hpp"

namespace cnstream {

struct StreamFpsStat {
  void Update(const std::shared_ptr<CNFrameInfo> data) {
    std::unique_lock<std::mutex> lock(mutex_);
    std::string stream_id = data->frame.stream_id;
    auto it = map_fps_.find(stream_id);
    if (it == map_fps_.end()) {
      StreamFps stat;
      map_fps_[stream_id] = stat;
    }
    map_fps_[stream_id].Update(data);
  }

  double Fps(const std::string &stream_id) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto it = map_fps_.find(stream_id);
    if (it == map_fps_.end()) {
      return 0.0f;
    }
    return map_fps_[stream_id].Fps();
  }

  void PrintFps(const std::string &moduleName) {
    double total_fps = 0.0;
    std::unique_lock<std::mutex> lock(mutex_);
    std::cout << "-----------------------";
    std::cout << moduleName;
    std::cout << " -- show Fps Statistics -------------------------" << std::endl;
    for (auto &it : map_fps_) {
      std::cout << it.second.stream_id_;
      std::cout << " -- fps: ";
      std::cout << it.second.Fps();
      std::cout << ",  frame_count :";
      std::cout << it.second.frame_count_;
      std::cout << std::endl;
      total_fps += it.second.Fps();
    }
    std::cout << "Total fps:" << total_fps << std::endl;
  }

 private:
  struct StreamFps {
    std::chrono::time_point<std::chrono::steady_clock> start_time_;
    std::chrono::time_point<std::chrono::steady_clock> end_time_;
    std::string stream_id_;
    uint64_t frame_count_ = 0;
    void Update(std::shared_ptr<CNFrameInfo> data) {
      if (stream_id_.empty()) {
        stream_id_ = data->frame.stream_id;
        start_time_ = end_time_ = std::chrono::steady_clock::now();
      }
      if (!(data->frame.flags & CN_FRAME_FLAG_EOS)) {
        ++frame_count_;
        end_time_ = std::chrono::steady_clock::now();
      }
    }
    double Fps() {
      std::chrono::duration<double, std::milli> diff = end_time_ - start_time_;
      if (diff.count()) {
        return (frame_count_ * 1000 * 1.f / diff.count());
      }
      return 0.0f;
    }
  };
  std::mutex mutex_;
  std::map<std::string, StreamFps> map_fps_;
};

}  // namespace cnstream

#endif  // CNSTREAM_STATISTIC_HPP_
