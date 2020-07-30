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

#include "perf_calculator.hpp"
#include "perf_manager.hpp"
#include "sqlite_db.hpp"
#include "util/cnstream_time_utility.hpp"

namespace cnstream {

extern std::string gTestPerfDir;
static const char kDbName[] = "test.db";
static std::vector<std::string> module_names = {"module_0", "module_1", "module_2", "module_3"};

static void Register(PerfManager* manager) {
  std::string start = PerfManager::GetStartTimeSuffix();
  std::string end = PerfManager::GetEndTimeSuffix();
  std::string table_name = PerfManager::GetDefaultType();
  std::string p_key = PerfManager::GetPrimaryKey();
  std::string thread_suffix = PerfManager::GetThreadSuffix();
  std::vector<std::string> keys = PerfManager::GetKeys(module_names, {start, end, thread_suffix});
  manager->RegisterPerfType(table_name, p_key, keys);
  EXPECT_TRUE(manager->perf_type_.find(table_name) != manager->perf_type_.end());
}

TEST(PerfManager, Stop) {
  PerfManager manager;
  manager.Stop();
  EXPECT_TRUE(manager.Init(gTestPerfDir + kDbName));
  EXPECT_TRUE(manager.running_);
  manager.Stop();
  EXPECT_FALSE(manager.running_);
}

TEST(PerfManager, Init) {
  PerfManager manager;
  EXPECT_TRUE(manager.Init(gTestPerfDir + kDbName));
  EXPECT_TRUE(manager.sql_ != nullptr);
  EXPECT_TRUE(manager.is_initialized_);
  EXPECT_TRUE(manager.perf_type_.find(manager.GetDefaultType()) == manager.perf_type_.end());

  Register(&manager);
  EXPECT_TRUE(manager.running_);
}

TEST(PerfManager, InitFailedCase) {
  PerfManager manager;
  EXPECT_FALSE(manager.Init(""));

  // can not init twice
  EXPECT_TRUE(manager.Init(gTestPerfDir + kDbName));
  EXPECT_FALSE(manager.Init(gTestPerfDir));

  // db file is holded by manager.
  PerfManager manager2;
  EXPECT_FALSE(manager.Init(gTestPerfDir + kDbName));

  manager.Stop();
}

TEST(PerfManager, Record) {
  PerfManager manager;
  EXPECT_TRUE(manager.Init(gTestPerfDir + kDbName));
  Register(&manager);

  std::string table_name = manager.GetDefaultType();

  for (uint32_t i = 0; i < module_names.size(); i++) {
    PerfManager::PerfInfo info{};
    EXPECT_TRUE(manager.Record(false, table_name, module_names[i], 0));
    EXPECT_TRUE(manager.Record(true, table_name, module_names[i], 0));
    EXPECT_TRUE(manager.Record(table_name, PerfManager::GetPrimaryKey(), "0",
                               module_names[i] + PerfManager::GetThreadSuffix(), "'th_0'"));
    EXPECT_TRUE(manager.Record(table_name, PerfManager::GetPrimaryKey(), "1",
                               module_names[i] + PerfManager::GetStartTimeSuffix()));
  }
  manager.Stop();

  for (uint32_t i = 0; i < module_names.size(); i++) {
    EXPECT_EQ(manager.sql_->Count(table_name, module_names[i] + PerfManager::GetStartTimeSuffix()), (unsigned)2);
    EXPECT_EQ(manager.sql_->Count(table_name, module_names[i] + PerfManager::GetEndTimeSuffix()), (unsigned)1);
    EXPECT_EQ(manager.sql_->Count(table_name, module_names[i] + PerfManager::GetThreadSuffix()), (unsigned)1);
  }
}

TEST(PerfManager, RecordFailedCase) {
  PerfManager manager;
  std::string table_name = manager.GetDefaultType();

  // Record before init
  EXPECT_FALSE(manager.Record(false, table_name, module_names[0], 0));
  EXPECT_TRUE(manager.Init(gTestPerfDir + kDbName));
  Register(&manager);

  for (uint32_t i = 0; i < module_names.size(); i++) {
    EXPECT_TRUE(manager.Record(false, table_name, module_names[i], 0));
    EXPECT_TRUE(manager.Record(true, table_name, module_names[i], 0));
  }
  manager.Stop();

  for (uint32_t i = 0; i < module_names.size(); i++) {
    EXPECT_EQ(manager.sql_->Count(table_name, module_names[i] + PerfManager::GetStartTimeSuffix()), (unsigned)1);
    EXPECT_EQ(manager.sql_->Count(table_name, module_names[i] + PerfManager::GetEndTimeSuffix()), (unsigned)1);
  }
  // Record after stop
  EXPECT_FALSE(manager.Record(true, table_name, module_names[0], 0));
}

void ThreadFunc(int i, std::vector<std::string> m_names, PerfManager* manager, int64_t num) {
  std::string table_name = PerfManager::GetDefaultType();
  for (int64_t pts = 0; pts < num; pts++) {
    EXPECT_TRUE(manager->Record(false, table_name, m_names[i % 4], pts));
  }
  for (int64_t pts = 0; pts < num; pts++) {
    EXPECT_TRUE(manager->Record(true, table_name, m_names[i % 4], pts));
  }
}

TEST(PerfManager, MultiThreadRecordInfo) {
  PerfManager manager;
  std::string table_name = manager.GetDefaultType();

  EXPECT_TRUE(manager.Init(gTestPerfDir + kDbName));
  Register(&manager);

  manager.SqlBeginTrans();

  std::vector<std::thread> ths;
  int64_t data_num = 100;
  for (int i = 0; i < 50; i++) {
    ths.push_back(std::thread(&ThreadFunc, i, module_names, &manager, data_num));
  }
  for (auto& it : ths) {
    if (it.joinable()) it.join();
  }
  manager.Stop();
  manager.SqlCommitTrans();

  for (uint32_t i = 0; i < module_names.size(); i++) {
    EXPECT_EQ(manager.sql_->Count(table_name, module_names[i] + PerfManager::GetStartTimeSuffix()), (unsigned)data_num);
    EXPECT_EQ(manager.sql_->Count(table_name, module_names[i] + PerfManager::GetEndTimeSuffix()), (unsigned)data_num);
  }
}

TEST(PerfManager, InsertInfoToDb) {
  PerfManager manager;
  std::string table_name = manager.GetDefaultType();

  EXPECT_TRUE(manager.Init(gTestPerfDir + kDbName));
  Register(&manager);
  EXPECT_TRUE(manager.sql_ != nullptr);

  int64_t pts = 0;
  PerfManager::PerfInfo info{table_name, PerfManager::GetPrimaryKey(), std::to_string(pts),
                             module_names[0] + PerfManager::GetStartTimeSuffix(), TimeStamp::CurrentToString()};
  EXPECT_NO_THROW(manager.InsertInfoToDb(info));

  EXPECT_EQ(manager.sql_->Count(table_name, PerfManager::GetPrimaryKey(),
                                PerfManager::GetPrimaryKey() + "=" + std::to_string(pts)),
            unsigned(1));
  EXPECT_EQ(manager.sql_->Count(table_name, module_names[0] + PerfManager::GetStartTimeSuffix(),
                                PerfManager::GetPrimaryKey() + "=" + std::to_string(pts)),
            unsigned(1));

  info.key = module_names[0] + PerfManager::GetEndTimeSuffix();
  EXPECT_NO_THROW(manager.InsertInfoToDb(info));

  EXPECT_EQ(manager.sql_->Count(table_name, module_names[0] + PerfManager::GetEndTimeSuffix(),
                                PerfManager::GetPrimaryKey() + "=" + std::to_string(pts)),
            unsigned(1));

  info.key = module_names[1] + PerfManager::GetStartTimeSuffix();
  EXPECT_NO_THROW(manager.InsertInfoToDb(info));

  EXPECT_EQ(manager.sql_->Count(table_name, module_names[1] + PerfManager::GetStartTimeSuffix(),
                                PerfManager::GetPrimaryKey() + "=" + std::to_string(pts)),
            unsigned(1));

  info.key = module_names[1] + PerfManager::GetEndTimeSuffix();
  EXPECT_NO_THROW(manager.InsertInfoToDb(info));
  EXPECT_EQ(manager.sql_->Count(table_name, module_names[1] + PerfManager::GetEndTimeSuffix(),
                                PerfManager::GetPrimaryKey() + "=" + std::to_string(pts)),
            unsigned(1));
}

TEST(PerfManager, InsertInfoToDbFailedCase) {
  PerfManager manager;
  std::string table_name = manager.GetDefaultType();

  EXPECT_TRUE(manager.Init(gTestPerfDir + kDbName));
  Register(&manager);

  EXPECT_TRUE(manager.sql_ != nullptr);

  PerfManager::PerfInfo info{"wrong_type", PerfManager::GetPrimaryKey(), "0",
                             module_names[0] + PerfManager::GetStartTimeSuffix(), TimeStamp::CurrentToString()};
  EXPECT_NO_THROW(manager.InsertInfoToDb(info));
  EXPECT_EQ(manager.sql_->Count(table_name, PerfManager::GetPrimaryKey(), PerfManager::GetPrimaryKey() + "=0"),
            unsigned(0));
}

TEST(PerfManager, RegisterPerfType) {
  PerfManager manager;
  std::string type1 = "type1";
  std::string type2 = "type2";
  EXPECT_TRUE(manager.Init(gTestPerfDir + kDbName));
  std::vector<std::string> keys =
      PerfManager::GetKeys(module_names, {PerfManager::GetStartTimeSuffix(), PerfManager::GetEndTimeSuffix()});
  EXPECT_TRUE(manager.sql_ != nullptr);
  EXPECT_TRUE(manager.RegisterPerfType(type1, PerfManager::GetPrimaryKey(), keys));
  EXPECT_TRUE(manager.RegisterPerfType(type2, PerfManager::GetPrimaryKey(), keys));

  PerfManager::PerfInfo info{type1, PerfManager::GetPrimaryKey(), "0",
                             module_names[0] + PerfManager::GetStartTimeSuffix(), TimeStamp::CurrentToString()};
  EXPECT_NO_THROW(manager.InsertInfoToDb(info));
  EXPECT_EQ(manager.sql_->Count(type1, PerfManager::GetPrimaryKey(), PerfManager::GetPrimaryKey() + "=0"), unsigned(1));

  info.perf_type = type2;
  EXPECT_NO_THROW(manager.InsertInfoToDb(info));
  EXPECT_EQ(manager.sql_->Count(type2, PerfManager::GetPrimaryKey(), PerfManager::GetPrimaryKey() + "=0"), unsigned(1));
}

TEST(PerfManager, RegisterPerfTypeFailedCase) {
  PerfManager manager;
  std::string table_name = manager.GetDefaultType();
  // register before init
  EXPECT_FALSE(manager.RegisterPerfType(table_name, PerfManager::GetPrimaryKey(), module_names));

  EXPECT_TRUE(manager.Init(gTestPerfDir + kDbName));
  // perf type should not be empty
  EXPECT_FALSE(manager.RegisterPerfType("", PerfManager::GetPrimaryKey(), module_names));

  // perf type is registered
  EXPECT_TRUE(manager.RegisterPerfType(table_name, PerfManager::GetPrimaryKey(), module_names));
  EXPECT_FALSE(manager.RegisterPerfType(table_name, PerfManager::GetPrimaryKey(), module_names));
}

TEST(PerfManager, GetKeys) {
  std::vector<std::string> suffix = {"1", "2"};
  std::vector<std::string> keys = PerfManager::GetKeys(module_names, suffix);
  EXPECT_EQ(keys.size(), module_names.size() * 2);
  for (uint32_t i = 0; i < module_names.size(); i++) {
    EXPECT_TRUE(keys[i] == module_names[i / 2] + suffix[i % 2]);
  }
}

TEST(PerfManager, SqlBeginAndCommit) {
  size_t start, end;
  size_t duration1, duration2;
  {
    PerfManager manager;
    std::string table_name = manager.GetDefaultType();

    EXPECT_TRUE(manager.Init(gTestPerfDir + kDbName));
    Register(&manager);

    start = TimeStamp::Current();
    manager.SqlBeginTrans();
    for (int64_t i = 0; i < 10000; i++) {
      EXPECT_TRUE(manager.Record(false, table_name, module_names[0], i));
    }
    manager.Stop();
    manager.SqlCommitTrans();
    end = TimeStamp::Current();
    duration1 = end - start;
  }
  {
    PerfManager manager;
    std::string table_name = manager.GetDefaultType();
    EXPECT_TRUE(manager.Init(gTestPerfDir + kDbName));
    Register(&manager);

    start = TimeStamp::Current();
    for (int64_t i = 0; i < 10000; i++) {
      EXPECT_TRUE(manager.Record(false, table_name, module_names[0], i));
    }
    manager.Stop();
    end = TimeStamp::Current();
    duration2 = end - start;
  }

  EXPECT_GT(duration2, duration1);
}

TEST(PerfManager, PrepareDbFileDir) {
  std::string outer_path = gTestPerfDir + "test_a/";
  std::string path = gTestPerfDir + "test_a/test_b/";
  std::string db_name = "test.db";
  std::string db_path = path + db_name;
  {
    PerfManager manager;
    remove(db_path.c_str());
    rmdir(path.c_str());
    rmdir(outer_path.c_str());
    // path not exist, create path
    EXPECT_TRUE(manager.PrepareDbFileDir(db_path));
    EXPECT_EQ(access(path.c_str(), 0), 0);
    EXPECT_TRUE(manager.Init(db_path));
    EXPECT_EQ(access(db_path.c_str(), 0), 0);
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
  std::string db_path = gTestPerfDir + kDbName;
  remove(db_path.c_str());
  EXPECT_FALSE(manager.PrepareDbFileDir(""));
  manager.sql_ = std::make_shared<Sqlite>(db_path);
  auto sql = manager.sql_;
  EXPECT_TRUE(manager.PrepareDbFileDir(db_path));
  sql->Connect();
  EXPECT_FALSE(manager.PrepareDbFileDir(db_path));
  sql->Close();
  EXPECT_TRUE(manager.PrepareDbFileDir(db_path));
}

}  // namespace cnstream
