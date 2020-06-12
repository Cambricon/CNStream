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

#ifndef MODULES_CORE_INCLUDE_PERF_MANAGER_HPP_
#define MODULES_CORE_INCLUDE_PERF_MANAGER_HPP_

#include <stdlib.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <unordered_set>
#include <utility>

#include "threadsafe_queue.hpp"

namespace cnstream {

class Sqlite;

/**
 * @brief PerfManager class.
 *
 * Creates sql handler and records data to database.
 */
class PerfManager {
 public:
 /**
  * @brief Constructor of PerfManager.
  */
  PerfManager() { }
  /**
  * @brief Destructor of PerfManager.
  */
  ~PerfManager();
  /**
  * @brief Stops to record data to database.
  *
  * @return Void.
  */
  void Stop();
  /**
  * @brief Initializes PerfManager.
  *
  * Creates database and starts the thread function, which inserts data to database.
  *
  * @param db_name The name of the database.
  *
  * @return Returns true if PerfManager is initialized successfully, otherwise returns false.
  */
  bool Init(std::string db_name);

  /**
  * @brief Records data to database.
  *
  * Creates timestamp and then records the data into database.
  *
  * @param is_finished If true, indicates the frame has been processed by the module,
                       and the end time will be reocorded to database.
  * @param perf_type The perf type.
  * @param module_name The module name.
  * @param pts The pts of the frame.
  *
  * @return Returns true if the information is recorded successfully, otherwise returns false.
  */
  bool Record(bool is_finished, std::string perf_type, std::string module_name, int64_t pts);
  /**
  * @brief Records data to database.
  *
  * Creates timestamp and then records the data into database.
  *
  * @param perf_type The perf type.
  * @param primary_key The primary key.
  * @param primary_value The value of the primary key.
  * @param key The key. The value of the key is the timestamp.
  *
  * @return Returns true if the information is recorded successfully, otherwise returns false.
  */
  bool Record(std::string perf_type, std::string primary_key, std::string primary_value, std::string key);
  /**
  * @brief Records data to database.
  *
  * @param perf_type The perf type.
  * @param primary_key The primary key.
  * @param primary_value The value of the primary key.
  * @param key The key. The value of the key is the timestamp.
  * @param value The value of the key.
  *
  * @return Returns true if the information is recorded successfully, otherwise returns false.
  */
  bool Record(std::string perf_type, std::string primary_key, std::string primary_value,
              std::string key, std::string value);
  /**
  * @brief Registers performance type.
  *
  * The performance type will be registered and a table with primary key and keys will be created.
  *
  * @param perf_type The performance type.
  * @param primary_key The primary key.
  * @param keys The keys.
  *
  * @return Returns true if the performance type is registered successfully, otherwise returns false.
  */
  bool RegisterPerfType(std::string perf_type, std::string primary_key, const std::vector<std::string> &keys);

  /**
  * @brief Begins a database event.
  *
  * @return Void.
  */
  void SqlBeginTrans();
  /**
  * @brief Commits a database event.
  *
  * @return Void.
  */
  void SqlCommitTrans();

  /**
  * @brief Gets sql handler.
  *
  * @return Returns sql handler.
  */
  std::shared_ptr<Sqlite> GetSql() { return sql_; }

  /**
  * @brief Gets keys.
  *
  * @param module_names The module names.
  * @param suffix The suffixes.
  *
  * @return Returns module names with suffixes.
  */
  static std::vector<std::string> GetKeys(const std::vector<std::string>& module_names,
                                          const std::vector<std::string>& suffix);

  /**
  * @brief Gets the end time suffix.
  *
  * @return Returns the end time suffix.
  */
  static inline std::string GetEndTimeSuffix() { return "_etime"; }
  /**
  * @brief Gets the start time suffix.
  *
  * @return Returns the start time suffix.
  */
  static inline std::string GetStartTimeSuffix() { return "_stime"; }
  /**
  * @brief Gets the primary key.
  *
  * @return Returns the primary key.
  */
  static inline std::string GetPrimaryKey() { return "pts"; }
  /**
  * @brief Gets the default table name.
  *
  * @return Returns the default table name.
  */
  static inline std::string GetDefaultType() { return "PROCESS"; }


 private:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  struct PerfInfo {
    std::string perf_type;
    std::string primary_key;
    std::string primary_value;
    std::string key;
    std::string value;
  };  // struct PerfInfo

  void PopInfoFromQueue();
  void InsertInfoToDb(const PerfInfo& info);
  bool PrepareDbFileDir(std::string file_dir);
  bool CreateDir(std::string dir);

  bool is_initialized_ = false;
  std::unordered_set<std::string> perf_type_;
  std::shared_ptr<Sqlite> sql_ = nullptr;
  ThreadSafeQueue<PerfInfo> queue_;
  std::thread thread_;
  std::atomic<bool> running_{false};
};  // PerfManager

}  // namespace cnstream

#endif  // MODULES_CORE_INCLUDE_PERF_MANAGER_HPP_
