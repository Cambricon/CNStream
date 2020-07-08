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

#include "perf_calculator.hpp"
#include "perf_manager.hpp"
#include "sqlite_db.hpp"
#include "util/cnstream_time_utility.hpp"

namespace cnstream {

extern bool CreateDir(std::string path);

extern std::string gTestPerfDir;
extern std::string gTestPerfFile;

TEST(PerfCalculator, PrintLatency) {
  PerfStats stats;
  stats.latency_avg = 10100;
  stats.latency_max = 20100;
  stats.latency_min = 5100;
  stats.frame_cnt = 1600;
  PrintLatency(stats);
}

TEST(PerfCalculator, PrintThroughput) {
  PerfStats stats;
  stats.frame_cnt = 1600;
  stats.fps = 100.5;
  PrintThroughput(stats);
  PrintThroughput(stats, false);
}

TEST(PerfCalculator, SetAndGetPerfUtils) {
  PerfCalculator perf_cal;
  std::shared_ptr<PerfUtils> utils = std::make_shared<PerfUtils>();
  EXPECT_FALSE(perf_cal.SetPerfUtils(nullptr));
  EXPECT_TRUE(perf_cal.SetPerfUtils(utils));
  EXPECT_TRUE(perf_cal.GetPerfUtils() == utils);
}

TEST(PerfCalculator, CalcLatency) {
#ifdef HAVE_SQLITE
  CreateDir(gTestPerfDir);
  remove(gTestPerfFile.c_str());

  PerfCalculator perf_cal;

  auto sql = std::make_shared<Sqlite>(gTestPerfFile);
  std::string table_name = "TEST";
  std::string sql_name = "sql";
  sql->Connect();
  std::vector<std::string> keys = {"a", "b"};
  sql->CreateTable(table_name, "ID", keys);

  perf_cal.GetPerfUtils()->AddSql(sql_name, sql);

  uint32_t fake_data_num = 3;
  size_t start[3] = {1000, 4000, 6000};
  size_t end[3] = {3000, 8000, 11000};
  size_t max = 0;
  size_t min = ~(0);
  size_t total = 0;
  for (int i = 0; i < 3; i++) {
    sql->Insert(table_name, "ID,a,b",
                std::to_string(i) + "," + std::to_string(start[i]) + "," + std::to_string(end[i]));
    size_t duration = end[i] - start[i];
    if (duration > max) max = duration;
    if (duration < min) min = duration;
    total += duration;
  }
  PerfStats stats = perf_cal.CalcLatency(sql_name, table_name, {keys[0], keys[1]});

  EXPECT_EQ(stats.latency_max, max);
  EXPECT_EQ(stats.latency_min, min);
  EXPECT_EQ(stats.latency_avg, total / fake_data_num);
  EXPECT_EQ(stats.frame_cnt, fake_data_num);

  uint32_t data_num = 10;
  total = 0;
  for (uint32_t i = fake_data_num; i < data_num; i++) {
    size_t start = TimeStamp::Current();
    std::this_thread::sleep_for(std::chrono::microseconds(10 + i));
    size_t end = TimeStamp::Current();
    sql->Insert(table_name, "ID,a,b", std::to_string(i) + "," + std::to_string(start) + "," + std::to_string(end));
    size_t duration = end - start;
    total += duration;
    if (duration > max) max = duration;
    if (duration < min) min = duration;
  }

  size_t avg =
      (stats.latency_avg * fake_data_num + total / (data_num - fake_data_num) * (data_num - fake_data_num)) / data_num;

  stats = perf_cal.CalcLatency(sql_name, table_name, {keys[0], keys[1]});

  EXPECT_EQ(stats.latency_avg, avg);
  EXPECT_EQ(stats.latency_max, max);
  EXPECT_EQ(stats.frame_cnt, data_num);
  EXPECT_EQ(perf_cal.GetLatency(sql_name, table_name).latency_avg, avg);
  EXPECT_EQ(perf_cal.GetLatency(sql_name, table_name).latency_max, max);
  EXPECT_EQ(perf_cal.GetLatency(sql_name, table_name).frame_cnt, data_num);
  sql->Close();
  remove(gTestPerfFile.c_str());
#endif
}

TEST(PerfCalculatorForModule, CalcThroughput) {
#ifdef HAVE_SQLITE
  CreateDir(gTestPerfDir);
  remove(gTestPerfFile.c_str());

  PerfCalculatorForModule perf_cal;

  auto sql = std::make_shared<Sqlite>(gTestPerfFile);
  std::string table_name = "TEST";
  std::string sql_name = "sql";
  sql->Connect();
  std::vector<std::string> keys = {"a", "b", "th"};
  sql->CreateTable(table_name, "ID", keys);

  perf_cal.GetPerfUtils()->AddSql(sql_name, sql);

  // throughput = 1 / (0.003 - 0.001) + 2 / (0.011 - 0.004) = 785.8
  uint32_t data_num = 3;
  size_t start[3] = {1000, 4000, 6000};
  size_t end[3] = {3000, 8000, 11000};
  std::vector<std::string> th_ids = {"th_0", "th_1", "th_1"};

  for (int i = 0; i < 3; i++) {
    sql->Insert(
        table_name, "ID,a,b,th",
        std::to_string(i) + "," + std::to_string(start[i]) + "," + std::to_string(end[i]) + ",'" + th_ids[i] + "'");
  }
  PerfStats stats = perf_cal.CalcThroughput("", table_name, {keys[0], keys[1], keys[2]});

  double th_0_fps = ceil(1e7 / (end[0] - start[0])) / 10;
  double th_1_fps = ceil(2e7 / (end[2] - start[1])) / 10;
  EXPECT_DOUBLE_EQ(stats.fps, th_0_fps + th_1_fps);
  EXPECT_EQ(stats.frame_cnt, data_num);
  std::vector<PerfStats> stats_th_0 = perf_cal.GetThroughput(th_ids[0], table_name);
  ASSERT_EQ(stats_th_0.size(), 1);
  EXPECT_EQ(stats_th_0[0].fps, th_0_fps);
  EXPECT_EQ(stats_th_0[0].frame_cnt, 1);
  std::vector<PerfStats> stats_th_1 = perf_cal.GetThroughput(th_ids[1], table_name);
  ASSERT_EQ(stats_th_1.size(), 1);
  EXPECT_EQ(stats_th_1[0].fps, th_1_fps);
  EXPECT_EQ(stats_th_1[0].frame_cnt, 2);
  std::vector<PerfStats> stats_vec = perf_cal.GetThroughput("", table_name);
  ASSERT_EQ(stats_vec.size(), 1);
  EXPECT_EQ(stats_vec[0].fps, th_0_fps + th_1_fps);
  EXPECT_EQ(stats_vec[0].frame_cnt, data_num);

  sql->Close();
  remove(gTestPerfFile.c_str());
#endif
}

TEST(PerfCalculatorForPipeline, CalcThroughput) {
  CreateDir(gTestPerfDir);
  remove(gTestPerfFile.c_str());

  PerfCalculatorForPipeline perf_cal;

  auto sql = std::make_shared<Sqlite>(gTestPerfFile);
  std::string table_name = "TEST";
  std::string sql_name = "sql";
  sql->Connect();
  std::string key = "end";
  sql->CreateTable(table_name, "ID", {key});

  perf_cal.GetPerfUtils()->AddSql(sql_name, sql);

  uint32_t data_num_1 = 3;
  size_t end[3] = {3000, 8000, 11000};
  for (int i = 0; i < 3; i++) {
    sql->Insert(table_name, "ID,end", std::to_string(i) + "," + std::to_string(end[i]));
  }
  PerfStats stats = perf_cal.CalcThroughput(sql_name, table_name, {key});

#ifdef HAVE_SQLITE
  double fps = ceil(data_num_1 * 1e7 / (end[2] - end[0])) / 10;
  EXPECT_EQ(stats.fps, fps);
  EXPECT_EQ(stats.frame_cnt, data_num_1);
#endif

  uint32_t data_num_2 = 7;
  size_t end_ts = 0;
  for (uint32_t i = data_num_1; i < data_num_1 + data_num_2; i++) {
    std::this_thread::sleep_for(std::chrono::microseconds(10 + i));
    end_ts = TimeStamp::Current();
    sql->Insert(table_name, "ID,end", std::to_string(i) + "," + std::to_string(end_ts));
  }

  stats = perf_cal.CalcThroughput(sql_name, table_name, {key});

#ifdef HAVE_SQLITE
  fps = ceil((data_num_2)*1e7 / (end_ts - end[data_num_1 - 1])) / 10;
  std::cout << "fps " << fps << " stats fps " << stats.fps << std::endl;
  EXPECT_EQ(stats.fps, fps);
  EXPECT_EQ(stats.frame_cnt, data_num_2);
  fps = ceil((data_num_1 + data_num_2) * 1e7 / (end_ts - end[0])) / 10;
  stats = perf_cal.GetAvgThroughput(sql_name, table_name);
  EXPECT_EQ(stats.fps, fps);
  EXPECT_EQ(stats.frame_cnt, data_num_1 + data_num_2);
#endif

  sql->Close();
  remove(gTestPerfFile.c_str());
}

TEST(PerfCalculatorForPipeline, CalcTotalThroughput) {
#ifdef HAVE_SQLITE
  CreateDir(gTestPerfDir);
  std::vector<std::string> sql_name = {"sql0", "sql1"};
  remove((gTestPerfDir + sql_name[0]).c_str());
  remove((gTestPerfDir + sql_name[1]).c_str());

  PerfCalculatorForPipeline perf_cal;

  std::vector<std::shared_ptr<Sqlite>> sql_vec;
  auto sql0 = std::make_shared<Sqlite>(gTestPerfDir + sql_name[0]);
  auto sql1 = std::make_shared<Sqlite>(gTestPerfDir + sql_name[1]);
  sql_vec.push_back(sql0);
  sql_vec.push_back(sql1);

  std::string table_name = "TEST";
  std::string key = {"end"};
  uint32_t end_s = 2000;
  uint32_t end_e_1 = 10000;
  uint32_t end_e_2 = 15000;
  std::vector<size_t> data0 = {2000, 3000, 6000, 15000};
  std::vector<size_t> data1 = {3000, 5000, 7000, 10000};
  std::vector<std::vector<size_t>> data = {data0, data1};
  size_t total_num = data0.size() * data.size();
  size_t num_2 = 1;
  size_t num_1 = total_num - num_2;

  for (uint32_t i = 0; i < sql_vec.size(); i++) {
    EXPECT_TRUE(sql_vec[i]->Connect());
    EXPECT_TRUE(sql_vec[i]->CreateTable(table_name, "ID", {key}));
    perf_cal.GetPerfUtils()->AddSql(sql_name[i], sql_vec[i]);
    for (uint32_t data_i = 0; data_i < data0.size(); data_i++) {
      EXPECT_TRUE(
          sql_vec[i]->Insert(table_name, "ID,end", std::to_string(data_i) + "," + std::to_string(data[i][data_i])));
    }
  }

  PerfStats stats = perf_cal.CalcThroughput("", table_name, {key});

  double fps = ceil(num_1 * 1e7 / (end_e_1 - end_s)) / 10;
  EXPECT_DOUBLE_EQ(stats.fps, fps);
  EXPECT_EQ(stats.frame_cnt, num_1);
  std::vector<PerfStats> stats_vec = perf_cal.GetThroughput("", table_name);
  ASSERT_EQ(stats_vec.size(), 1);
  EXPECT_EQ(stats_vec[0].fps, fps);
  EXPECT_EQ(stats_vec[0].frame_cnt, num_1);

  stats = perf_cal.CalcThroughput("", table_name, {key});

  fps = ceil(num_2 * 1e7 / (end_e_2 - end_e_1)) / 10;
  EXPECT_DOUBLE_EQ(stats.fps, fps);
  EXPECT_EQ(stats.frame_cnt, num_2);
  stats_vec = perf_cal.GetThroughput("", table_name);
  ASSERT_EQ(stats_vec.size(), 2);
  EXPECT_EQ(stats_vec[1].fps, fps);
  EXPECT_EQ(stats_vec[1].frame_cnt, num_2);
  // avg
  PerfStats avg = perf_cal.GetAvgThroughput("", table_name);
  fps = ceil(total_num * 1e7 / (end_e_2 - end_s)) / 10;
  EXPECT_DOUBLE_EQ(avg.fps, fps);
  EXPECT_EQ(avg.frame_cnt, total_num);

  for (uint32_t i = 0; i < sql_vec.size(); i++) {
    sql_vec[i]->Close();
    remove((gTestPerfDir + sql_name[i]).c_str());
  }
#endif
}

TEST(PerfCalculatorForInfer, CalcThroughput) {
#ifdef HAVE_SQLITE
  CreateDir(gTestPerfDir);
  remove(gTestPerfFile.c_str());

  PerfCalculatorForInfer perf_cal;

  auto sql = std::make_shared<Sqlite>(gTestPerfFile);
  std::string sql_name = "sql";
  sql->Connect();
  std::vector<std::string> table_name = {"TEST0", "TEST1"};
  std::vector<std::string> keys = {"a", "b", "th"};
  sql->CreateTable(table_name[0], "ID", keys);
  sql->CreateTable(table_name[1], "ID", keys);

  perf_cal.GetPerfUtils()->AddSql(sql_name, sql);

  uint32_t data_num = 3;
  size_t start[6] = {1000, 4000, 6000, 2000, 3000, 7000};
  size_t end[6] = {3000, 8000, 11000, 4000, 5000, 9000};

  for (int i = 0; i < 3; i++) {
    sql->Insert(table_name[0], "ID,a,b",
                std::to_string(i) + "," + std::to_string(start[i]) + "," + std::to_string(end[i]));
    sql->Insert(
        table_name[1], "ID,a,b",
        std::to_string(i) + "," + std::to_string(start[data_num + i]) + "," + std::to_string(end[data_num + i]));
  }
  PerfStats stats_0 = perf_cal.CalcThroughput(sql_name, table_name[0], {keys[0], keys[1]});
  PerfStats stats_1 = perf_cal.CalcThroughput(sql_name, table_name[1], {keys[0], keys[1]});

  size_t duration[2] = {0, 0};
  for (uint32_t i = 0; i < 2; i++) {
    for (uint32_t data_i = 0; data_i < data_num; data_i++) {
      uint32_t index = data_i + data_num * i;
      if (data_i == 0) {
        duration[i] += end[index] - start[index];
      } else {
        duration[i] += end[index] - (start[index] > end[index - 1] ? start[index] : end[index - 1]);
      }
    }
  }
  double th_0_fps = ceil(data_num * 1e7 / duration[0]) / 10;
  double th_1_fps = ceil(data_num * 1e7 / duration[1]) / 10;
  EXPECT_DOUBLE_EQ(stats_0.fps, th_0_fps);
  EXPECT_EQ(stats_0.frame_cnt, data_num);
  EXPECT_DOUBLE_EQ(stats_1.fps, th_1_fps);
  EXPECT_EQ(stats_1.frame_cnt, data_num);
  std::vector<PerfStats> stats_th_0 = perf_cal.GetThroughput(sql_name, table_name[0]);
  ASSERT_EQ(stats_th_0.size(), 1);
  EXPECT_EQ(stats_th_0[0].fps, th_0_fps);
  EXPECT_EQ(stats_th_0[0].frame_cnt, data_num);
  std::vector<PerfStats> stats_th_1 = perf_cal.GetThroughput(sql_name, table_name[1]);
  ASSERT_EQ(stats_th_1.size(), 1);
  EXPECT_EQ(stats_th_1[0].fps, th_1_fps);
  EXPECT_EQ(stats_th_1[0].frame_cnt, data_num);

  sql->Close();
  remove(gTestPerfFile.c_str());
#endif
}

TEST(PerfCalculationMethod, ThroughputAndLatency) {
  PerfCalculationMethod method;
  size_t start_time = 1000;
  std::vector<DbIntegerItem> item_vec = {{2000}, {4000}};
  PerfStats stats = method.CalcThroughput(start_time, item_vec);
  EXPECT_EQ(stats.frame_cnt, 0);
  EXPECT_DOUBLE_EQ(stats.fps, 0);

  stats = method.CalcLatency(item_vec);
  EXPECT_EQ(stats.frame_cnt, 0);
  EXPECT_EQ(stats.latency_avg, 0);
  EXPECT_EQ(stats.latency_max, 0);
  EXPECT_EQ(stats.latency_min, 0);

  item_vec.clear();
  item_vec.push_back({1000, 4000});
  item_vec.push_back({3000, 5000});
  size_t fps_duration = 0;
  size_t latency_duration = 0;
  size_t max = 0;
  size_t min = ~(0);
  for (uint32_t i = 0; i < item_vec.size(); i++) {
    if (i == 0) {
      fps_duration += item_vec[i][1] - item_vec[i][0];
    } else {
      fps_duration += item_vec[i][1] - (item_vec[i][0] > item_vec[i - 1][1] ? item_vec[i][0] : item_vec[i - 1][1]);
    }
    size_t duration = item_vec[i][1] - item_vec[i][0];
    if (duration > max) max = duration;
    if (duration < min) min = duration;
    latency_duration += duration;
  }
  double fps = ceil(item_vec.size() * 1e7 / fps_duration) / 10;

  stats = method.CalcThroughput(start_time, item_vec);

  EXPECT_EQ(stats.frame_cnt, item_vec.size());
  EXPECT_DOUBLE_EQ(stats.fps, fps);

  stats = method.CalcLatency(item_vec);
  EXPECT_EQ(stats.frame_cnt, item_vec.size());
  EXPECT_EQ(stats.latency_avg, latency_duration / item_vec.size());
  EXPECT_EQ(stats.latency_max, max);
  EXPECT_EQ(stats.latency_min, min);

  item_vec.clear();
  item_vec.push_back({1000, 4000, 2});
  item_vec.push_back({3000, 5000, 3});
  size_t frame_cnt = 0;
  latency_duration = 0;
  for (auto it : item_vec) {
    frame_cnt += it[2];
    latency_duration += (it[1] - it[0]) * it[2];
  }
  fps = ceil(frame_cnt * 1e7 / fps_duration) / 10;

  stats = method.CalcThroughput(start_time, item_vec);
  EXPECT_EQ(stats.frame_cnt, frame_cnt);
  EXPECT_DOUBLE_EQ(stats.fps, fps);

  stats = method.CalcLatency(item_vec);
  EXPECT_EQ(stats.frame_cnt, frame_cnt);
  EXPECT_EQ(stats.latency_avg, latency_duration / frame_cnt);
  EXPECT_EQ(stats.latency_max, max);
  EXPECT_EQ(stats.latency_min, min);
}

TEST(PerfUtils, AddAndRemoveSql) {
  PerfUtils utils;
  std::string sql_name0 = "sql0";
  std::string sql_name1 = "sql1";
  std::shared_ptr<Sqlite> sql0 = std::make_shared<Sqlite>(sql_name0);
  std::shared_ptr<Sqlite> sql1 = std::make_shared<Sqlite>(sql_name1);
  EXPECT_TRUE(utils.AddSql(sql_name0, sql0));
  EXPECT_FALSE(utils.AddSql(sql_name1, nullptr));
  EXPECT_TRUE(utils.AddSql(sql_name1, sql1));
  EXPECT_FALSE(utils.AddSql(sql_name0, sql0));
  EXPECT_FALSE(utils.AddSql("", sql0));

  EXPECT_TRUE(utils.RemoveSql(sql_name0));
  EXPECT_TRUE(utils.RemoveSql(sql_name1));
  EXPECT_FALSE(utils.RemoveSql(sql_name1));
}

TEST(PerfUtils, Max) {
  std::vector<PerfStats> stats_vec;
  PerfStats stats1;
  stats1.frame_cnt = 1;
  stats1.fps = 0.f;
  stats_vec.push_back(stats1);
  PerfStats stats2;
  stats2.frame_cnt = 100;
  stats2.fps = 333.5;
  stats_vec.push_back(stats2);
  PerfStats stats3;
  stats3.frame_cnt = 2;
  stats3.fps = 2.5;
  stats_vec.push_back(stats3);
  auto max_stats = PerfUtils::Max(stats_vec, [](const PerfStats& v1, const PerfStats& v2) { return v1.fps < v2.fps; });
  EXPECT_DOUBLE_EQ(stats2.fps, max_stats.fps);
  EXPECT_EQ(stats2.frame_cnt, max_stats.frame_cnt);
}

}  // namespace cnstream
