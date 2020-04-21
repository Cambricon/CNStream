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

#include "perf_manager.hpp"

#include <fcntl.h>
#include <sys/stat.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "glog/logging.h"
#include "cnstream_time_utility.hpp"
#include "perf_calculator.hpp"
#include "sqlite_db.hpp"
#include "threadsafe_queue.hpp"

namespace cnstream {

const std::string kSTimeSuffix = "_stime";  // NOLINT
const std::string kETimeSuffix = "_etime";  // NOLINT
const std::string kPipelineSuffix = "_pipeline";  // NOLINT
const std::string kId = "pts";  // NOLINT

void PerfManager::Stop() {
  running_.store(false);
  if (thread_.joinable()) {
    thread_.join();
  }
}

PerfManager::~PerfManager() {
  if (running_) {
    Stop();
  }
  if (sql_) {
    sql_->Close();
    sql_ = nullptr;
    is_initialized_ = false;
  }
}

bool PerfManager::Init(std::string db_name, std::vector<std::string> module_names,
                       std::string start_node, std::vector<std::string> end_nodes) {
  if (is_initialized_) {
    LOG(ERROR) << "Should not initialize perf manager twice.";
    return false;
  }
  if (std::find(std::begin(module_names), std::end(module_names), start_node) ==
      std::end(module_names)) {
    LOG(ERROR) << "Start node name [" << start_node << "] is not found in module names.";
    return false;
  }
  for (auto it : end_nodes) {
    if (std::find(std::begin(module_names), std::end(module_names), it) ==
        std::end(module_names)) {
      LOG(ERROR) << "End node name [" << it << "] is not found in module names.";
      return false;
    }
  }
  if (!PrepareDbFileDir(db_name)) return false;

  start_node_ = start_node;
  end_nodes_ = end_nodes;
  module_names_ = module_names;

  sql_ = std::make_shared<Sqlite>(db_name);
  if (!sql_->Connect()) {
    LOG(ERROR) << "Can not connect to sqlite db.";
    return false;
  }

  // register PROCESS to perf type
  RegisterPerfType("PROCESS");

  // create tables
  std::vector<std::string> keys = GetKeys(module_names);
  for (auto it : perf_type_) {
    if (!sql_->CreateTable(it, kId, keys)) {
      LOG(ERROR) << "Can not create table to sqlite db";
      sql_->Close();
      sql_ = nullptr;
      return false;
    }
  }

  // create calculators
  for (auto type : perf_type_) {
    CreatePerfCalculator(type);
  }
  running_.store(true);
  thread_ = std::thread(&PerfManager::PopInfoFromQueue, this);
  is_initialized_ = true;
  return true;
}

bool PerfManager::RecordPerfInfo(PerfInfo info) {
  if (!running_) {
    return false;
  }
  info.timestamp = TimeStamp::Current();;
  queue_.Push(info);
  return true;
}

void PerfManager::PopInfoFromQueue() {
  PerfInfo info;
  while (running_) {
    if (queue_.WaitAndTryPop(info, std::chrono::milliseconds(100))) {
      InsertInfoToDb(info);
    }
  }

  while (queue_.TryPop(info)) {
    InsertInfoToDb(info);
  }
}

void PerfManager::InsertInfoToDb(const PerfInfo& info) {
  if (sql_ == nullptr) {
    LOG(ERROR) << "sql pointer is nullptr";
    return;
  }
  if (perf_type_.find(info.perf_type) == perf_type_.end()) {
    LOG(ERROR) << "perf type [" << info.perf_type << "] is not found. Please register first.";
    return;
  }
  std::string pts_str = std::to_string(info.pts);
  std::string timestamp_str = std::to_string(info.timestamp);
  std::string key;
  if (info.is_finished) {
    key = info.module_name + kETimeSuffix;
  } else {
    key = info.module_name + kSTimeSuffix;
  }
  if (sql_->Count(info.perf_type, kId, kId + "=" + pts_str) == 0) {
    sql_->Insert(info.perf_type, kId + "," + key, pts_str + "," + timestamp_str);
  } else {
    sql_->Update(info.perf_type, kId, pts_str, key, timestamp_str);
  }
}

bool PerfManager::RegisterPerfType(std::string type) {
  if (type.empty()) return false;
  if (perf_type_.find(type) == perf_type_.end()) {
    if (is_initialized_) {
      std::vector<std::string> keys = GetKeys(module_names_);
      if (!sql_->CreateTable(type, kId, keys)) {
        LOG(ERROR) << "Register perf type " << type << " failed";
        return false;
      }
      CreatePerfCalculator(type);
    }
    perf_type_.insert(type);
  }
  return true;
}

std::vector<std::string> PerfManager::GetKeys(const std::vector<std::string> &module_names) {
  std::vector<std::string> keys;
  if (start_node_.empty()) {
    LOG(ERROR) << "There is no start node in perf manager.";
    return keys;
  }
  keys.push_back(start_node_ + kSTimeSuffix);
  keys.push_back(start_node_ + kETimeSuffix);
  for (auto it : module_names) {
    if (it != start_node_) {
      keys.push_back(it + kSTimeSuffix);
      keys.push_back(it + kETimeSuffix);
    }
  }
  return keys;
}

void PerfManager::CreatePerfCalculator(std::string perf_type) {
  for (auto name : module_names_) {
    if (calculator_map_.find(perf_type + "_" + name) == calculator_map_.end()) {
      calculator_map_[perf_type + "_" + name] = std::make_shared<PerfCalculator>();
    }
  }

  for (auto name : end_nodes_) {
    if (calculator_map_.find(perf_type + "_" + name + kPipelineSuffix) == calculator_map_.end()) {
      calculator_map_[perf_type + "_" + name + kPipelineSuffix] = std::make_shared<PerfCalculator>();
    }
  }
}

void PerfManager::SqlBeginTrans() {
  if (sql_) sql_->Begin();
}

void PerfManager::SqlCommitTrans() {
  if (sql_) sql_->Commit();
}

bool PerfManager::PrepareDbFileDir(std::string file_path) {
  if (file_path.empty()) return false;

  int fd = open(file_path.c_str(), O_RDONLY);
  if (fd < 0) {
    LOG(INFO) << "File [" << file_path << "] not exist";
    return CreateDir(file_path);
  }
  if (fcntl(fd, F_SETLEASE, F_WRLCK) && EAGAIN == errno) {
    close(fd);
    LOG(ERROR) << "File [" << file_path << "] is opened";
    return false;
  } else {
    fcntl(fd, F_SETLEASE, F_UNLCK);
    close(fd);
    LOG(INFO) << "File [" << file_path << "] exist, but not opened. Remove file.";
    if (remove(file_path.c_str()) != 0) {
      return false;
    }
    return true;
  }
}

bool PerfManager::CreateDir(std::string dir) {
  std::string path;
  int success = 0;
  for (uint32_t i = 0; i < dir.size(); i++) {
    path.push_back(dir[i]);
    if (dir[i] == '/' && access(path.c_str(), 0) != success && mkdir(path.c_str(), 00700) != success) {
      LOG(ERROR) << "Failed at create directory";
      return false;
    }
  }
  return true;
}

std::shared_ptr<PerfCalculator> PerfManager::GetCalculator(std::string perf_type, std::string module_name) {
  auto it = calculator_map_.find(perf_type + "_" + module_name);
  if (it != calculator_map_.end()) {
    return it->second;
  }
  LOG(ERROR) << "Can not find perf calculator of PerfType ["<< perf_type << "] ModuleName [" << module_name << "]";
  return nullptr;
}

PerfStats PerfManager::CalculatePerfStats(std::string perf_type, std::string module_name) {
  PerfStats stats{0, 0, 0, 0.f};
  if (sql_ == nullptr) {
    LOG(ERROR) << "sql pointer is nullptr";
    return stats;
  }

  std::shared_ptr<PerfCalculator> calculator = GetCalculator(perf_type, module_name);
  if (calculator) {
    stats = calculator->CalcLatency(sql_, perf_type, module_name + kSTimeSuffix, module_name + kETimeSuffix);
    if (stats.latency_avg != 0) {
      stats.fps = (1e9 / stats.latency_avg) / 1000.f;
    }
  }
  return stats;
}

std::vector<std::pair<std::string, PerfStats>> PerfManager::CalculatePipelinePerfStats(std::string perf_type) {
  std::vector<std::pair<std::string, PerfStats>> pipeline_stats;
  std::shared_ptr<PerfCalculator> calc = nullptr;
  for (auto end_node : end_nodes_) {
    PerfStats stats{0, 0, 0, 0.f};
    calc = GetCalculator(perf_type, end_node + kPipelineSuffix);
    if (calc && sql_) {
      stats = calc->CalcLatency(sql_, perf_type, start_node_ + kSTimeSuffix, end_node + kETimeSuffix);
      stats.fps = calc->CalcThroughput(sql_, perf_type, start_node_ + kSTimeSuffix, end_node + kETimeSuffix).fps;
    }
    pipeline_stats.push_back(std::make_pair(end_node, stats));
  }
  return pipeline_stats;
}

}  // namespace cnstream
