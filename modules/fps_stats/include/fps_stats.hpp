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

#ifndef MODULES_FPS_STATS_HPP_
#define MODULES_FPS_STATS_HPP_
/**
 *  \file fps_statistics.hpp
 *
 *  This file contains a declaration of fps_statistics
 */
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"

namespace cnstream {
class FpsStats : public Module, public ModuleCreator<FpsStats> {
 public:
  /**
   * @brief Construct FpsStats object with a given moduleName
   * @param
   * 	moduleName[in]:defined module name
   */
  explicit FpsStats(const std::string &moduleName);
  /**
   * @brief Deconstruct FpsStats object
   *
   */
  ~FpsStats();

  /**
   * @brief Called by pipeline when pipeline start.
   * @param
   * @return
   *    true if paramSet are supported and valid, othersize false
   */
  bool Open(ModuleParamSet paramSet) override;
  /**
   * @brief Called by pipeline when pipeline stop.
   */
  void Close() override;
  /**
   * @brief do stats for each frame
   * @param
   *   data[in]: data to be processed.
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

 public:
  void ShowStatistics();

 private:
  static const int MAX_STREAM_NUM = 64;
  struct {
    std::mutex mutex_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
    std::chrono::time_point<std::chrono::high_resolution_clock> end_time_;
    std::string stream_id_;
    uint64_t frame_count_ = 0;
    void update(std::shared_ptr<CNFrameInfo> data) {
      if (stream_id_.empty()) {
        stream_id_ = data->frame.stream_id;
        start_time_ = std::chrono::high_resolution_clock::now();
        end_time_ = std::chrono::high_resolution_clock::now();
      }
      if (!(data->frame.flags & CN_FRAME_FLAG_EOS)) {
        ++frame_count_;
        end_time_ = std::chrono::high_resolution_clock::now();
      }
    }
    double fps() {
      std::chrono::duration<double, std::milli> diff = end_time_ - start_time_;
      if (diff.count()) {
        return (frame_count_ * 1000 * 1.f / diff.count());
      }
      return 0.0f;
    }
  } stream_fps_[MAX_STREAM_NUM];
};  // class FpsStats

}  // namespace cnstream

#endif  // MODULES_FPS_STATS_HPP_
