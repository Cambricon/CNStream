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

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "cnstream_time_utility.hpp"
#include "sqlite_db.hpp"
#include "perf_calculator.hpp"
#include "perf_manager.hpp"

namespace cnstream {

TEST(PerfCalculator, PrintLatency) {
  PerfStats stats = {10100, 20010, 1600, 100.5};
  PrintLatency(stats);
}

TEST(PerfCalculator, PrintThroughput) {
  PerfStats stats = {10100, 20010, 1600, 100.5};
  PrintThroughput(stats);
}

TEST(PerfCalculator, PrintPerfStats) {
  PerfStats stats = {10123, 20001, 1600, 100.526};
  PrintPerfStats(stats);
}

TEST(PerfCalculator, Construct) {
  PerfCalculator perf_cal;
  EXPECT_NE(perf_cal.pre_time_, unsigned(0));
}

TEST(PerfCalculator, CalcLatency) {
  remove("test.db");
  PerfCalculator perf_cal;
  auto sql = std::make_shared<Sqlite>("test.db");
  std::string table_name = "TEST";
  sql->Connect();
  std::vector<std::string> keys = {"a", "b"};
  sql->CreateTable(table_name, "ID", keys);

  size_t total = 0;
  size_t max = 0;
  uint32_t data_num = 10;
  for (uint32_t i = 0; i < data_num; i++) {
    size_t start = TimeStamp::Current();
    std::this_thread::sleep_for(std::chrono::microseconds(10 + i));
    size_t end = TimeStamp::Current();
    sql->Insert(table_name, "ID,a,b", std::to_string(i) + ","+ std::to_string(start) + "," + std::to_string(end));
    size_t duration = end - start;
    total += duration;
    if (duration > max) max = duration;
  }
  PerfStats stats = perf_cal.CalcLatency(sql, table_name, keys[0], keys[1]);
#ifdef HAVE_SQLITE
  EXPECT_EQ(stats.latency_avg, total / data_num);
  EXPECT_EQ(stats.latency_max, max);
  EXPECT_EQ(stats.frame_cnt, data_num);
  EXPECT_EQ(perf_cal.stats_.latency_avg, total / data_num);
  EXPECT_EQ(perf_cal.stats_.latency_max, max);
  EXPECT_EQ(perf_cal.stats_.frame_cnt, data_num);
#else
  EXPECT_EQ(stats.latency_avg, (unsigned)0);
  EXPECT_EQ(stats.latency_max, (unsigned)0);
  EXPECT_EQ(stats.frame_cnt, (unsigned)0);
#endif
  remove("test.db");
}

void CheckForZeroLatency(PerfStats stats, uint32_t line) {
  EXPECT_EQ(stats.latency_avg, (unsigned)0) << "wrong line = " << line;
  EXPECT_EQ(stats.latency_max, (unsigned)0) << "wrong line = " << line;
  EXPECT_EQ(stats.frame_cnt, (unsigned)0) << "wrong line = " << line;
}

TEST(PerfCalculator, CalcLatencyFailedCase) {
  remove("test.db");
  PerfCalculator perf_cal;
  PerfStats stats = perf_cal.CalcLatency(nullptr, "", "", "");
  CheckForZeroLatency(stats, __LINE__);

  auto sql = std::make_shared<Sqlite>("test.db");
  std::string table_name = "TEST";
  sql->Connect();
  std::vector<std::string> keys = {"a", "b"};
  sql->CreateTable(table_name, "ID", keys);

  stats = perf_cal.CalcLatency(sql, "", "", "");
  CheckForZeroLatency(stats, __LINE__);
  // no start and end time
  stats = perf_cal.CalcLatency(sql, table_name, keys[0], keys[1]);
  CheckForZeroLatency(stats, __LINE__);

  // no start, but only end time
  size_t end = TimeStamp::Current();
  sql->Insert(table_name, "ID,b", "0,"+ std::to_string(end));
  stats = perf_cal.CalcLatency(sql, table_name, keys[0], keys[1]);
  CheckForZeroLatency(stats, __LINE__);
  std::this_thread::sleep_for(std::chrono::microseconds(10));
  size_t start = TimeStamp::Current();
  sql->Update(table_name, "a", std::to_string(start), "ID", "0");
  // end - start is negtive
  stats = perf_cal.CalcLatency(sql, table_name, keys[0], keys[1]);
  CheckForZeroLatency(stats, __LINE__);
  remove("test.db");
}

TEST(PerfCalculator, CalcThroughput) {
  remove("test.db");
  PerfCalculator perf_cal;
  auto sql = std::make_shared<Sqlite>("test.db");
  std::string table_name = "TEST";
  sql->Connect();
  std::vector<std::string> keys = {"a", "b"};
  sql->CreateTable(table_name, "ID", keys);

  uint32_t data_num = 10;
  size_t start = TimeStamp::Current();
  sql->Insert(table_name, "ID,a,b", "0,"+ std::to_string(start) + "," + std::to_string(TimeStamp::Current()));
  for (uint32_t i = 1; i < data_num - 1; i++) {
    size_t s = TimeStamp::Current();
    std::this_thread::sleep_for(std::chrono::microseconds(10 + i));
    size_t e = TimeStamp::Current();
    sql->Insert(table_name, "ID,a,b", std::to_string(i) + ","+ std::to_string(s) + "," + std::to_string(e));
  }
  size_t end = TimeStamp::Current();
  sql->Insert(table_name, "ID,a,b", std::to_string(data_num - 1) + "," + std::to_string(TimeStamp::Current()) +
              "," + std::to_string(end));
  PerfStats stats = perf_cal.CalcThroughput(sql, table_name, keys[0], keys[1]);
#ifdef HAVE_SQLITE
  EXPECT_EQ(stats.frame_cnt, data_num);
  EXPECT_DOUBLE_EQ(stats.fps, ceil(static_cast<double>(data_num) * 1e7 / (end -start)) / 10.0);
#else
  EXPECT_EQ(stats.frame_cnt, (unsigned)0);
  EXPECT_DOUBLE_EQ(stats.fps, 0);
#endif
  remove("test.db");
}

TEST(PerfCalculator, CalcThroughputFailedCase) {
  remove("test.db");
  PerfCalculator perf_cal;
  PerfStats stats = perf_cal.CalcThroughput(nullptr, "", "", "");
  CheckForZeroLatency(stats, __LINE__);

  auto sql = std::make_shared<Sqlite>("test.db");
  std::string table_name = "TEST";
  sql->Connect();
  std::vector<std::string> keys = {"a", "b"};
  sql->CreateTable(table_name, "ID", keys);

  stats = perf_cal.CalcThroughput(sql, "", "", "");
  CheckForZeroLatency(stats, __LINE__);
  // no start and end time
  stats = perf_cal.CalcThroughput(sql, table_name, keys[0], keys[1]);
  CheckForZeroLatency(stats, __LINE__);

  // no start, but only end time
  size_t end = TimeStamp::Current();
  sql->Insert(table_name, "ID,b", "0,"+ std::to_string(end));
  stats = perf_cal.CalcThroughput(sql, table_name, keys[0], keys[1]);
  CheckForZeroLatency(stats, __LINE__);
  std::this_thread::sleep_for(std::chrono::microseconds(10));
  size_t start = TimeStamp::Current();
  sql->Update(table_name, "a", std::to_string(start), "ID", "0");
  // end - start is negtive
  stats = perf_cal.CalcThroughput(sql, table_name, keys[0], keys[1]);
  CheckForZeroLatency(stats, __LINE__);

  // no end, but only start time
  sql->Delete(table_name, "ID", "0");
  sql->Insert(table_name, "ID,a", "0,"+ std::to_string(start));
  stats = perf_cal.CalcThroughput(sql, table_name, keys[0], keys[1]);
  CheckForZeroLatency(stats, __LINE__);
  remove("test.db");
}

}  // namespace cnstream
