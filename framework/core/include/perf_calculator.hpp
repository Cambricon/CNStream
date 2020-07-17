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

#ifndef MODULES_CORE_INCLUDE_PERF_CALCULATOR_HPP_
#define MODULES_CORE_INCLUDE_PERF_CALCULATOR_HPP_

#include <stdlib.h>
#include <algorithm>
#include <memory>
#include <mutex>
#include <numeric>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <iostream>

namespace cnstream {

/**
 * @brief The data type of database item
 */
using DbItem = std::pair<int, std::vector<std::string>>;
/**
 * @brief The data type of integer item
 */
using DbIntegerItem = std::vector<size_t>;

class Sqlite;

/**
 * @brief The basic data structure of performance statistics, including latency, frame count, and throughout.
 */
struct PerfStats {
  size_t latency_avg = 0;  /// Average latency
  size_t latency_min = 0;  /// Min latency
  size_t latency_max = 0;  /// Max latency
  size_t frame_cnt = 0;    /// Frame count
  double fps = 0.f;        /// Throughput
};                         // struct PerfStats

/**
 * @brief Prints latency.
 * @param stats The performance statstics.
 * @param width The width of console output "frame count". Default value is 0.
 *
 * @return Void.
 */
void PrintLatency(const PerfStats &stats, uint32_t width = 0);
/**
 * @brief Prints throughput.
 * @param stats The performance statstics.
 * @param width The width of console output "frame count". Default value is 0.
 *
 * @return Void.
 */
void PrintThroughput(const PerfStats &stats, uint32_t width = 0);
/**
 * @brief Prints stream id.
 * @param stream_id The stream id.
 *
 * @return Void.
 */
void PrintStreamId(std::string stream_id);
/**
 * @brief Prints string.
 * @param str The string.
 * @param width The width of the printed string. The default value is 15.
 * @param fill_charater Fills output with fill_charater. The default value is empty char
 *
 * @return Void.
 */
void PrintStr(const std::string str, uint32_t width = 15, const char fill_charater = ' ');
/**
 * @brief Prints title.
 * @param title The title.
 *
 * @return Void.
 */
void PrintTitle(std::string title);
/**
 * @brief Prints title for latest throughput.
 * @param timeframe The latest throughput is over timeframe. The default value is 2s.
 *
 * @return Void.
 */
void PrintTitleForLatestThroughput(const std::string timeframe = "2s");
/**
 * @brief Prints title for average throughput.
 *
 * @return Void.
 */
void PrintTitleForAverageThroughput();
/**
 * @brief Prints 'total'.
 *
 * @return Void.
 */
void PrintTitleForTotal();

/**
 * @brief Perf Utilities is for getting data from databse.
 */
class PerfUtils {
 public:
  /**
   * @brief Adds sql handler.
   *
   * @param name sql The handler name.
   * @param sql The sql handler.
   *
   * @return Returns true if add sql sucessfully, otherwise false.
   */
  bool AddSql(const std::string &name, std::shared_ptr<Sqlite> sql);
  /**
   * @brief Removes sql handler.
   *
   * @param name sql The handler name.
   *
   * @return Returns true if remove sql sucessfully, otherwise false.
   */
  bool RemoveSql(const std::string &name);

  /**
   * @brief Searchs data from database.
   *
   * @param sql The sql handler.
   * @param sql_statement The sql statement.
   *
   * @return Returns database items.
   */
  std::vector<DbItem> SearchFromDatabase(std::shared_ptr<Sqlite> sql, std::string sql_statement);
  /**
   * @brief Gets thread id.
   *
   * @param name The sql handler name.
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param th_key The thread key.
   *
   * @return Returns thread ids.
   */
  std::set<std::string> GetThreadId(std::string name, std::string perf_type, std::string th_key);
  /**
   * @brief Gets thread id from all database.
   *
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param th_key The thread key.
   *
   * @return Returns thread ids.
   */
  std::set<std::string> GetThreadIdFromAllDb(std::string perf_type, std::string thread_key);

  /**
   * @brief Gets items.
   *
   * @param name The sql handler name.
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param keys The item keys.
   * @param condition The condition statement.
   *
   * @return Returns items.
   */
  std::vector<DbItem> GetItems(std::string name, std::string perf_type, std::vector<std::string> keys,
                               std::string condition = "");
  /**
   * @brief Gets items from all database.
   *
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param keys The item keys.
   * @param condition The condition statement.
   *
   * @return Returns items.
   */
  std::vector<DbItem> GetItemsFromAllDb(std::string perf_type, std::vector<std::string> keys,
                                        std::string condition = "");

  /**
   * @brief Transfers database items to ineger.
   *
   * @param data The database items.
   *
   * @return Returns integer items.
   */
  static std::vector<DbIntegerItem> ToInteger(const std::vector<DbItem> &data);

  /**
   * @brief Finds max value.
   *
   * @param name The sql handler name.
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param keys The key.
   * @param condition The condition statement.
   *
   * @return Returns max value.
   */
  size_t FindMaxValue(std::string name, std::string perf_type, std::string key, std::string condition = "");
  /**
   * @brief Finds max value from each database.
   *
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param keys The key.
   * @param condition The condition statement.
   *
   * @return Returns max values.
   */
  std::vector<size_t> FindMaxValues(std::string perf_type, std::string key, std::string condition = "");

  /**
   * @brief Finds min value.
   *
   * @param name The sql handler name.
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param keys The key.
   * @param condition The condition statement.
   *
   * @return Returns min value.
   */
  size_t FindMinValue(std::string name, std::string perf_type, std::string key, std::string condition = "");
  /**
   * @brief Finds min value from each database.
   *
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param keys The key.
   * @param condition The condition statement.
   *
   * @return Returns min values.
   */
  std::vector<size_t> FindMinValues(std::string perf_type, std::string key, std::string condition = "");

  /**
   * @brief Gets count
   *
   * @param name The sql handler name.
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param keys The key.
   * @param condition The condition statement.
   *
   * @return Returns count.
   */
  size_t GetCount(std::string name, std::string perf_type, std::string key, std::string condition = "");
  /**
   * @brief Finds count from each database.
   *
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param keys The key.
   * @param condition The condition statement.
   *
   * @return Returns counts.
   */
  std::vector<size_t> GetCountFromAllDb(std::string perf_type, std::string key, std::string condition = "");

  /**
   * @brief Gets table names.
   *
   * @param name The sql handler name.
   *
   * @return Returns table names.
   */
  std::vector<std::string> GetTableNames(std::string name);

  /**
   * @brief Checks if the sql handler exists.
   *
   * @param name The sql handler name.
   *
   * @return Returns true if the sql handler exists, otherwise false.
   */
  bool SqlIsExisted(std::string name);
  /**
   * @brief Records data to database.
   *
   * @param sql_name The sql handler name.
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param keys The keys.
   * @param values The values.
   *
   * @return Returns true if the data has been recorded successfully, otherwise false.
   */
  bool Record(std::string sql_name, std::string perf_type, std::vector<std::string> keys,
              std::vector<std::string>values);
  /**
   * @brief Creates database.
   *
   * @param name The database name.
   *
   * @return Returns Sqlite pointer if the database has been created successfully, otherwise false.
   */
  static std::shared_ptr<Sqlite> CreateDb(std::string name);
  /**
   * @brief Creates table to database.
   *
   * @param sql The sql handler.
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param primary_key The primary key.
   * @param keys The keys.
   *
   * @return Returns true if the table has been created successfully, otherwise false.
   */
  static bool CreateTable(std::shared_ptr<Sqlite> sql, std::string perf_type,
                          std::string primary_key, std::vector<std::string> keys);
  /**
   * @brief Gets max value of a vector.
   *
   * @param values The vector contains values.
   *
   * @return Returns max value.
   */
  template <class T>
  static T Max(std::vector<T> values) {
    if (values.size()) {
      return *(std::max_element(values.begin(), values.end()));
    }
    return 0;
  }
  /**
   * @brief Gets max value of a vector.
   *
   * @param values The vector contains values.
   * @param p It indicates how to find the max value of the vector.
   *
   * @return Returns max value.
   */
  template <class T, typename Proc>
  static T Max(std::vector<T> values, Proc p) {
    if (values.size()) {
      return *(std::max_element(values.begin(), values.end(), p));
    }
    return T();
  }
  /**
   * @brief Gets min value of a vector.
   *
   * @param values The vector contains values.
   *
   * @return Returns min value.
   */
  template <class T>
  static T Min(std::vector<T> values) {
    if (values.size()) {
      return *(std::min_element(values.begin(), values.end()));
    }
    return ~0;
  }

  /**
   * @brief Sums a vector.
   *
   * @param values The vector contains values.
   *
   * @return Returns sum.
   */
  template <class T>
  static T Sum(std::vector<T> values) {
    if (values.size()) {
      return std::accumulate(values.begin(), values.end(), (T)0);
    }
    return 0;
  }

  /**
   * @brief Sorts a vector.
   *
   * @param vec The vector contains values.
   * @param p It indicates how to sort the vector.
   *
   * @return Void.
   */
  template <typename T, typename Proc>
  static void Sort(std::vector<T> *vec, Proc p) {
    if (vec->size()) {
      std::sort(vec->begin(), vec->end(), p);
    }
  }

 private:
  std::vector<std::string> GetSqlNames();
  std::unordered_map<std::string, std::shared_ptr<Sqlite>> sql_map_;
  std::mutex sql_map_lock_;
};  // PerfUtils

class PerfCalculationMethod;

/**
 * @brief Calculates performance statistics.
 *
 * Gets data from database and then calculates performance statistics including average, max and min latency,
 * throughput and frame count.
 *
 * It has four child classes for extending the calculation. If a new calculator is needed, inherit from this class
 * and override functions.
 */
class PerfCalculator {
 public:
  /**
   * @brief Constructor of PerfCalculator.
   */
  PerfCalculator() {
    perf_utils_ = std::make_shared<PerfUtils>();
    method_ = std::make_shared<PerfCalculationMethod>();
    print_throughput_ = true;
  }

  virtual ~PerfCalculator() {}

  /**
   * @brief Sets PerfUtils.
   *
   * @param perf_utils The perf utils for getting data from database.
   *
   * @return Returns true if perf_utils is not nullptr, otherwise returns false.
   */
  bool SetPerfUtils(std::shared_ptr<PerfUtils> perf_utils) {
    if (perf_utils == nullptr) return false;
    perf_utils_ = perf_utils;
    return true;
  }
  /**
   * @brief Gets PerfUtils.
   *
   * @return Returns perf utils.
   */
  std::shared_ptr<PerfUtils> GetPerfUtils() { return perf_utils_; }
  /**
   * @brief Adds database handler to calculator.
   *
   * @param name The name of the database handler.
   * @param handler The database handler.
   *
   * @return Returns true if database handler has been added successfully, otherwise returns false.
   */
  bool AddDataBaseHandler(const std::string &name, std::shared_ptr<Sqlite> handler) {
    return perf_utils_->AddSql(name, handler);
  }
  /**
   * @brief Creates database for storing unprocessed data.
   *
   * @param db_name The name of the database file.
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param module_name The module name.
   * @param suffixes The suffixes will be appended to module name.
   *
   * @return Returns true if database has been created successfully, otherwise returns false.
   */
  bool CreateDbForStoreUnprocessedData(const std::string &db_name, const std::string &perf_type,
                                       const std::string &module_name, const std::vector<std::string> &suffixes);
  /**
   * @brief Removes data related to performance calculation for one module of one stream.
   *
   * This function could be overridden by child class.
   *
   * @param sql_name The name of the database handler.
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param module_name The module name.
   *
   * @return void
   */
  virtual void RemovePerfStats(const std::string &sql_name, const std::string &perf_type,
                               const std::string &module_name);
  /**
   * @brief Gets latency.
   *
   * @param sql_name The sql name.
   * @param perf_type The perf type.
   *
   * @return Returns performance statistics.
   */
  PerfStats GetLatency(const std::string &sql_name, const std::string &perf_type);
  /**
   * @brief Gets throughput.
   *
   * @param sql_name The sql name.
   * @param perf_type The perf type.
   *
   * @return Returns performance statistics vector.
   */
  std::vector<PerfStats> GetThroughput(const std::string &sql_name, const std::string &perf_type);
  /**
   * @brief Calculates average throughput.
   *
   * @param stats_vec performance statistics vector.
   *
   * @return Returns performance statistics.
   */
  PerfStats CalcAvgThroughput(const std::vector<PerfStats> &stats_vec);
  /**
   * @brief Gets average throughput.
   *
   * @param sql_name The sql name.
   * @param perf_type The perf type.
   *
   * @return Returns performance statistics.
   */
  PerfStats GetAvgThroughput(const std::string &sql_name, const std::string &perf_type);
  /**
   * @brief Calculates final throughput.
   *
   * @param sql_name The sql name.
   * @param perf_type The perf type.
   * @param keys Keys needed by calculating throughput.
   *
   * @return Returns performance statistics.
   */
  PerfStats CalculateFinalThroughput(const std::string &sql_name, const std::string &perf_type,
                                     const std::vector<std::string> &keys);
  /**
   * @brief Calculates latency.
   *
   * This function could be overridden by child class.
   *
   * @param sql_name The sql name.
   * @param perf_type The perf type.
   * @param keys Keys needed by calculating latency.
   *
   * @return Returns performance statistics.
   */
  virtual PerfStats CalcLatency(const std::string &sql_name, const std::string &perf_type,
                                const std::vector<std::string> &keys);
  /**
   * @brief Calculates throughput.
   *
   * This function could be overridden by child class.
   *
   * @param sql_name The sql name.
   * @param perf_type The perf type.
   * @param keys Keys needed by calculating throughput.
   *
   * @return Returns performance statistics.
   */
  virtual PerfStats CalcThroughput(const std::string &sql_name, const std::string &perf_type,
                                   const std::vector<std::string> &keys) {
    return PerfStats();
  }

  /**
   * @brief Sets whether print throughput.
   *
   * @param enable Whether enable to print throughput.
   *
   * @return Void.
   */
  void SetPrintThroughput(bool enable) { print_throughput_ = enable; }

 protected:
  void RemoveLatency(const std::string &sql_name, const std::string &perf_type);
  std::shared_ptr<PerfUtils> perf_utils_;
  std::shared_ptr<PerfCalculationMethod> method_;

  std::unordered_map<std::string, size_t> pre_time_map_;
  std::unordered_map<std::string, PerfStats> stats_latency_map_;
  std::unordered_map<std::string, std::vector<PerfStats>> throughput_;

  bool print_throughput_ = true;

  std::mutex latency_mutex_;
  std::mutex fps_mutex_;
};  // PerfCalculator

/**
 * @brief Calculates performance statistics for module.
 *
 * Gets data from database and then calculates performance statistics including average, max and min latency,
 * throughput and frame count.
 */
class PerfCalculatorForModule : public PerfCalculator {
 public:
   /**
   * @brief Removes data related to performance calculation for one module of one stream.
   *
   * @param sql_name The name of the database handler.
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param module_name The module name.
   *
   * @return void
   */
  void RemovePerfStats(const std::string &sql_name, const std::string &perf_type,
                       const std::string &module_name) override;
  /**
   * @brief Calculates throughput.
   *
   * @param sql_name The sql name.
   * @param perf_type The perf type.
   * @param keys Keys needed by calculating throughput.
   *
   * @return Returns performance statistics.
   */
  PerfStats CalcThroughput(const std::string &sql_name, const std::string &perf_type,
                           const std::vector<std::string> &keys) override;

 private:
  void StoreUnprocessedData(const std::string &sql_name, const std::string &perf_type,
                            const std::string &module_name);
};  // PerfCalculatorForModule

/**
 * @brief Calculates performance statistics for pipeline.
 *
 * Gets data from database and then calculates performance statistics including average, max and min latency,
 * throughput and frame count.
 */
class PerfCalculatorForPipeline : public PerfCalculator {
 public:
  /**
   * @brief Removes data related to performance calculation for one end node of the pipeline of one stream.
   *
   * @param sql_name The name of the database handler.
   * @param perf_type The perf type, i.e., the table name of the database.
   * @param module_name The end node name of the pipeline.
   *
   * @return void
   */
  void RemovePerfStats(const std::string &sql_name, const std::string &perf_type,
                       const std::string &key) override;
  /**
   * @brief Calculates throughput.
   *
   * @param sql_name The sql name.
   * @param perf_type The perf type.
   * @param keys Keys needed by calculating throughput.
   *
   * @return Returns performance statistics.
   */
  PerfStats CalcThroughput(const std::string &sql_name, const std::string &perf_type,
                           const std::vector<std::string> &keys) override;

 private:
  void RemoveThroughput(const std::string &sql_name, const std::string &perf_type);
  void StoreUnprocessedData(const std::string &sql_name, const std::string &perf_type,
                            const std::string &key);
};  // PerfCalculatorForPipeline

/**
 * @brief Calculates performance statistics for inferencer module.
 *
 * Gets data from database and then calculates performance statistics including average, max and min latency,
 * throughput and frame count.
 */
class PerfCalculatorForInfer : public PerfCalculator {
 public:
  /**
   * @brief Calculates throughput.
   *
   * @param sql_name The sql name.
   * @param perf_type The perf type.
   * @param keys Keys needed by calculating throughput.
   *
   * @return Returns performance statistics.
   */
  PerfStats CalcThroughput(const std::string &sql_name, const std::string &perf_type,
                           const std::vector<std::string> &keys) override;
};  // PerfCalculatorForInfer

/**
 * @brief Calculates performance statistics.
 *
 * Calculates performance statistics including average, max and min latency, throughput and frame count.
 */
class PerfCalculationMethod {
 public:
  /**
   * @brief Calculates throughput.
   *
   * @param start_time The start time.
   * @param item_vec The items including data needed by calculation.
   *
   * @return Returns performance statistics.
   */
  PerfStats CalcThroughput(size_t start_time, const std::vector<DbIntegerItem> &item_vec);
  /**
   * @brief Calculates throughput.
   *
   * @param start_time The start time.
   * @param end_time The end time.
   * @param frame_cnt The frame count.
   *
   * @return Returns performance statistics.
   */
  PerfStats CalcThroughput(size_t start_time, size_t end_time, size_t frame_cnt);
  /**
   * @brief Calculates latency.
   *
   * @param item_vec The items including data needed by calculation.
   *
   * @return Returns performance statistics.
   */
  PerfStats CalcLatency(const std::vector<DbIntegerItem> &item_vec);
};  // PerfCalculationMethod

}  // namespace cnstream

#endif  // MODULES_CORE_INCLUDE_PERF_CALCULATOR_HPP_
