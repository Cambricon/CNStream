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
#include <mutex>
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

std::mutex calculator_map_mutex;
std::mutex perf_type_set_mutex;

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

bool PerfManager::SetModuleNames(std::vector<std::string> module_names) {
  if (module_names_.size() != 0) {
    LOG(ERROR) << "module names has been set.";
    return false;
  }

  module_names_ = module_names;
  return true;
}

bool PerfManager::SetStartNode(std::string start_node) {
  if (!start_node_.empty() || start_node.empty()) {
    LOG(ERROR) << "start node name is empty or it has been set.";
    return false;
  }
  if (std::find(std::begin(module_names_), std::end(module_names_), start_node) ==
      std::end(module_names_)) {
    LOG(ERROR) << "Start node name [" << start_node << "] is not found in module names.";
    return false;
  }

  start_node_ = start_node;
  return true;
}

bool PerfManager::SetEndNodes(std::vector<std::string> end_nodes) {
  if (end_nodes_.size() != 0) {
    LOG(ERROR) << "end nodes has been set.";
    return false;
  }
  for (auto it : end_nodes) {
    if (it.empty() || std::find(std::begin(module_names_), std::end(module_names_), it) ==
        std::end(module_names_)) {
      LOG(ERROR) << "End node name [" << it << "] is not found in module names.";
      return false;
    }
  }

  end_nodes_ = end_nodes;
  return true;
}

bool PerfManager::Init(std::string db_name) {
  if (is_initialized_) {
    LOG(ERROR) << "Should not initialize perf manager twice.";
    return false;
  }
  if (!PrepareDbFileDir(db_name)) {
    LOG(ERROR) << "Prepare database file failed.";
    return false;
  }

  sql_ = std::make_shared<Sqlite>(db_name);
  if (!sql_->Connect()) {
    LOG(ERROR) << "Can not connect to sqlite db.";
    return false;
  }

  running_.store(true);
  thread_ = std::thread(&PerfManager::PopInfoFromQueue, this);
  is_initialized_ = true;
  return true;
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
  std::lock_guard<std::mutex> lg(perf_type_set_mutex);
  {
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
      CreatePerfCalculatorForModules(type);
      CreatePerfCalculatorForPipeline(type);
    }
  }
  running_.store(true);
  thread_ = std::thread(&PerfManager::PopInfoFromQueue, this);
  is_initialized_ = true;
  return true;
}

bool PerfManager::Record(bool is_finished, std::string type, std::string module_name, int64_t pts) {
  std::string timestamp_str = TimeStamp::CurrentToString();
  std::string pts_str = std::to_string(pts);
  std::string key;
  if (is_finished) {
    key = module_name + kETimeSuffix;
  } else {
    key = module_name + kSTimeSuffix;
  }
  return Record(type, kId, pts_str, key, timestamp_str);
}

bool PerfManager::Record(std::string type, std::string primary_key, std::string primary_value, std::string key) {
  return Record(type, primary_key, primary_value, key, TimeStamp::CurrentToString());
}

bool PerfManager::Record(std::string type, std::string primary_key, std::string primary_value,
                         std::string key, std::string value) {
  if (!running_) {
    return false;
  }

  PerfInfo info = {type, primary_key, primary_value, key, value};
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
  std::lock_guard<std::mutex> lg(perf_type_set_mutex);
  {
    if (perf_type_.find(info.perf_type) == perf_type_.end()) {
      LOG(ERROR) << "perf type [" << info.perf_type << "] is not found. Please register first.";
      return;
    }
  }

  if (sql_->Count(info.perf_type, info.primary_key, info.primary_key + "=" + info.primary_value) == 0) {
    sql_->Insert(info.perf_type, info.primary_key + "," + info.key, info.primary_value + "," + info.value);
  } else {
    sql_->Update(info.perf_type, info.primary_key, info.primary_value, info.key, info.value);
  }
}

bool PerfManager::RegisterPerfType(std::string type, std::string primary_key, std::vector<std::string> keys) {
  std::lock_guard<std::mutex> lg(perf_type_set_mutex);
  if (type.empty() || perf_type_.find(type) != perf_type_.end() || !is_initialized_) {
    return false;
  }
  if (!sql_->CreateTable(type, primary_key, keys)) {
    LOG(ERROR) << "Register perf type " << type << " failed";
    return false;
  }
  perf_type_.insert(type);
  return true;
}

bool PerfManager::RegisterPerfType(std::string type) {
  if (type.empty()) return false;
  std::lock_guard<std::mutex> lg(perf_type_set_mutex);
  if (perf_type_.find(type) == perf_type_.end()) {
    if (is_initialized_) {
      std::vector<std::string> keys = GetKeys(module_names_);
      if (!sql_->CreateTable(type, kId, keys)) {
        LOG(ERROR) << "Register perf type " << type << " failed";
        return false;
      }
      CreatePerfCalculatorForModules(type);
      CreatePerfCalculatorForPipeline(type);
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

void PerfManager::CreatePerfCalculatorForModules(std::string perf_type) {
  for (auto name : module_names_) {
    CreatePerfCalculator(perf_type + "_" + name);
  }
}
void PerfManager::CreatePerfCalculatorForPipeline(std::string perf_type) {
  for (auto name : end_nodes_) {
    CreatePerfCalculator(perf_type + "_" + name + kPipelineSuffix);
  }
}

void PerfManager::CreatePerfCalculator(std::string perf_type, std::string start_node, std::string end_node) {
  CreatePerfCalculator(perf_type + "_" + start_node + "_" + end_node);
}

void PerfManager::CreatePerfCalculator(std::string name) {
  std::lock_guard<std::mutex> lg(calculator_map_mutex);
  if (calculator_map_.find(name) == calculator_map_.end()) {
      calculator_map_[name] = std::make_shared<PerfCalculator>();
  }
}

void PerfManager::SqlBeginTrans() {
  if (sql_) sql_->Begin();
}

void PerfManager::SqlCommitTrans() {
  if (sql_) sql_->Commit();
}

bool PerfManager::PrepareDbFileDir(std::string file_path) {
  if (file_path.empty()) {
    LOG(ERROR) << "file path is empty.";
    return false;
  }

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

std::shared_ptr<PerfCalculator> PerfManager::GetCalculator(std::string name) {
  std::lock_guard<std::mutex> lg(calculator_map_mutex);
  auto it = calculator_map_.find(name);
  if (it != calculator_map_.end()) {
    return it->second;
  }
  LOG(ERROR) << "Can not find perf calculator [" << name << "]";
  return nullptr;
}

std::shared_ptr<PerfCalculator> PerfManager::GetCalculator(std::string perf_type, std::string module_name) {
  return GetCalculator(perf_type + "_" + module_name);
}

PerfStats PerfManager::CalculatePerfStats(std::string calculator_name, std::string perf_type,
                                          std::string start_key, std::string end_key) {
  PerfStats stats{0, 0, 0, 0.f};
  if (sql_ == nullptr) {
    LOG(ERROR) << "sql pointer is nullptr";
    return stats;
  }

  std::shared_ptr<PerfCalculator> calculator = GetCalculator(calculator_name);
  if (calculator) {
    stats = calculator->CalcLatency(sql_, perf_type, start_key, end_key);
    if (stats.latency_avg != 0) {
      stats.fps = (1e9 / stats.latency_avg) / 1000.f;
    }
  }
  return stats;
}

PerfStats PerfManager::CalculateThroughput(std::string calculator_name, std::string perf_type,
                                           std::string start_key, std::string end_key) {
  PerfStats stats{0, 0, 0, 0.f};
  if (sql_ == nullptr) {
    LOG(ERROR) << "sql pointer is nullptr";
    return stats;
  }

  std::shared_ptr<PerfCalculator> calculator = GetCalculator(calculator_name);
  if (calculator) {
    stats = calculator->CalcThroughputByEachFrameTime(sql_, perf_type, start_key, end_key);
  }
  return stats;
}

PerfStats PerfManager::CalculatePerfStats(std::string perf_type, std::string start_key, std::string end_key) {
  return CalculatePerfStats(perf_type + "_" + start_key + "_" + end_key, perf_type, start_key, end_key);
}

PerfStats PerfManager::CalculatePerfStats(std::string perf_type, std::string module_name) {
  return CalculatePerfStats(perf_type + "_" + module_name, perf_type,
                            module_name + kSTimeSuffix, module_name + kETimeSuffix);
}

PerfStats PerfManager::CalculateThroughput(std::string perf_type, std::string start_key, std::string end_key) {
  return CalculateThroughput(perf_type + "_" + start_key + "_" + end_key, perf_type, start_key, end_key);
}

std::vector<std::pair<std::string, PerfStats>> PerfManager::CalculatePipelinePerfStats(std::string perf_type) {
  std::vector<std::pair<std::string, PerfStats>> pipeline_stats;
  std::shared_ptr<PerfCalculator> calc = nullptr;
  for (auto end_node : end_nodes_) {
    PerfStats stats{0, 0, 0, 0.f};
    calc = GetCalculator(perf_type, end_node + kPipelineSuffix);
    if (calc && sql_) {
      stats = calc->CalcLatency(sql_, perf_type, start_node_ + kSTimeSuffix, end_node + kETimeSuffix);
      stats.fps =
          calc->CalcThroughputByTotalTime(sql_, perf_type, start_node_ + kSTimeSuffix, end_node + kETimeSuffix).fps;
    }
    pipeline_stats.push_back(std::make_pair(end_node, stats));
  }
  return pipeline_stats;
}

}  // namespace cnstream
