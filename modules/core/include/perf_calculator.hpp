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
#include <memory>
#include <string>
#include <vector>

namespace cnstream {

class Sqlite;

/**
 * @brief The basic data structure of performance statistics, including latency, frame count and throughout.
 */
struct PerfStats {
  size_t latency_avg;
  size_t latency_max;
  size_t frame_cnt;
  double fps;
};  // struct PerfStats

/**
 * @brief Prints latency.
 * @param stats Performance statstics.
 * @return Void.
 */
void PrintLatency(const PerfStats& stats);
/**
 * @brief Prints throughput.
 * @param stats Performance statstics.
 * @return Void.
 */
void PrintThroughput(const PerfStats& stats);
/**
 * @brief Prints performance statistics.
 * @param stats Performance statstics.
 * @return Void.
 */
void PrintPerfStats(const PerfStats& stats);

/**
 * @brief PerfCalculator class is for calculating performance statistics.
 *
 * Read useful data from database and then it could calculate performance statistics like latency and throughput.
 */
class PerfCalculator {
 public:
 /**
  * @brief Constructor of PerfCalculator.
  */
  PerfCalculator();
 /**
  * @brief Destructor of PerfCalculator.
  */
  ~PerfCalculator();
  /**
   * @brief Calculates latency
   *
   * Each time we call this function, it will calculate latency from the previous time to now.
   * After that, it will update the performance statistics, store it and replace the previous time with now.
   * Finally, return the performance statistics.
   *
   * @param sql A shared pointer points to Sqlite which is a class for operating the database.
   * @param type perf type.
   * @param start_key The index name of start time points in database.
   * @param end_key The index name of end time points in database.
   *
   * @return PerfStats, i.e., performance statistics.
   */
  PerfStats CalcLatency(std::shared_ptr<Sqlite> sql, std::string type, std::string start_key, std::string end_key);
  /**
   * @brief Calculates throughput
   *
   * throughput = frame count / (the maximum of the end time points - the minimum of the start time points)
   *
   * @param sql A shared pointer points to Sqlite which is a class for operating the database.
   * @param type perf type.
   * @param start_key The index name of the start time points in database.
   * @param end_key The index name of the end time points in database.
   *
   * @return PerfStats, i.e., performance statistics.
   */
  PerfStats CalcThroughput(std::shared_ptr<Sqlite> sql, std::string type, std::string start_node, std::string end_node);

 private:
#ifdef UNIT_TEST
 public:  //NOLINT
#endif
  size_t pre_time_ = 0;
  PerfStats stats_ = {0, 0, 0, 0.f};
};  // PerfCalculator

}  // namespace cnstream

#endif  // MODULES_CORE_INCLUDE_PERF_CALCULATOR_HPP_
