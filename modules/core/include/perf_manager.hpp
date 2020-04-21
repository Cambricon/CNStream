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
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "threadsafe_queue.hpp"

namespace cnstream {

class Sqlite;
class PerfCalculator;
class PerfStats;

/**
 * @brief The basic data structure of measuring performance.
 *
 * Record PerfInfo at start time point and end time point.
 */
struct PerfInfo {
  bool is_finished;         /// If it is true means start time, otherwise end time.
  std::string perf_type;    /// perf type
  std::string module_name;  /// module name
  int64_t pts;              /// pts of each data frame
  size_t timestamp;         /// timestamp
};  // struct PerfInfo

/**
 * @brief PerfManager class
 *
 * It could record PerfInfo and calculate modules and pipeline performance statistics.
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
  * @brief Stops to record PerfInfo.
  *
  * @return Void.
  */
  void Stop();
  /**
  * @brief Inits PerfManager.
  *
  * Create database and tables, create PerfCalculator, and start thread function, which is used to insert PerfInfo to database.
  *
  * @param db_name The name of the database.
  * @param module_names All module names of the pipeline.
  * @param start_node Start node module name of the pipeline.
  * @param end_nodes All end node module names of the pipeline.
  *
  * @return Returns true if PerfManager inits successfully, otherwise returns false.
  */
  bool Init(std::string db_name, std::vector<std::string> module_names,
            std::string start_node, std::vector<std::string> end_nodes);
  /**
  * @brief Records PerfInfo.
  *
  * Create timestamp and set it to PerfInfo. And then insert it to database.
  *
  * @param info PerfInfo.
  *
  * @return Returns true if the info is recorded successfully, otherwise returns false.
  */
  bool RecordPerfInfo(PerfInfo info);
  /**
  * @brief Registers perf type.
  *
  * @param type perf type.
  *
  * @return Returns true if type is registered successfully, otherwise returns false.
  */
  bool RegisterPerfType(std::string type);

  /**
  * @brief Begins sqlite3 event.
  *
  * @return Void.
  */
  void SqlBeginTrans();
  /**
  * @brief Commits sqlite3 event.
  *
  * @return Void.
  */
  void SqlCommitTrans();
  /**
  * @brief Calculates Performance statistics of modules.
  *
  * @param perf_type perf type.
  * @param module_name module name.
  *
  * @return Returns performance statistics.
  */
  PerfStats CalculatePerfStats(std::string perf_type, std::string module_name);
  /**
  * @brief Calculates Performance statistics of pipeline.
  *
  * @param perf_type perf type.
  *
  * @return Returns performance statistics of all end nodes of pipeline.
  */
  std::vector<std::pair<std::string, PerfStats>> CalculatePipelinePerfStats(std::string perf_type);

 private:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  std::vector<std::string> GetKeys(const std::vector<std::string>& module_names);
  void PopInfoFromQueue();
  void InsertInfoToDb(const PerfInfo& info);
  void CreatePerfCalculator(std::string perf_type);
  bool PrepareDbFileDir(std::string file_dir);
  bool CreateDir(std::string dir);
  std::shared_ptr<PerfCalculator> GetCalculator(std::string perf_type, std::string module_name);

  bool is_initialized_ = false;
  std::string start_node_;
  std::vector<std::string> end_nodes_;
  std::vector<std::string> module_names_;
  std::unordered_set<std::string> perf_type_;
  std::shared_ptr<Sqlite> sql_ = nullptr;
  std::unordered_map<std::string, std::shared_ptr<PerfCalculator>> calculator_map_;
  ThreadSafeQueue<PerfInfo> queue_;
  std::thread thread_;
  std::atomic<bool> running_{false};
};  // PerfManager

}  // namespace cnstream

#endif  // MODULES_CORE_INCLUDE_PERF_MANAGER_HPP_
