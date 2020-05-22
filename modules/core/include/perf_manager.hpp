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
 * @brief The basic data structure for performance measurements.
 *
 * Records performance information at the start-time point and end-time point.
 */
struct PerfInfo {
  std::string perf_type;          /// perf type
  std::string primary_key;        /// pts of each data frame
  std::string primary_value;      /// pts of each data frame
  std::string key;
  std::string value;
};  // struct PerfInfo

/**
 * @brief PerfManager class.
 *
 * Records PerfInfo and calculates the performance statistics of modules and pipeline .
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
  * @brief Initializes PerfManager.
  *
  * Creates database, tables, and PerfCalculator, and starts the thread function, which inserts PerfInfo to database.
  *
  * @param db_name The name of the database.
  * @param module_names All module names in the pipeline.
  * @param start_node The name of the start module in the pipeline.
  * @param end_nodes The names of all end modules in the pipeline.
  *
  * @return Returns true if PerfManager is initialized successfully, otherwise returns false.
  */
  bool Init(std::string db_name, std::vector<std::string> module_names,
            std::string start_node, std::vector<std::string> end_nodes);
  /**
  * @brief Records PerfInfo.
  *
  * Creates timestamp and sets it to PerfInfo. And then inserts the time stamp into database.
  *
  * @param info The PerfInfo.
  *
  * @return Returns true if the information is recorded successfully, otherwise returns false.
  */
  bool Record(bool is_finished, std::string type, std::string module_name, int64_t pts);
  bool Record(std::string type, std::string primary_key, std::string primary_value, std::string key);
  bool Record(std::string type, std::string primary_key, std::string primary_value,
              std::string key, std::string value);
  bool RegisterPerfType(std::string type, std::string primary_key, std::vector<std::string> keys);
  /**
  * @brief Registers performance type.
  *
  * @param type The type of performance.
  *
  * @return Returns true if the performance type is registered successfully, otherwise returns false.
  */
  bool RegisterPerfType(std::string type);

  /**
  * @brief Begins a sqlite3 event.
  *
  * @return Void.
  */
  void SqlBeginTrans();
  /**
  * @brief Commits a sqlite3 event.
  *
  * @return Void.
  */
  void SqlCommitTrans();
  /**
  * @brief Calculates the performance statistics of modules.
  *
  * @param perf_type The type of performance.
  * @param module_name The module name.
  *
  * @return Returns the performance statistics.
  */
  PerfStats CalculatePerfStats(std::string perf_type, std::string module_name);
  /**
  * @brief Calculates the performance statistics of a pipeline.
  *
  * @param perf_type The type of performance.
  *
  * @return Returns performance statistics of all end modules of a pipeline.
  */
  std::vector<std::pair<std::string, PerfStats>> CalculatePipelinePerfStats(std::string perf_type);

  bool Init(std::string db_name);
  void CreatePerfCalculator(std::string perf_type);
  void CreatePerfCalculator(std::string perf_type, std::string start_node, std::string end_node);
  std::shared_ptr<PerfCalculator> GetCalculator(std::string name);
  PerfStats CalculatePerfStats(std::string calculator_name, std::string perf_type,
                               std::string start_key, std::string end_key);
  PerfStats CalculatePerfStats(std::string perf_type, std::string start_key, std::string end_key);
  bool SetModuleNames(std::vector<std::string> module_names);
  bool SetStartNode(std::string start_node);
  bool SetEndNodes(std::vector<std::string> end_nodes);
  PerfStats CalculateThroughput(std::string calculator_name, std::string perf_type,
                                std::string start_key, std::string end_key);
  PerfStats CalculateThroughput(std::string perf_type, std::string start_key, std::string end_key);

 private:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  std::vector<std::string> GetKeys(const std::vector<std::string>& module_names);
  void PopInfoFromQueue();
  void InsertInfoToDb(const PerfInfo& info);
  void CreatePerfCalculatorForModules(std::string perf_type);
  void CreatePerfCalculatorForPipeline(std::string perf_type);
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
