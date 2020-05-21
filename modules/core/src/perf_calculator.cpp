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

#include "perf_calculator.hpp"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include "glog/logging.h"
#include "cnstream_time_utility.hpp"
#include "perf_manager.hpp"
#include "sqlite_db.hpp"

namespace cnstream {

void PrintLatency(const PerfStats& stats) {
  std::cout << " -- [latency] avg : " << stats.latency_avg / 1000 << "."
            << std::setw(3) << std::setfill('0') << stats.latency_avg % 1000
            << " ms, max : " << stats.latency_max / 1000 << "."
            << std::setw(3) << std::setfill('0') << stats.latency_max % 1000
            << " ms, [frame cnt] : " << stats.frame_cnt << std::endl;
}

void PrintThroughput(const PerfStats& stats) {
  std::cout << " -- [fps] : " << std::fixed << std::setprecision(1) << stats.fps
            << ", [frame cnt] : " << stats.frame_cnt << std::endl;
}

void PrintPerfStats(const PerfStats& stats) {
  std::cout << " -- [fps] : " << std::fixed << std::setprecision(1) << stats.fps
            << ", [latency] avg : " << stats.latency_avg / 1000 << "."
            << std::setw(3) << std::setfill('0') << stats.latency_avg % 1000
            << " ms, max : " << stats.latency_max / 1000 << "."
            << std::setw(3) << std::setfill('0') << stats.latency_max % 1000
            << " ms, [frame cnt] : " << stats.frame_cnt << std::endl;
}

PerfCalculator::PerfCalculator() {
  pre_time_ = TimeStamp::Current();
}

PerfCalculator::~PerfCalculator() { }

#ifdef HAVE_SQLITE
static int LatencyCallback(void *data, int argc, char **argv, char **azColName) {
  PerfStats* stats = reinterpret_cast<PerfStats*>(data);
  size_t &latency_total = stats->latency_avg;

  size_t start_time = argv[0] ? atoll(argv[0]) : 0;
  size_t end_time = argv[1] ? atoll(argv[1]) : 0;

  if (end_time != 0 && start_time !=0 && end_time > start_time) {
    size_t duration = end_time - start_time;
    if (duration > stats->latency_max) {
      stats->latency_max = duration;
    }
    latency_total += duration;
    stats->frame_cnt += 1;
  }
  return 0;
}
#endif

PerfStats PerfCalculator::CalcLatency(std::shared_ptr<Sqlite> sql, std::string type,
                                      std::string start_key, std::string end_key) {
  if (sql == nullptr) {
    LOG(ERROR) << "sqlite is nullptr.";
    return stats_;
  }

#ifdef HAVE_SQLITE
  PerfStats stats = {0, 0, 0, 0.f};
  size_t now = sql->FindMax(type, end_key);
  std::string condition = end_key + " > " + std::to_string(pre_time_) + " AND " +
                          end_key + " <= " + std::to_string(now);
  sql->Select(type, start_key + "," + end_key, condition, LatencyCallback, reinterpret_cast<void*>(&stats));

  if (stats.frame_cnt != 0) {
    size_t &latency_total = stats.latency_avg;
    stats_.latency_avg = (stats_.latency_avg * stats_.frame_cnt + latency_total) / (stats_.frame_cnt + stats.frame_cnt);
    if (stats.latency_max > stats_.latency_max) {
      stats_.latency_max = stats.latency_max;
    }
    stats_.frame_cnt += stats.frame_cnt;
  }
  pre_time_ = now;
#endif
  return stats_;
}

PerfStats PerfCalculator::CalcThroughput(std::shared_ptr<Sqlite> sql, std::string type,
                                         std::string start_node, std::string end_node) {
  PerfStats stats = {0, 0, 0, 0.f};
  if (sql == nullptr) {
    LOG(ERROR) << "sqlite is nullptr.";
    return stats;
  }
#ifdef HAVE_SQLITE
  size_t frame_cnt = sql->Count(type, end_node);
  size_t start = sql->FindMin(type, start_node);
  size_t end = sql->FindMax(type, end_node);
  if (end > start) {
    size_t interval =  end - start;
    stats.fps = ceil(static_cast<double>(frame_cnt) * 1e7 / interval) / 10.0;
    stats.frame_cnt = frame_cnt;
  }
#endif
  return stats;
}

}  // namespace cnstream
