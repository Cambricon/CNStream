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
#include <mutex>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "cnstream_time_utility.hpp"
#include "perf_manager.hpp"
#include "sqlite_db.hpp"

namespace cnstream {

std::mutex latency_mutex;
std::mutex fps_mutex;

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

PerfCalculator::PerfCalculator() { }

PerfCalculator::~PerfCalculator() { }

#ifdef HAVE_SQLITE
static int Callback(void *data, int argc, char **argv, char **azColName) {
  std::vector<DbItem>* item_vec = reinterpret_cast<std::vector<DbItem>*>(data);
  DbItem item;
  for (int i = 0; i < argc; i++) {
    if (argv[i]) {
      item.second.push_back(argv[i]);
    } else {
      item.second.push_back("");
    }
  }
  item.first = argc;
  item_vec->push_back(item);
  return 0;
}
#endif

PerfStats PerfCalculator::GetLatency() {
  std::lock_guard<std::mutex> lg(latency_mutex);
  return stats_latency_;
}

PerfStats PerfCalculator::GetThroughput() {
  std::lock_guard<std::mutex> lg(fps_mutex);
  return stats_fps_;
}

std::vector<DbItem> PerfCalculator::SearchFromDatabase(std::shared_ptr<Sqlite> sql, std::string table,
                                                       std::string key, std::string condition) {
  if (sql == nullptr) {
    LOG(ERROR) << "sqlite is nullptr.";
  }
  std::vector<DbItem> item_vec;
#ifdef HAVE_SQLITE
  sql->Select(table, key, condition, Callback, reinterpret_cast<void*>(&item_vec));
#endif
  return item_vec;
}


PerfStats PerfCalculator::CalcLatency(std::shared_ptr<Sqlite> sql, std::string type,
                                      std::string start_key, std::string end_key) {
  std::lock_guard<std::mutex> lg(latency_mutex);
  if (sql == nullptr) {
    LOG(ERROR) << "sqlite is nullptr.";
    return stats_latency_;
  }

#ifdef HAVE_SQLITE
  size_t now = sql->FindMax(type, end_key);
  std::string condition = end_key + " > " + std::to_string(pre_time_) + " AND " +
                          end_key + " <= " + std::to_string(now);
  std::vector<DbItem> item_vec = SearchFromDatabase(sql, type, start_key + "," + end_key, condition);

  size_t latency_total = 0;
  size_t frame_cnt = 0;
  for (auto it : item_vec) {
    size_t start_time = it.second[0] != "" ? atoll(it.second[0].c_str()) : 0;
    size_t end_time = it.second[1] != "" ? atoll(it.second[1].c_str()) : 0;

    if (end_time != 0 && start_time !=0 && end_time > start_time) {
      size_t duration = end_time - start_time;
      if (duration > stats_latency_.latency_max) {
        stats_latency_.latency_max = duration;
      }
      latency_total += duration;
      frame_cnt += 1;
    }
  }
  if (frame_cnt != 0) {
    stats_latency_.latency_avg = (stats_latency_.latency_avg * stats_latency_.frame_cnt + latency_total) /
                                 (stats_latency_.frame_cnt + frame_cnt);
    stats_latency_.frame_cnt += frame_cnt;
  }
  pre_time_ = now;
#endif
  return stats_latency_;
}

PerfStats PerfCalculator::CalcThroughputByEachFrameTime(std::shared_ptr<Sqlite> sql, std::string type,
                                                        std::string start_key, std::string end_key) {
  std::lock_guard<std::mutex> lg(latency_mutex);
  if (sql == nullptr) {
    LOG(ERROR) << "sqlite is nullptr.";
    return stats_fps_;
  }

#ifdef HAVE_SQLITE
  size_t now = sql->FindMax(type, end_key);
  std::string condition = end_key + " > " + std::to_string(pre_time_) + " AND " +
                          end_key + " <= " + std::to_string(now) + " AND " +
                          start_key + " > 0";
  std::vector<DbItem> item_vec = SearchFromDatabase(sql, type, start_key + "," + end_key, condition);
  if (pre_end_time_ == 0 || pre_end_time_ == ~((size_t)0)) {
    pre_end_time_ = sql->FindMin(type, start_key);
  }

  size_t pre_frame_end_time = pre_end_time_;
  size_t total_time = 0;
  size_t frame_cnt = 0;
  for (auto it : item_vec) {
    size_t start_time = it.second[0] != "" ? atoll(it.second[0].c_str()) : 0;
    size_t end_time = it.second[1] != "" ? atoll(it.second[1].c_str()) : 0;

    if (end_time != 0 && start_time !=0 && end_time > start_time) {
      size_t duration = end_time - (start_time > pre_frame_end_time ? start_time : pre_frame_end_time);
      total_time += duration;
      frame_cnt += 1;
      pre_frame_end_time = end_time;
    }
  }
  if (frame_cnt != 0) {
    size_t& total = stats_fps_.latency_max;
    stats_fps_.frame_cnt += frame_cnt;
    total += total_time;
    if (total != 0) {
      stats_fps_.fps = ceil(stats_fps_.frame_cnt * 1e7 / total) / 10;
    } else {
      stats_fps_.fps = 0;
    }
  }
  pre_time_ = now;
  pre_end_time_ = pre_frame_end_time;
#endif
  return stats_fps_;
}

PerfStats PerfCalculator::CalcThroughputByTotalTime(std::shared_ptr<Sqlite> sql, std::string type,
                                                    std::string start_key, std::string end_key) {
  std::lock_guard<std::mutex> lg(fps_mutex);
  if (sql == nullptr) {
    LOG(ERROR) << "sqlite is nullptr.";
    return stats_fps_;
  }
#ifdef HAVE_SQLITE
  size_t frame_cnt = sql->Count(type, end_key);
  size_t start = sql->FindMin(type, start_key);
  size_t end = sql->FindMax(type, end_key);
  if (end > start) {
    size_t interval =  end - start;
    stats_fps_.fps = ceil(static_cast<double>(frame_cnt) * 1e7 / interval) / 10.0;
    stats_fps_.frame_cnt = frame_cnt;
  }
#endif
  return stats_fps_;
}

}  // namespace cnstream
