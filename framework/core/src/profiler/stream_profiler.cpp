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

#include <string>
#include <utility>

#include "profiler/stream_profiler.hpp"

namespace cnstream {

StreamProfiler::StreamProfiler(const std::string& stream_name)
    : stream_name_(stream_name) {}

StreamProfile StreamProfiler::GetProfile() {
  StreamProfile profile;
  profile.stream_name = GetName();
  profile.completed = completed_;
  profile.dropped = dropped_;
  profile.counter = profile.completed + profile.dropped;
  double total_latency_ms = total_latency_.count();
  double total_phy_time_ms = total_phy_time_.count();
  profile.latency = -1;
  profile.fps = -1;
  if (total_phy_time_ms)
    profile.fps = 1e3 / total_phy_time_ms * profile.counter;
  if (latency_add_times_) {
    profile.latency = total_latency_ms / latency_add_times_;
    profile.maximum_latency = maximum_latency_.count();
    profile.minimum_latency = minimum_latency_.count();
  }
  return profile;
}

}  // namespace cnstream
