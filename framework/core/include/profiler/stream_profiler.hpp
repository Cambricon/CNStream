/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#ifndef CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_STREAM_PROFILER_HPP_
#define CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_STREAM_PROFILER_HPP_

#include <algorithm>
#include <chrono>
#include <string>
#include "profiler/profile.hpp"

namespace cnstream {

/**
 * StreamProfiler is responsible for the performance statistics of a certain processing process of a stream.
 * It is used by ProcessProfiler.
 * 
 * @see ProcessProfiler.
 **/
class StreamProfiler {
  using Duration = std::chrono::duration<double, std::milli>;

 public:
  explicit StreamProfiler(const std::string& stream_name);

  StreamProfiler& AddLatency(const Duration& latency);

  StreamProfiler& UpdatePhysicalTime(const Duration& time);

  StreamProfiler& AddDropped(uint64_t dropped);

  StreamProfiler& AddCompleted();

  std::string GetName() const;

  StreamProfile GetProfile();

 private:
  std::string stream_name_ = "";
  uint64_t completed_ = 0;
  uint64_t latency_add_times_ = 0;
  uint64_t dropped_ = 0;
  Duration total_latency_ = Duration::zero();
  Duration maximum_latency_      = Duration::zero();
  Duration minimum_latency_      = Duration::max();
  Duration total_phy_time_ = Duration::zero();
};  // class StreamProfiler

inline StreamProfiler& StreamProfiler::AddLatency(const Duration& latency) {
  latency_add_times_++;
  total_latency_ += latency;
  maximum_latency_ = std::max(latency, maximum_latency_);
  minimum_latency_ = std::min(latency, minimum_latency_);
  return *this;
}

inline StreamProfiler& StreamProfiler::UpdatePhysicalTime(const Duration& time) {
  total_phy_time_ = time;
  return *this;
}

inline StreamProfiler& StreamProfiler::AddDropped(uint64_t dropped) {
  dropped_ += dropped;
  return *this;
}

inline StreamProfiler& StreamProfiler::AddCompleted() {
  completed_++;
  return *this;
}

inline std::string StreamProfiler::GetName() const {
  return stream_name_;
}

}  // namespace cnstream

#endif  // CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_STREAM_PROFILER_HPP_
