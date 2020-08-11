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

#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "glog/logging.h"
#include "perf_manager.hpp"
#include "sqlite_db.hpp"
#include "util/cnstream_time_utility.hpp"

namespace cnstream {

void PrintLatency(const PerfStats &stats, uint32_t width) {
  std::cout << std::right << "  -- [latency] avg: " << std::setw(4) << std::setfill(' ')
            << stats.latency_avg / 1000 << "." << stats.latency_avg % 1000 / 100
            << "ms, min: " << std::setw(4) << std::setfill(' ')
            << stats.latency_min / 1000 << "." << stats.latency_min % 1000 / 100
            << "ms, max: " << std::setw(4) << std::setfill(' ')
            << stats.latency_max / 1000 << "." << stats.latency_max % 1000 / 100
            << "ms, [frame count]: " << std::setw(width) << std::setfill(' ') << stats.frame_cnt << std::endl;
}

void PrintThroughput(const PerfStats &stats, uint32_t width) {
  std::cout << std::right << "  -- [fps]: " << std::setw(6) << std::setfill(' ')
            << std::fixed << std::setprecision(1) << stats.fps << ", [frame count]: "
            << std::setw(width) << std::setfill(' ') << stats.frame_cnt << std::endl;
  std::cout.unsetf(std::ios::fixed);
}

void PrintStreamId(std::string stream_id) {
  std::cout << "[stream id] " << std::left << std::setw(9) << std::setfill(' ') << stream_id;
}

void PrintStr(const std::string str, uint32_t width, const char fill_charater) {
  std::cout << std::left << std::setw(width) << std::setfill(fill_charater) << str;
}

void PrintTitle(std::string title) {
  std::cout << "\033[32m" << std::endl;
  PrintStr("===================================[ " + title + " ]", 96, '=');
  std::cout << "\033[0m" << std::endl;
}

void PrintTitleForLatestThroughput(const std::string timeframe) {
  std::cout << std::endl;
  PrintStr("-", 56, '-');
  std::cout << "\nThroughput over the last " << timeframe << std::endl;
}

void PrintTitleForAverageThroughput() {
  std::cout << std::endl;
  PrintStr("-", 56, '-');
  std::cout << "\nAverage throughput over the process" << std::endl;
}

void PrintTitleForTotal() {
  std::cout << std::endl;
  PrintStr("Total : ");
}

// --- PerfCalculator  --- //
PerfStats PerfCalculator::GetLatency(const std::string &sql_name, const std::string &perf_type) {
  PerfStats stats;
  std::string map_key = sql_name + "_" + perf_type + "_latency";

  std::lock_guard<std::mutex> lg(latency_mutex_);
  if (stats_latency_map_.find(map_key) == stats_latency_map_.end()) {
    LOG(WARNING) << "Can not find latency for " << map_key;
  } else {
    stats = stats_latency_map_.at(map_key);
  }

  return stats;
}

std::vector<PerfStats> PerfCalculator::GetThroughput(const std::string &sql_name, const std::string &perf_type) {
  std::vector<PerfStats> throughput;
  std::string map_key = sql_name + "_" + perf_type + "_throughput";

  std::lock_guard<std::mutex> lg(fps_mutex_);
  if (throughput_.find(map_key) == throughput_.end()) {
    LOG(ERROR) << "Can not find throughput for " << map_key;
  } else {
    throughput = {throughput_.at(map_key)};
  }

  return throughput;
}

PerfStats PerfCalculator::CalcAvgThroughput(const std::vector<PerfStats> &stats_vec) {
  PerfStats stats;
  for (auto& it : stats_vec) {
    stats.frame_cnt += it.frame_cnt;
    stats.latency_max += it.latency_max;
  }
  if (stats.latency_max != 0) {
    stats.fps = ceil(stats.frame_cnt * 1e7 / stats.latency_max) / 10;
  }
  return stats;
}

PerfStats PerfCalculator::GetAvgThroughput(const std::string &sql_name, const std::string &perf_type) {
  PerfStats throughput;
  std::string map_key = sql_name + "_" + perf_type + "_throughput";

  std::lock_guard<std::mutex> lg(fps_mutex_);
  if (throughput_.find(map_key) == throughput_.end()) {
    LOG(ERROR) << "Can not find throughput for " << map_key;
  } else {
    throughput = throughput_.at(map_key);
  }

  return throughput;
}


PerfStats PerfCalculator::CalculateFinalThroughput(const std::string &sql_name,
    const std::string &perf_type, const std::vector<std::string> &keys) {
  std::vector<PerfStats> throughput_vec;
  PerfStats stats;
  do {
    stats = CalcThroughput(sql_name, perf_type, keys);
    throughput_vec.push_back(stats);
  } while (stats.frame_cnt);

  return CalcAvgThroughput(throughput_vec);;
}

void PerfCalculator::RemoveLatency(const std::string &sql_name, const std::string &perf_type) {
  std::lock_guard<std::mutex> lg(latency_mutex_);
  std::string map_key = sql_name + "_" + perf_type + "_latency";
  if (stats_latency_map_.find(map_key) != stats_latency_map_.end()) {
    stats_latency_map_.erase(map_key);
  }
  if (pre_time_map_.find(map_key) != pre_time_map_.end()) {
    pre_time_map_.erase(map_key);
  }
}

void PerfCalculator::RemovePerfStats(const std::string &sql_name, const std::string &perf_type,
                                     const std::string &key) {
  RemoveLatency(sql_name, perf_type);
}

bool PerfCalculator::CreateDbForStoreUnprocessedData(const std::string &db_name, const std::string &perf_type,
    const std::string &module_name, const std::vector<std::string> &suffixes) {
  std::vector<std::string> keys = PerfManager::GetKeys({module_name}, suffixes);
  std::shared_ptr<Sqlite> sql = PerfUtils::CreateDb(db_name);
  if (sql == nullptr) {
    LOG(ERROR) << "Create Database failed. Database name is " << db_name;
    return false;
  }
  if (!PerfUtils::CreateTable(sql, perf_type, "", keys)) {
    LOG(ERROR) << "Create database table failed. Database name is " << db_name;
    return false;
  }
  return perf_utils_->AddSql("_" + perf_type + "_throughput", sql);
}

PerfStats PerfCalculator::CalcLatency(const std::string &sql_name, const std::string &perf_type,
                                      const std::vector<std::string> &keys) {
  PerfStats stats;
  const std::string map_key = sql_name + "_" + perf_type + "_latency";

  if (keys.size() != 2 && keys.size() != 3) {
    LOG(ERROR) << "[Calc Latency] Please provide two or three keys for calculation.";
    return PerfStats();
  }
  const std::string &end_key = keys[1];

  if (stats_latency_map_.find(map_key) == stats_latency_map_.end()) {
    stats_latency_map_[map_key] = stats;
  }
  PerfStats &stats_latency = stats_latency_map_[map_key];

  if (pre_time_map_.find(map_key) == pre_time_map_.end()) {
    pre_time_map_[map_key] = 0;
  }
  size_t &pre_time = pre_time_map_[map_key];

  size_t now = perf_utils_->FindMaxValue(sql_name, perf_type, end_key);

  // Get data
  std::string condition = end_key + " > " + std::to_string(pre_time) + " AND " + end_key + " <= " + std::to_string(now);

  std::vector<DbItem> item_vec = perf_utils_->GetItems(sql_name, perf_type, keys, condition);

  std::vector<DbIntegerItem> integer_item_vec = perf_utils_->ToInteger(item_vec);

  // Calculate latency
  stats = method_->CalcLatency(integer_item_vec);

  if (stats.frame_cnt > 0) {
    // Update latency
    if (stats.latency_max > stats_latency.latency_max) {
      stats_latency.latency_max = stats.latency_max;
    }
    if (stats_latency.latency_min == 0 || stats.latency_min < stats_latency.latency_min) {
      stats_latency.latency_min = stats.latency_min;
    }

    size_t old_total_time = stats_latency.latency_avg * stats_latency.frame_cnt;
    size_t additional_total_time = stats.latency_avg * stats.frame_cnt;
    stats_latency.frame_cnt += stats.frame_cnt;
    stats_latency.latency_avg = (old_total_time + additional_total_time) / stats_latency.frame_cnt;
  }

  pre_time = now;
  return stats_latency;
}

// --- PerfCalculatorForModule  --- //
void PerfCalculatorForModule::RemovePerfStats(const std::string &sql_name, const std::string &perf_type,
                                              const std::string &key) {
  RemoveLatency(sql_name, perf_type);
  StoreUnprocessedData(sql_name, perf_type, key);
}

void PerfCalculatorForModule::StoreUnprocessedData(const std::string &sql_name, const std::string &perf_type,
                                                   const std::string &key) {
  std::string thread_key = key + PerfManager::GetThreadSuffix();
  std::string start_key = key + PerfManager::GetStartTimeSuffix();
  std::string end_key = key + PerfManager::GetEndTimeSuffix();

  std::set<std::string> th_ids = perf_utils_->GetThreadIdFromAllDb(perf_type, thread_key);
  for (auto th_id : th_ids) {
    std::string map_key =  th_id + "_" + perf_type + "_throughput";
    if (pre_time_map_.find(map_key) == pre_time_map_.end()) {
      pre_time_map_[map_key] = 0;
    }
    size_t pre_time = pre_time_map_[map_key];
    std::string condition = end_key + " > " + std::to_string(pre_time) + " and " + thread_key + " = '" + th_id + "'";
    std::vector<DbItem> items = perf_utils_->GetItems(sql_name, perf_type, {start_key, end_key, thread_key}, condition);
    for (auto it : items) {
      if (it.second.size() != 3) { continue; }
      it.second[2] = "'" + it.second[2] + "'";
      perf_utils_->Record("_" + perf_type + "_throughput", perf_type, {start_key, end_key, thread_key}, it.second);
    }
  }
}

PerfStats PerfCalculatorForModule::CalcThroughput(const std::string &sql_name, const std::string &perf_type,
                                                  const std::vector<std::string> &keys) {
  if (keys.size() != 3) {
    LOG(ERROR) << "[Calc Throughput] Please provide three keys for calculation.";
    return PerfStats();
  }
  const std::string &start_key = keys[0];
  const std::string &end_key = keys[1];
  const std::string &thread_key = keys[2];

  std::set<std::string> th_ids = perf_utils_->GetThreadIdFromAllDb(perf_type, thread_key);

  std::vector<double> module_fps_vec;
  size_t frame_cnts = 0;

  std::vector<std::pair<std::string, PerfStats>> latest_fps;
  std::vector<uint32_t> latest_frame_cnt_digit;
  for (auto th_id : th_ids) {
    std::string map_key = th_id + "_" + perf_type + "_throughput";
    if (pre_time_map_.find(map_key) == pre_time_map_.end()) {
      pre_time_map_[map_key] = 0;
    }
    size_t &pre_time = pre_time_map_[map_key];

    if (pre_time == 0 || pre_time == ~((size_t)0)) {
      std::vector<size_t> min_values =
          perf_utils_->FindMinValues(perf_type, start_key, thread_key + "='" + th_id + "'");
      pre_time = PerfUtils::Min<size_t>(min_values);
    }

    std::vector<size_t> max_values = perf_utils_->FindMaxValues(
        perf_type, end_key, thread_key + "='" + th_id + "' and " + end_key + " > " + std::to_string(pre_time));
    size_t now = ~(size_t(0));
    for (auto it : max_values) {
      if (it != 0 && it < now) {
        now = it;
      }
    }
    if (now == ~(size_t(0))) {
      now = 0;
    }

    // Get Data
    std::vector<DbItem> data =
        perf_utils_->GetItemsFromAllDb(perf_type, {start_key, end_key},
                                       thread_key + "='" + th_id + "' and " + end_key + " <= " + std::to_string(now) +
                                           " and " + end_key + " > " + std::to_string(pre_time));

    std::vector<DbIntegerItem> integer_data = perf_utils_->ToInteger(data);
    PerfUtils::Sort(&integer_data,
                    [](const std::vector<size_t> &lhs, const std::vector<size_t> &rhs) { return lhs[1] < rhs[1]; });

    // Calculate throughput
    auto stats = method_->CalcThroughput(pre_time, integer_data);
    latest_fps.push_back(std::make_pair(th_id, stats));
    latest_frame_cnt_digit.push_back(std::to_string(stats.frame_cnt).length());

    {
      std::lock_guard<std::mutex> lg(fps_mutex_);
      throughput_[map_key] = CalcAvgThroughput({throughput_[map_key], stats});
    }

    module_fps_vec.push_back(stats.fps);

    if (now != 0) {
      pre_time = now;
    }
    frame_cnts += stats.frame_cnt;
  }

  if (print_throughput_) {
    uint32_t max_digit = PerfUtils::Max(latest_frame_cnt_digit);
    for (auto& it : latest_fps) {
      std::cout << std::left << std::setw(15) << std::setfill(' ') << it.first;
      PrintThroughput(it.second, max_digit);
    }
  }
  // Sum throughput of all threads
  PerfStats total_stats;
  total_stats.frame_cnt = frame_cnts;
  total_stats.fps = PerfUtils::Sum(module_fps_vec);
  if (total_stats.fps > 1e-6) {
    total_stats.latency_max = frame_cnts * 1e6 / total_stats.fps;
  }
  std::string total_map_key = "_" + perf_type + "_throughput";
  {
    std::lock_guard<std::mutex> lg(fps_mutex_);
    throughput_[total_map_key] = CalcAvgThroughput({throughput_[total_map_key], total_stats});
  }

  return total_stats;
}

// --- PerfCalculatorForPipeline  --- //
void PerfCalculatorForPipeline::RemoveThroughput(const std::string &sql_name, const std::string &perf_type) {
  std::lock_guard<std::mutex> lg(fps_mutex_);
  std::string map_key = sql_name + "_" + perf_type + "_throughput";
  if (throughput_.find(map_key) != throughput_.end()) {
    throughput_.erase(map_key);
  }
  if (pre_time_map_.find(map_key) != pre_time_map_.end()) {
    pre_time_map_.erase(map_key);
  }
}

void PerfCalculatorForPipeline::RemovePerfStats(const std::string &sql_name, const std::string &perf_type,
                                                const std::string &key) {
  RemoveLatency(sql_name, perf_type);
  RemoveThroughput(sql_name, perf_type);
  StoreUnprocessedData(sql_name, perf_type, key);
}

void PerfCalculatorForPipeline::StoreUnprocessedData(const std::string &sql_name, const std::string &perf_type,
                                                     const std::string &key) {
  std::string map_key =  "_" + perf_type + "_throughput";
  if (pre_time_map_.find(map_key) == pre_time_map_.end()) {
    pre_time_map_[map_key] = 0;
  }
  size_t pre_time = pre_time_map_[map_key];
  std::string condition = key + " > " + std::to_string(pre_time);
  std::vector<DbItem> items = perf_utils_->GetItems(sql_name, perf_type, {key}, condition);
  for (auto it : items) {
    if (it.second.size() != 1) { continue; }
    perf_utils_->Record(map_key, perf_type, {key}, it.second);
  }
}

PerfStats PerfCalculatorForPipeline::CalcThroughput(const std::string &sql_name, const std::string &perf_type,
                                                    const std::vector<std::string> &keys) {
  if (keys.size() != 1) {
    LOG(ERROR) << "[Calc Throughput] Please provide one key for calculation.";
    return PerfStats();
  }
  const std::string &end_key = keys[0];
  std::string map_key = sql_name + "_" + perf_type + "_throughput";

  if (pre_time_map_.find(map_key) == pre_time_map_.end()) {
    pre_time_map_[map_key] = 0;
  }
  size_t &pre_time = pre_time_map_[map_key];

  bool first = false;
  size_t frame_cnt;
  size_t end_time;

  if (sql_name != "") {
    // calculate throughput of one stream
    if (pre_time == 0 || pre_time == ~((size_t)0)) {
      pre_time = perf_utils_->FindMinValue(sql_name, perf_type, end_key);
      first = true;
    }

    end_time = perf_utils_->FindMaxValue(sql_name, perf_type, end_key);

    if (first) {
      frame_cnt = perf_utils_->GetCount(
          sql_name, perf_type, end_key,
          end_key + " >= " + std::to_string(pre_time) + " and " + end_key + " <=" + std::to_string(end_time));
    } else {
      frame_cnt = perf_utils_->GetCount(
          sql_name, perf_type, end_key,
          end_key + " > " + std::to_string(pre_time) + " and " + end_key + " <=" + std::to_string(end_time));
    }
  } else {
    // calculate throughput of all streams
    if (pre_time == 0 || pre_time == ~((size_t)0)) {
      std::vector<size_t> min_values = perf_utils_->FindMinValues(perf_type, end_key);
      pre_time = PerfUtils::Min(min_values);
      first = true;
    }

    std::vector<size_t> max_values;
    if (first) {
      max_values = perf_utils_->FindMaxValues(perf_type, end_key, end_key + " >= " + std::to_string(pre_time));
    } else {
      max_values = perf_utils_->FindMaxValues(perf_type, end_key, end_key + " > " + std::to_string(pre_time));
    }

    end_time = ~(0);
    for (auto it : max_values) {
      if (it != 0 && it < end_time) {
        end_time = it;
      }
    }
    if (end_time == ~(size_t(0))) {
      end_time = 0;
    }

    std::vector<size_t> frame_cnts;
    if (first) {
      frame_cnts = perf_utils_->GetCountFromAllDb(
          perf_type, end_key,
          end_key + " >= " + std::to_string(pre_time) + " and " + end_key + " <=" + std::to_string(end_time));
    } else {
      frame_cnts = perf_utils_->GetCountFromAllDb(
          perf_type, end_key,
          end_key + " > " + std::to_string(pre_time) + " and " + end_key + " <=" + std::to_string(end_time));
    }
    frame_cnt = PerfUtils::Sum(frame_cnts);
  }

  // Calculate throughput
  PerfStats stats = method_->CalcThroughput(pre_time, end_time, frame_cnt);
  if (end_time > pre_time) {
    stats.latency_max = end_time - pre_time;
  }

  {
    std::lock_guard<std::mutex> lg(fps_mutex_);
    throughput_[map_key] = CalcAvgThroughput({throughput_[map_key], stats});
  }

  if (end_time > 0) {
    pre_time = end_time;
  }
  return stats;
}

// --- PerfCalculatorForInfer  --- //
PerfStats PerfCalculatorForInfer::CalcThroughput(const std::string &sql_name, const std::string &perf_type,
                                                 const std::vector<std::string> &keys) {
  PerfStats stats;

  if (keys.size() != 3 && keys.size() != 2) {
    LOG(ERROR) << "[Calc Throughput] Please provide two or three keys for calculation.";
    return stats;
  }
  const std::string &start_key = keys[0];
  const std::string &end_key = keys[1];

  std::string map_key = sql_name + "_" + perf_type + "_throughput";

  if (pre_time_map_.find(map_key) == pre_time_map_.end()) {
    pre_time_map_[map_key] = 0;
  }
  size_t &pre_time = pre_time_map_[map_key];

  std::vector<DbIntegerItem> integer_item;
  size_t now;
  if (perf_type != "") {
    // calculate data in one table
    if (pre_time == 0 || pre_time == ~((size_t)0)) {
      pre_time = perf_utils_->FindMinValue(sql_name, perf_type, start_key);
    }
    now = perf_utils_->FindMaxValue(sql_name, perf_type, end_key);

    // Get Data
    std::string condition = end_key + " > " + std::to_string(pre_time) + " AND " + end_key +
                            " <= " + std::to_string(now) + " AND " + start_key + " > 0";

    std::vector<DbItem> item_vec = perf_utils_->GetItems(sql_name, perf_type, keys, condition);
    integer_item = PerfUtils::ToInteger(item_vec);
  } else {
    // perf type is empty, so we will calculate all perf types data, i.e., all tables data
    std::vector<std::string> thread_ids = perf_utils_->GetTableNames(sql_name);

    if (pre_time == 0 || pre_time == ~((size_t)0)) {
      std::vector<size_t> min_values;
      for (auto thread_id : thread_ids) {
        min_values.push_back(perf_utils_->FindMinValue(sql_name, thread_id, start_key));
      }
      pre_time = PerfUtils::Min(min_values);
    }

    std::vector<size_t> max_values;
    for (auto thread_id : thread_ids) {
      max_values.push_back(perf_utils_->FindMaxValue(sql_name, thread_id, end_key));
    }
    now = PerfUtils::Max(max_values);

    // Get Data
    std::string condition = end_key + " > " + std::to_string(pre_time) + " AND " + end_key +
                            " <= " + std::to_string(now) + " AND " + start_key + " > 0";

    std::vector<DbItem> item_vec;
    for (auto thread_id : thread_ids) {
      std::vector<DbItem> item = perf_utils_->GetItems(sql_name, thread_id, keys, condition);

      item_vec.reserve(item_vec.size() + item.size());
      item_vec.insert(item_vec.end(), item.begin(), item.end());
    }
    integer_item = PerfUtils::ToInteger(item_vec);
    PerfUtils::Sort(&integer_item,
                    [](const std::vector<size_t> &lhs, const std::vector<size_t> &rhs) { return lhs[1] < rhs[1]; });
  }

  // Calculate throughput
  stats = method_->CalcThroughput(pre_time, integer_item);

  {
    std::lock_guard<std::mutex> lg(fps_mutex_);
    throughput_[map_key] = CalcAvgThroughput({throughput_[map_key], stats});
  }

  pre_time = now;

  return stats;
}

// --- PerfCalculationMethod  --- //
PerfStats PerfCalculationMethod::CalcThroughput(size_t start_time, const std::vector<DbIntegerItem> &item_vec) {
  PerfStats stats;
  size_t total_time = 0;
  size_t frame_cnt = 0;
  for (auto it : item_vec) {
    if (it.size() >= 2) {
      size_t &item_start_time = it[0];
      size_t &item_end_time = it[1];

      if (item_end_time != 0 && item_start_time != 0 && item_end_time > item_start_time) {
        size_t duration = item_end_time - (item_start_time > start_time ? item_start_time : start_time);
        total_time += duration;
        if (it.size() == 3) {
          size_t &cnt = it[2];
          frame_cnt += cnt;
        } else {
          frame_cnt += 1;
        }
        start_time = item_end_time;
      }
    }
    stats.frame_cnt = frame_cnt;
    if (frame_cnt != 0) {
      if (total_time != 0) {
        stats.fps = ceil(stats.frame_cnt * 1e7 / total_time) / 10;
      } else {
        stats.fps = 0;
      }
      stats.latency_max = total_time;
    }
  }
  return stats;
}

PerfStats PerfCalculationMethod::CalcThroughput(size_t start_time, size_t end_time, size_t frame_cnt) {
  PerfStats stats;
  if (end_time > start_time) {
    size_t interval = end_time - start_time;
    stats.fps = ceil(frame_cnt * 1e7 / interval) / 10;
    stats.frame_cnt = frame_cnt;
  } else if (frame_cnt == 1 && start_time == end_time) {
    stats.frame_cnt = frame_cnt;
    stats.fps = 0;
  }
  return stats;
}

PerfStats PerfCalculationMethod::CalcLatency(const std::vector<DbIntegerItem> &item_vec) {
  PerfStats stats;
  stats.latency_min = ~(0);
  size_t latency_total = 0;
  size_t &frame_cnt = stats.frame_cnt;
  for (auto it : item_vec) {
    if (it.size() >= 2) {
      size_t &start_time = it[0];
      size_t &end_time = it[1];

      if (end_time != 0 && start_time != 0 && end_time > start_time) {
        size_t duration = end_time - start_time;
        if (duration > stats.latency_max) {
          stats.latency_max = duration;
        }
        if (duration < stats.latency_min) {
          stats.latency_min = duration;
        }
        if (it.size() == 3) {
          size_t &cnt = it[2];
          latency_total += duration * cnt;
          frame_cnt += cnt;
        } else {
          latency_total += duration;
          frame_cnt += 1;
        }
      }
    }
  }
  if (frame_cnt != 0) {
    stats.latency_avg = latency_total / frame_cnt;
  } else {
    stats.latency_min = 0;
  }
  return stats;
}

// --- PerfUtils  --- //
static int Callback(void *data, int argc, char **argv, char **azColName) {
  std::vector<DbItem> *item_vec = reinterpret_cast<std::vector<DbItem> *>(data);
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

bool PerfUtils::AddSql(const std::string &name, std::shared_ptr<Sqlite> sql) {
  std::lock_guard<std::mutex> lg(sql_map_lock_);
  if (SqlIsExisted(name)) {
    LOG(ERROR) << "Add sql handler failed. sql '" << name << "' exists.";
    return false;
  }
  if (name.empty()) {
    LOG(ERROR) << "Add sql handler failed, name is empty string";
    return false;
  }
  if (sql == nullptr) {
    LOG(ERROR) << "Add sql handler failed, sql pointer is nullptr";
    return false;
  }
  sql_map_[name] = sql;
  return true;
}

bool PerfUtils::RemoveSql(const std::string &name) {
  std::lock_guard<std::mutex> lg(sql_map_lock_);
  if (!SqlIsExisted(name)) {
    LOG(ERROR) << "Remove sql failed. sql '" << name << "' does not exist.";
    return false;
  }
  sql_map_.erase(name);
  return true;
}

std::vector<DbItem> PerfUtils::SearchFromDatabase(std::shared_ptr<Sqlite> sql, std::string sql_statement) {
  std::vector<DbItem> item_vec;
  if (sql == nullptr) {
    LOG(ERROR) << "sqlite is nullptr.";
    return item_vec;
  }
  sql->Select(sql_statement, Callback, reinterpret_cast<void *>(&item_vec));
  return item_vec;
}

std::vector<std::string> PerfUtils::GetSqlNames() {
  std::vector<std::string> sql_names;
  std::lock_guard<std::mutex> lg(sql_map_lock_);
  for (auto sql : sql_map_) {
    sql_names.push_back(sql.first);
  }
  return sql_names;
}

std::set<std::string> PerfUtils::GetThreadId(std::string name, std::string perf_type, std::string th_key) {
  std::set<std::string> ids;
  std::vector<DbItem> th_vec;
  {
    std::lock_guard<std::mutex> lg(sql_map_lock_);
    if (!SqlIsExisted(name)) {
      return ids;
    }

    std::string select_sql = " select distinct " + th_key + " from " + perf_type + ";";
    th_vec = SearchFromDatabase(sql_map_[name], select_sql);
  }
  for (auto it : th_vec) {
    if (!it.second[0].empty()) {
      ids.insert(it.second[0]);
    }
  }
  return ids;
}

std::set<std::string> PerfUtils::GetThreadIdFromAllDb(std::string perf_type, std::string th_key) {
  std::set<std::string> ids;
  for (auto sql_name : GetSqlNames()) {
    std::set<std::string> th_ids = GetThreadId(sql_name, perf_type, th_key);
    ids.insert(th_ids.begin(), th_ids.end());
  }
  return ids;
}

std::vector<DbItem> PerfUtils::GetItems(std::string name, std::string perf_type, std::vector<std::string> keys,
                                        std::string condition) {
  std::lock_guard<std::mutex> lg(sql_map_lock_);
  std::vector<DbItem> item_vec;
  if (!SqlIsExisted(name)) {
    return item_vec;
  }

  std::string key_str = "";
  for (auto key : keys) {
    key_str += key + ",";
  }
  key_str.pop_back();

  std::string select_sql = " select " + key_str + " from " + perf_type + " where " + condition + ";";
  item_vec = SearchFromDatabase(sql_map_[name], select_sql);
  return item_vec;
}

std::vector<DbItem> PerfUtils::GetItemsFromAllDb(std::string perf_type, std::vector<std::string> keys,
                                                 std::string condition) {
  std::vector<DbItem> items;
  for (auto sql_name : GetSqlNames()) {
    std::vector<DbItem> item_vec = GetItems(sql_name, perf_type, keys, condition);
    items.reserve(items.size() + item_vec.size());
    items.insert(items.end(), item_vec.begin(), item_vec.end());
  }
  return items;
}

std::vector<DbIntegerItem> PerfUtils::ToInteger(const std::vector<DbItem> &data) {
  std::vector<DbIntegerItem> integer_item_vec;
  for (auto item : data) {
    DbIntegerItem integer_item;
    for (auto element : item.second) {
      size_t integer_element = element != "" ? atoll(element.c_str()) : 0;
      integer_item.push_back(integer_element);
    }
    integer_item_vec.push_back(integer_item);
  }
  return integer_item_vec;
}

size_t PerfUtils::FindMaxValue(std::string name, std::string perf_type, std::string key, std::string condition) {
  std::lock_guard<std::mutex> lg(sql_map_lock_);
  if (!SqlIsExisted(name)) {
    return 0;
  }
  return sql_map_[name]->FindMax(perf_type, key, condition);
}

std::vector<size_t> PerfUtils::FindMaxValues(std::string perf_type, std::string key, std::string condition) {
  std::vector<size_t> max_values;
  for (auto sql_name : GetSqlNames()) {
    max_values.push_back(FindMaxValue(sql_name, perf_type, key, condition));
  }
  return max_values;
}

size_t PerfUtils::FindMinValue(std::string name, std::string perf_type, std::string key, std::string condition) {
  std::lock_guard<std::mutex> lg(sql_map_lock_);
  if (!SqlIsExisted(name)) {
    return 0;
  }
  return sql_map_[name]->FindMin(perf_type, key, condition);
}

std::vector<size_t> PerfUtils::FindMinValues(std::string perf_type, std::string key, std::string condition) {
  std::vector<size_t> min_values;
  for (auto sql_name : GetSqlNames()) {
    min_values.push_back(FindMinValue(sql_name, perf_type, key, condition));
  }
  return min_values;
}

size_t PerfUtils::GetCount(std::string name, std::string perf_type, std::string key, std::string condition) {
  std::lock_guard<std::mutex> lg(sql_map_lock_);
  if (!SqlIsExisted(name)) {
    return 0;
  }
  return sql_map_[name]->Count(perf_type, key, condition);
}

std::vector<size_t> PerfUtils::GetCountFromAllDb(std::string perf_type, std::string key, std::string condition) {
  std::vector<size_t> count_vec;
  for (auto sql_name : GetSqlNames()) {
    count_vec.push_back(GetCount(sql_name, perf_type, key, condition));
  }
  return count_vec;
}

std::vector<std::string> PerfUtils::GetTableNames(std::string name) {
  std::vector<DbItem> item;
  std::vector<std::string> table_names;
  {
    std::lock_guard<std::mutex> lg(sql_map_lock_);
    if (!SqlIsExisted(name)) {
      return table_names;
    }
    std::string select_sql = "select name from sqlite_master where type ='table'";
    item = SearchFromDatabase(sql_map_[name], select_sql);
  }
  for (auto it : item) {
    if (it.first != 1) continue;
    table_names.push_back(it.second[0]);
  }
  return table_names;
}

bool PerfUtils::SqlIsExisted(std::string name) {
  if (sql_map_.find(name) == sql_map_.end()) {
    return false;
  }
  return true;
}

std::shared_ptr<Sqlite> PerfUtils::CreateDb(std::string name) {
  if (name.empty()) {
    LOG(ERROR) << "Can not create database with empty name";
    return nullptr;
  }
  std::shared_ptr<Sqlite> sql = std::make_shared<Sqlite>(name);
  if (!sql->Connect()) {
    LOG(ERROR) << "Can not connect to database " << name;
    sql->Close();
    return nullptr;
  }
  return sql;
}

bool PerfUtils::CreateTable(std::shared_ptr<Sqlite> sql, std::string perf_type, std::string primary_key,
                            std::vector<std::string> keys) {
  if (sql == nullptr) {
    LOG(ERROR) << "Can not create table for nullptr";
    return false;
  }
  return sql->CreateTable(perf_type, primary_key, keys);
}

bool PerfUtils::Record(std::string sql_name, std::string perf_type, std::vector<std::string> keys,
                       std::vector<std::string>values) {
  if (keys.size() != values.size()) {
    LOG(ERROR) << "Record: The size of keys and values is not the same.";
    return false;
  }
  std::string key_str, value_str;
  for (uint32_t i = 0; i < keys.size(); i++) {
    key_str += keys[i] + ",";
    value_str += values[i] + ",";
  }
  key_str.pop_back();
  value_str.pop_back();
  {
    std::lock_guard<std::mutex> lg(sql_map_lock_);
    if (!SqlIsExisted(sql_name)) {
      LOG(ERROR) << "sql '" << sql_name << "' is not exist.";
      return false;
    }
    return sql_map_[sql_name]->Insert(perf_type, key_str, value_str);
  }
}

}  // namespace cnstream
