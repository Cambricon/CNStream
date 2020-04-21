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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_time_utility.hpp"
#include "perf_calculator.hpp"
#include "perf_manager.hpp"
#include "sqlite_db.hpp"

namespace cnstream {

static const char kTableName[] = "PROCESS";
static const char kDbName[] = "test.db";
static std::vector<std::string> module_names = {"module_0", "module_1", "module_2", "module_3"};

TEST(PerfManager, Stop) {
  PerfManager manager;
  manager.Stop();
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  EXPECT_TRUE(manager.running_);
  manager.Stop();
  EXPECT_FALSE(manager.running_);
}

TEST(PerfManager, Init) {
  PerfManager manager;
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  EXPECT_TRUE(manager.sql_ != nullptr);
  EXPECT_TRUE(manager.is_initialized_);
  EXPECT_TRUE(manager.perf_type_.find(kTableName) != manager.perf_type_.end());
  std::string table_name = kTableName;
  for (auto it : module_names) {
    EXPECT_TRUE(manager.calculator_map_.find(table_name + "_" + it) != manager.calculator_map_.end());
  }
  EXPECT_TRUE(manager.running_);
  EXPECT_TRUE(manager.is_initialized_);
}

TEST(PerfManager, InitFailedCase) {
  PerfManager manager;
  EXPECT_FALSE(manager.Init("", module_names, module_names[0], {module_names[1], module_names[3]}));
  // moudle names should be unique
  std::vector<std::string> m_names = {"m1", "m", "m"};
#ifdef HAVE_SQLITE
  EXPECT_FALSE(manager.Init(kDbName, m_names, m_names[0], {m_names[2]}));
#endif
  // start node should be found in module names
  EXPECT_FALSE(manager.Init(kDbName, module_names, m_names[0], {module_names[1], module_names[3]}));
  // end nodes should be found in module names
  EXPECT_FALSE(manager.Init(kDbName, module_names, module_names[0], {m_names[1]}));
  // can not init twice
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  EXPECT_FALSE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));

  // db file is holded by manager.
  PerfManager manager2;
  EXPECT_FALSE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
}

TEST(PerfManager, RecordPerfInfo) {
  PerfManager manager;
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  for (uint32_t i = 0; i < module_names.size(); i++) {
    PerfInfo info {false, kTableName, module_names[i], 0};
    EXPECT_TRUE(manager.RecordPerfInfo(info));
    info.is_finished = true;
    EXPECT_TRUE(manager.RecordPerfInfo(info));
  }
  manager.Stop();
#ifdef HAVE_SQLITE
  for (uint32_t i = 0; i < module_names.size(); i++) {
    EXPECT_EQ(manager.sql_->Count(kTableName, module_names[i] + "_stime"), (unsigned)1);
    EXPECT_EQ(manager.sql_->Count(kTableName, module_names[i] + "_etime"), (unsigned)1);
  }
#endif
}

TEST(PerfManager, RecordPerfInfoFailedCase) {
  PerfManager manager;
  PerfInfo info {false, kTableName, module_names[0], 0};
  EXPECT_FALSE(manager.RecordPerfInfo(info));
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  for (uint32_t i = 0; i < module_names.size(); i++) {
    PerfInfo info {false, kTableName, module_names[i], 0};
    EXPECT_TRUE(manager.RecordPerfInfo(info));
    info.is_finished = true;
    EXPECT_TRUE(manager.RecordPerfInfo(info));
  }
  manager.Stop();
#ifdef HAVE_SQLITE
  for (uint32_t i = 0; i < module_names.size(); i++) {
    EXPECT_EQ(manager.sql_->Count(kTableName, module_names[i] + "_stime"), (unsigned)1);
    EXPECT_EQ(manager.sql_->Count(kTableName, module_names[i] + "_etime"), (unsigned)1);
  }
#endif
  EXPECT_FALSE(manager.RecordPerfInfo(info));
}

void ThreadFunc(int i, std::vector<std::string> m_names, PerfManager* manager, int64_t num) {
  for (int64_t pts = 0; pts < num; pts++) {
    PerfInfo info {false, kTableName, m_names[i%4], pts};
    EXPECT_TRUE(manager->RecordPerfInfo(info));
  }
  for (int64_t pts = 0; pts < num; pts++) {
    PerfInfo info {true, kTableName, m_names[i%4], pts};
    EXPECT_TRUE(manager->RecordPerfInfo(info));
  }
}

TEST(PerfManager, MultiThreadRecordInfo) {
  PerfManager manager;
  std::vector<std::string> end_module_names = {module_names[1], module_names[3]};
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], end_module_names));
  manager.SqlBeginTrans();

  std::vector<std::thread> ths;
  int64_t data_num = 100;
  for (int i = 0; i < 50; i++) {
    ths.push_back(std::thread(&ThreadFunc, i, module_names, &manager, data_num));
  }
  for (auto &it : ths) {
    if (it.joinable()) it.join();
  }
  manager.Stop();
  manager.SqlCommitTrans();
#ifdef HAVE_SQLITE
  for (uint32_t i = 0; i < module_names.size(); i++) {
    EXPECT_EQ(manager.sql_->Count(kTableName, module_names[i] + "_stime"), (unsigned)data_num);
    EXPECT_EQ(manager.sql_->Count(kTableName, module_names[i] + "_etime"), (unsigned)data_num);
  }
#endif
}

TEST(PerfManager, InsertInfoToDb) {
  PerfManager manager;
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  EXPECT_TRUE(manager.sql_ != nullptr);

  int64_t pts = 0;
  PerfInfo info {false, kTableName, module_names[0], pts};
  EXPECT_NO_THROW(manager.InsertInfoToDb(info));
#ifdef HAVE_SQLITE
  EXPECT_EQ(manager.sql_->Count(kTableName, "pts", "pts=" + std::to_string(pts)), unsigned(1));
  EXPECT_EQ(manager.sql_->Count(kTableName, module_names[0] + "_stime", "pts=" + std::to_string(pts)), unsigned(1));
#endif

  info.is_finished = true;
  EXPECT_NO_THROW(manager.InsertInfoToDb(info));
#ifdef HAVE_SQLITE
  EXPECT_EQ(manager.sql_->Count(kTableName, module_names[0] + "_etime", "pts=" + std::to_string(pts)), unsigned(1));
#endif

  info.is_finished = false;
  info.module_name = module_names[1];
  EXPECT_NO_THROW(manager.InsertInfoToDb(info));
#ifdef HAVE_SQLITE
  EXPECT_EQ(manager.sql_->Count(kTableName, module_names[1] + "_stime", "pts=" + std::to_string(pts)), unsigned(1));
#endif

  info.is_finished = true;
  EXPECT_NO_THROW(manager.InsertInfoToDb(info));
#ifdef HAVE_SQLITE
  EXPECT_EQ(manager.sql_->Count(kTableName, module_names[1] + "_etime", "pts=" + std::to_string(pts)), unsigned(1));
#endif
}

TEST(PerfManager, InsertInfoToDbFailedCase) {
  PerfManager manager;
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  EXPECT_TRUE(manager.sql_ != nullptr);

  PerfInfo info {false, "wrong_type", module_names[0], 0};
  EXPECT_NO_THROW(manager.InsertInfoToDb(info));
  EXPECT_EQ(manager.sql_->Count(kTableName, "pts", "pts=0"), unsigned(0));
}

TEST(PerfManager, RegisterPerfType) {
  PerfManager manager;
  std::string type1 = "type1";
  std::string type2 = "type2";
  EXPECT_TRUE(manager.RegisterPerfType(type1));
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  EXPECT_TRUE(manager.sql_ != nullptr);
  EXPECT_TRUE(manager.RegisterPerfType(type2));

  PerfInfo info {false, type1, module_names[0], 0};
  EXPECT_NO_THROW(manager.InsertInfoToDb(info));
#ifdef HAVE_SQLITE
  EXPECT_EQ(manager.sql_->Count(type1, "pts", "pts=0"), unsigned(1));
#endif

  info.perf_type = type2;
  EXPECT_NO_THROW(manager.InsertInfoToDb(info));
#ifdef HAVE_SQLITE
  EXPECT_EQ(manager.sql_->Count(type2, "pts", "pts=0"), unsigned(1));
#endif
}

TEST(PerfManager, RegisterPerfTypeFailedCase) {
  PerfManager manager;
  EXPECT_FALSE(manager.RegisterPerfType(""));
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  EXPECT_TRUE(manager.RegisterPerfType(kTableName));
}

TEST(PerfManager, GetKeys) {
  PerfManager manager;
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  std::vector<std::string> keys = manager.GetKeys(module_names);
  EXPECT_EQ(keys.size(), module_names.size() * 2);
  std::vector<std::string> suffix = {"_stime", "_etime"};
  for (uint32_t i = 0; i < module_names.size(); i++) {
    EXPECT_TRUE(keys[i] == module_names[i/2] + suffix[i%2]);
  }
}

TEST(PerfManager, GetKeysFailedCase) {
  PerfManager manager;
  std::vector<std::string> keys = manager.GetKeys(module_names);
  EXPECT_EQ(keys.size(), unsigned(0));
}

TEST(PerfManager, CreatePerfCalculator) {
  PerfManager manager;
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  manager.CreatePerfCalculator("type1");
  for (uint32_t i = 0; i < module_names.size(); i++) {
    EXPECT_TRUE(manager.calculator_map_.find("type1_" + module_names[i]) != manager.calculator_map_.end());
    if (i == 1 || i == 3) {
      EXPECT_TRUE(manager.calculator_map_.find("type1_" + module_names[i] + "_pipeline") !=
                  manager.calculator_map_.end());
    }
  }
}

TEST(PerfManager, SqlBeginAndCommit) {
  size_t start, end;
  size_t duration1, duration2;
  {
    PerfManager manager;
    EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
    start = TimeStamp::Current();
    manager.SqlBeginTrans();
    for (int64_t i = 0; i < 10000; i++) {
      PerfInfo info {false, kTableName, module_names[0], i};
      EXPECT_TRUE(manager.RecordPerfInfo(info));
    }
    manager.Stop();
    manager.SqlCommitTrans();
    end = TimeStamp::Current();
    duration1 = end - start;
  }
  {
    PerfManager manager;
    EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
    start = TimeStamp::Current();
    for (int64_t i = 0; i < 10000; i++) {
      PerfInfo info {false, kTableName, module_names[0], i};
      EXPECT_TRUE(manager.RecordPerfInfo(info));
    }
    manager.Stop();
    end = TimeStamp::Current();
    duration2 = end - start;
  }
#ifdef HAVE_SQLITE
  EXPECT_GT(duration2, duration1);
#else
  EXPECT_NE(duration1, (unsigned)0);
  EXPECT_NE(duration2, (unsigned)0);
#endif
}

TEST(PerfManager, PrepareDbFileDir) {
  std::string outer_path = "test_a/";
  std::string path = "test_a/test_b/";
  std::string db_name = kDbName;
  std::string db_path = path + db_name;
  {
    PerfManager manager;
    remove(db_path.c_str());
    rmdir(path.c_str());
    rmdir(outer_path.c_str());
    // path not exist, create path
    EXPECT_TRUE(manager.PrepareDbFileDir(db_path));
    EXPECT_EQ(access(path.c_str(), 0), 0);

    EXPECT_TRUE(manager.Init(db_path, module_names, module_names[0], {module_names[1], module_names[3]}));

#ifdef HAVE_SQLITE
    EXPECT_EQ(access(db_path.c_str(), 0), 0);
#else
    EXPECT_NE(access(db_path.c_str(), 0), 0);
#endif
  }

  {
    PerfManager manager;
    // file exist
    EXPECT_TRUE(manager.PrepareDbFileDir(db_path));
    EXPECT_NE(access(db_path.c_str(), 0), 0);
    EXPECT_EQ(access(path.c_str(), 0), 0);

    rmdir(path.c_str());
    rmdir(outer_path.c_str());
  }
}

TEST(PerfManager, PrepareDbFileDirFailedCase) {
  PerfManager manager;
  std::string db_path = "test.db";
  remove(db_path.c_str());

  EXPECT_FALSE(manager.PrepareDbFileDir(""));

  manager.sql_ = std::make_shared<Sqlite>(db_path);
  auto sql = manager.sql_;

  EXPECT_TRUE(manager.PrepareDbFileDir(db_path));
  sql->Connect();
#ifdef HAVE_SQLITE
  EXPECT_FALSE(manager.PrepareDbFileDir(db_path));
#else
  EXPECT_TRUE(manager.PrepareDbFileDir(db_path));
#endif
  sql->Close();
  EXPECT_TRUE(manager.PrepareDbFileDir(db_path));
}

TEST(PerfManager, GetCalculator) {
  PerfManager manager;
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  manager.CreatePerfCalculator("type1");
  for (uint32_t i = 0; i < module_names.size(); i++) {
    EXPECT_TRUE(manager.calculator_map_.find("type1_" + module_names[i]) != manager.calculator_map_.end());
    if (i == 1 || i == 3) {
      EXPECT_TRUE(manager.calculator_map_.find("type1_" + module_names[i] + "_pipeline") !=
                  manager.calculator_map_.end());
    }
  }
  EXPECT_TRUE(manager.GetCalculator("type1", module_names[0]) != nullptr);
  EXPECT_TRUE(manager.GetCalculator("type2", module_names[0]) == nullptr);
}

void CheckForPerfStats(PerfStats stats, bool success_case, uint32_t line) {
  if (success_case) {
    EXPECT_NE(stats.latency_avg, (unsigned)0) << "wrong line = " << line;
    EXPECT_DOUBLE_EQ(stats.fps, 1e9 / stats.latency_avg / 1000.f) << "wrong line = " << line;
    EXPECT_EQ(stats.latency_avg, stats.latency_max) << "wrong line = " << line;
    EXPECT_EQ(stats.frame_cnt, (unsigned)1) << "wrong line = " << line;
  } else {
    EXPECT_EQ(stats.latency_avg, (unsigned)0) << "wrong line = " << line;
    EXPECT_EQ(stats.latency_max, (unsigned)0) << "wrong line = " << line;
    EXPECT_DOUBLE_EQ(stats.fps, 0) << "wrong line = " << line;
    EXPECT_EQ(stats.frame_cnt, (unsigned)0) << "wrong line = " << line;
  }
}
TEST(PerfManager, CalculatePerfStats) {
  PerfManager manager;
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  PerfInfo info {false, kTableName, module_names[0], 0};
  manager.RecordPerfInfo(info);
  std::this_thread::sleep_for(std::chrono::microseconds(100));
  info.is_finished = true;
  manager.RecordPerfInfo(info);
  manager.Stop();
  PerfStats stats = manager.CalculatePerfStats(kTableName, module_names[0]);
#ifdef HAVE_SQLITE
  CheckForPerfStats(stats, true, __LINE__);
#else
  CheckForPerfStats(stats, false, __LINE__);
#endif
}

TEST(PerfManager, CalculatePerfStatsFailedCase) {
  PerfManager manager;
  PerfStats stats = manager.CalculatePerfStats(kTableName, module_names[0]);
  CheckForPerfStats(stats, false, __LINE__);

  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  stats = manager.CalculatePerfStats(kTableName, module_names[0]);
  CheckForPerfStats(stats, false, __LINE__);

  stats = manager.CalculatePerfStats("wrong_table", module_names[0]);
  CheckForPerfStats(stats, false, __LINE__);
}

void CheckForPipelinePerfStats(std::vector<std::pair<std::string, PerfStats>> vec_stats, bool success_case,
                               uint32_t line) {
  EXPECT_EQ(vec_stats.size(), (unsigned)2) << "wrong line = " << line;
  EXPECT_TRUE(vec_stats[0].first == module_names[1]) << "wrong line = " << line;
  EXPECT_TRUE(vec_stats[1].first == module_names[3]) << "wrong line = " << line;
  for (uint32_t i = 0; i < 2; i++) {
    if (success_case) {
      EXPECT_NE(vec_stats[i].second.latency_avg, (unsigned)0) << "wrong line = " << line;
      EXPECT_NE(vec_stats[i].second.fps, 0) << "wrong line = " << line;
      EXPECT_EQ(vec_stats[i].second.latency_avg, vec_stats[i].second.latency_max) << "wrong line = " << line;
      EXPECT_EQ(vec_stats[i].second.frame_cnt, (unsigned)1) << "wrong line = " << line;
    } else {
      CheckForPerfStats(vec_stats[i].second, false, line);
    }
  }
}

TEST(PerfManager, CalculatePipelinePerfStats) {
  PerfManager manager;
  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  PerfInfo info1 {false, kTableName, module_names[0], 0};
  manager.RecordPerfInfo(info1);
  std::this_thread::sleep_for(std::chrono::microseconds(100));
  PerfInfo info2 {true, kTableName, module_names[1], 0};
  manager.RecordPerfInfo(info2);
  std::this_thread::sleep_for(std::chrono::microseconds(100));
  PerfInfo info3 {true, kTableName, module_names[3], 0};
  manager.RecordPerfInfo(info3);
  manager.Stop();
  auto vec_stats = manager.CalculatePipelinePerfStats(kTableName);
#ifdef HAVE_SQLITE
  CheckForPipelinePerfStats(vec_stats, true, __LINE__);
#endif
}

TEST(PerfManager, CalculatePipelinePerfStatsFailedCase) {
  PerfManager manager;
  auto vec_stats_1 = manager.CalculatePipelinePerfStats(kTableName);
  EXPECT_EQ(vec_stats_1.size(), (unsigned)0);

  EXPECT_TRUE(manager.Init(kDbName, module_names, module_names[0], {module_names[1], module_names[3]}));
  auto vec_stats_2 = manager.CalculatePipelinePerfStats(kTableName);
  CheckForPipelinePerfStats(vec_stats_2, false, __LINE__);
  auto vec_stats_3 = manager.CalculatePipelinePerfStats("wrong_table");
  CheckForPipelinePerfStats(vec_stats_3, false, __LINE__);
}

}  // namespace cnstream
