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

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "glog/logging.h"
#include "sqlite_db.hpp"
#include "util/cnstream_queue.hpp"
#include "util/cnstream_time_utility.hpp"

namespace cnstream {

std::mutex perf_type_set_mutex;

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

std::shared_ptr<PerfManager> PerfManager::CreateDefaultManager(const std::string db_name,
                                                               const std::vector<std::string> &module_names) {
  std::shared_ptr<PerfManager> manager = std::make_shared<PerfManager>();
  if (!manager) {
    LOG(ERROR) << "PerfManager::CreateDefaultManager() new PerfManager failed.";
    return nullptr;
  }
  if (!manager->Init(db_name)) {
    LOG(ERROR) << "Init PerfManager " << db_name << " failed.";
    return nullptr;
  }
  std::vector<std::string> keys =
      PerfManager::GetKeys(module_names, {GetStartTimeSuffix(), GetEndTimeSuffix(), GetThreadSuffix()});
  if (!manager->RegisterPerfType(GetDefaultType(), GetPrimaryKey(), keys)) {
    LOG(ERROR) << "PerfManager " << db_name << " register perf type " << GetDefaultType() << "failed.";
    return nullptr;
  }
  return manager;
}

bool PerfManager::Init(std::string db_name) {
  if (db_name.empty()) {
    LOG(ERROR) << "Please init with database file name.";
    return false;
  }
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

bool PerfManager::RegisterPerfType(std::string type, std::string primary_key, const std::vector<std::string>& keys) {
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

bool PerfManager::Record(bool is_finished, std::string type, std::string module_name, int64_t pts) {
  std::string timestamp_str = TimeStamp::CurrentToString();
  std::string pts_str = std::to_string(pts);
  std::string key;
  if (is_finished) {
    key = module_name + GetEndTimeSuffix();
  } else {
    key = module_name + GetStartTimeSuffix();
  }
  return Record(type, GetPrimaryKey(), pts_str, key, timestamp_str);
}

bool PerfManager::Record(std::string type, std::string primary_key, std::string primary_value, std::string key) {
  return Record(type, primary_key, primary_value, key, TimeStamp::CurrentToString());
}

bool PerfManager::Record(std::string type, std::string primary_key, std::string primary_value, std::string key,
                         std::string value) {
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

std::vector<std::string> PerfManager::GetKeys(const std::vector<std::string>& module_names,
                                              const std::vector<std::string>& suffix) {
  std::vector<std::string> keys;

  for (auto module_name : module_names) {
    for (auto it : suffix) {
      keys.push_back(module_name + it);
    }
  }
  return keys;
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
    // LOG(INFO) << "File [" << file_path << "] not exist";
    return CreateDir(file_path);
  }
  if (fcntl(fd, F_SETLEASE, F_WRLCK) && EAGAIN == errno) {
    close(fd);
    LOG(ERROR) << "File [" << file_path << "] is opened";
    return false;
  } else {
    fcntl(fd, F_SETLEASE, F_UNLCK);
    close(fd);
    // LOG(INFO) << "File [" << file_path << "] exist, but not opened. Remove file.";
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

}  // namespace cnstream
