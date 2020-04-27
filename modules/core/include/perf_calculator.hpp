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
 * @brief The basic data structure of performance statistics, including latency, frame count, and throughout.
 */
struct PerfStats {
  size_t latency_avg;
  size_t latency_max;
  size_t frame_cnt;
  double fps;
};  // struct PerfStats

/**
 * @brief Prints latency.
 * @param stats The performance statstics.
 * @return Void.
 */
void PrintLatency(const PerfStats& stats);
/**
 * @brief Prints throughput.
 * @param stats The performance statstics.
 * @return Void.
 */
void PrintThroughput(const PerfStats& stats);
/**
 * @brief Prints performance statistics.
 * @param stats The performance statstics.
 * @return Void.
 */
void PrintPerfStats(const PerfStats& stats);

/**
 * @brief Calculates performance statistics.
 *
 * Reads useful data from database and then calculates performance statistics including latency, throughput, and so on.
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
   * @brief Calculates latency.
   *
   * Calculates latency from the previous time that this API is called, to now.
   * Then the performance statistics are updated and saved. 
   * The time when this API is called is recorded and will be used in calculation in the call of this API next time.
   *
   * @param sql A shared pointer points to Sqlite, which is a class for operating the database.
   * @param type The type of perf.
   * @param start_key The index name of start-time points in database.
   * @param end_key The index name of end-time points in database.
   *
   * @return Returns the performance statistics.
   */
  PerfStats CalcLatency(std::shared_ptr<Sqlite> sql, std::string type, std::string start_key, std::string end_key);
  /**
   * @brief Calculates throughput. The formlua is as follows:
   *
   * throughput = frame_count / (maximum_of_the_end_time_points - minimum_of_the_start_time_points)
   *
   * @param sql A shared pointer points to Sqlite, which is a class for operating the database.
   * @param type The type of perf.
   * @param start_key The index name of the start-time points in database.
   * @param end_key The index name of the end-time points in database.
   *
   * @return Returns the performance statistics.
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
