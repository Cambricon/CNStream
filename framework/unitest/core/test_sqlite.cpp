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

#include <iostream>
#include <string>
#include <vector>

#include "sqlite_db.hpp"
#include "test_base.hpp"

namespace cnstream {

std::string gTestPerfDir = GetExePath() + "../test_perf_tmp/";  // NOLINT
std::string gTestPerfFile = gTestPerfDir + "test.db";           // NOLINT

bool CreateDir(std::string path) { return access(path.c_str(), 0) == 0 || mkdir(path.c_str(), 00700) == 0; }

TEST(PerfSqlite, ConnectAndClose) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());
  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, ConnectAndCloseFailedCase) {
  std::string db_name = gTestPerfDir + "not_exist/test_db";
  remove(db_name.c_str());
  Sqlite sql(db_name);
  EXPECT_FALSE(sql.Connect());
  EXPECT_TRUE(sql.Close());
  remove(db_name.c_str());
}

TEST(PerfSqlite, SetGetDbName) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(gTestPerfFile == sql.GetDbName());

  std::string db_name = gTestPerfDir + "test1.db";
  EXPECT_TRUE(sql.SetDbName(db_name));
  EXPECT_TRUE(db_name == sql.GetDbName());
}

TEST(PerfSqlite, SetGetDbNameFailedCase) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  // cannot change database name, until the sqlite connection is closed
  std::string db_name = gTestPerfDir + "test2.db";
  EXPECT_FALSE(sql.SetDbName(db_name));
  EXPECT_TRUE(sql.Close());
  EXPECT_TRUE(sql.SetDbName(db_name));
  EXPECT_TRUE(db_name == sql.GetDbName());
  remove(db_name.c_str());

  // cannot set database name to null string
  db_name = "";
  EXPECT_FALSE(sql.SetDbName(db_name));
}

TEST(PerfSqlite, Execution) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string statement =
      "CREATE TABLE COMPANY("
      "ID INT PRIMARY KEY     NOT NULL,"
      "NAME           TEXT    NOT NULL,"
      "AGE            INT     NOT NULL,"
      "ADDRESS        CHAR(50),"
      "SALARY         REAL );";
  EXPECT_TRUE(sql.Execution(statement));

  statement =
      "INSERT INTO COMPANY (ID,NAME,AGE,ADDRESS,SALARY) "
      "VALUES (1, 'Paul', 32, 'California', 20000.00 ); "
      "INSERT INTO COMPANY (ID,NAME,AGE,ADDRESS,SALARY) "
      "VALUES (2, 'Allen', 25, 'Texas', 15000.00 ); "
      "INSERT INTO COMPANY (ID,NAME,AGE,ADDRESS,SALARY)"
      "VALUES (3, 'Teddy', 23, 'Norway', 20000.00 );"
      "INSERT INTO COMPANY (ID,NAME,AGE,ADDRESS,SALARY)"
      "VALUES (4, 'Mark', 25, 'Rich-Mond ', 65000.00 );";
  EXPECT_TRUE(sql.Execution(statement));

  statement = "this is a wrong sql statement";
  EXPECT_FALSE(sql.Execution(statement));

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, ExecutionFailedCase) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string statement = "this is a wrong sql statement";
  EXPECT_FALSE(sql.Execution(statement));

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, CreateTable) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable("my_table", primary_key, keys));

  keys.clear();
  EXPECT_TRUE(sql.CreateTable("my_table2", primary_key, keys));
  EXPECT_TRUE(sql.CreateTable("my_table3", "", keys));

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, CreateTableFailedCase) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  {
    remove(gTestPerfFile.c_str());
    Sqlite sql(gTestPerfFile);
    EXPECT_TRUE(sql.Connect());

    std::vector<std::string> keys = {"key1", "key2", "key3"};
    EXPECT_FALSE(sql.CreateTable("", "id", keys));

    // table is existed
    EXPECT_TRUE(sql.CreateTable("my_table", "id", keys));
    EXPECT_FALSE(sql.CreateTable("my_table", "id", keys));

    EXPECT_TRUE(sql.Close());
    remove(gTestPerfFile.c_str());
  }

  {
    // same key
    Sqlite sql(gTestPerfFile);
    EXPECT_TRUE(sql.Connect());

    std::vector<std::string> keys = {"key1", "key1"};
    EXPECT_FALSE(sql.CreateTable("my_table", "id", keys));
    EXPECT_TRUE(sql.Close());
    remove(gTestPerfFile.c_str());
  }
}

TEST(PerfSqlite, Insert) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "1, 1, 1, 1"));
  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2", "2, 5, 5"));
  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key2", "3, 10"));

  EXPECT_EQ(sql.Count(table_name, primary_key), unsigned(3));
  EXPECT_EQ(sql.Count(table_name, "key1"), unsigned(2));
  EXPECT_EQ(sql.Count(table_name, "key2"), unsigned(3));
  EXPECT_EQ(sql.Count(table_name, "key3"), unsigned(1));
  EXPECT_EQ(sql.FindMax(table_name, "key2"), unsigned(10));
  EXPECT_EQ(sql.FindMin(table_name, "key2"), unsigned(1));

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, InsertFailedCase) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "1, 1, 1, 1"));
  EXPECT_EQ(sql.Count(table_name, primary_key), unsigned(1));

  // primary key value must be uniqued and not null
  EXPECT_FALSE(sql.Insert(table_name, primary_key, "1"));
  EXPECT_FALSE(sql.Insert(table_name, "key1", "1"));
  EXPECT_EQ(sql.Count(table_name, primary_key), unsigned(1));

  EXPECT_TRUE(sql.Close());

  EXPECT_FALSE(sql.Insert(table_name, "key1", "1"));
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, Update) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "1, 1, 1, 1"));
  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "2, 2, 2, 2"));
  EXPECT_EQ(sql.Count(table_name, primary_key), unsigned(2));

  EXPECT_TRUE(sql.Update(table_name, primary_key, "1", "key1", "10"));
  EXPECT_EQ(sql.FindMax(table_name, "key1"), unsigned(10));
  EXPECT_TRUE(sql.Update(table_name, primary_key, "2", "key2", "20"));
  EXPECT_EQ(sql.FindMax(table_name, "key2"), unsigned(20));

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, UpdateFailedCase) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "1, 1, 1, 1"));
  EXPECT_FALSE(sql.Update("", primary_key, "1", "key1", "10"));
  EXPECT_FALSE(sql.Update("wrong_table", primary_key, "1", "key1", "10"));
  EXPECT_FALSE(sql.Update(table_name, "wrong_key", "1", "key1", "10"));
  EXPECT_FALSE(sql.Update(table_name, primary_key, "1", "wrong_key", "10"));

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, Delete) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "1, 1, 1, 1"));
  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "2, 2, 2, 2"));
  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "3, 3, 3, 3"));
  EXPECT_EQ(sql.Count(table_name, primary_key), unsigned(3));

  EXPECT_TRUE(sql.Delete(table_name, primary_key, "1"));
  EXPECT_EQ(sql.Count(table_name, primary_key), unsigned(2));

  EXPECT_TRUE(sql.Delete(table_name, "key1", "2"));
  EXPECT_EQ(sql.Count(table_name, "key1"), unsigned(1));

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, DeleteFailedCase) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "1, 1, 1, 1"));

  EXPECT_FALSE(sql.Delete("wrong_table", primary_key, "1"));
  EXPECT_FALSE(sql.Delete(table_name, "wrong_key", "1"));

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

static int Callback(void *data, int argc, char **argv, char **azColName) {
  int *cnt = reinterpret_cast<int *>(data);
  *cnt += 1;
  EXPECT_EQ(argc, 3);
  for (int i = 0; i < argc; i++) {
    EXPECT_EQ(atoi(argv[i]), *cnt);
  }
  return 0;
}

TEST(PerfSqlite, Select) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "1, 1, 1, 1"));
  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "2, 2, 2, 2"));
  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "3, 3, 3, 3"));
  EXPECT_EQ(sql.Count(table_name, primary_key), unsigned(3));

  int data = 0;
  EXPECT_TRUE(sql.Select(table_name, "key1,key2,key3", "", Callback, reinterpret_cast<void *>(&data)));
  EXPECT_EQ(data, 3);
  data = 0;
  EXPECT_TRUE(sql.Select("select key1,key2,key3 from " + table_name + ";", Callback, reinterpret_cast<void *>(&data)));
  EXPECT_EQ(data, 3);
  data = 0;
  EXPECT_TRUE(sql.Select(table_name, "key1,key2,key3", "key1=1 or key2=2", Callback, reinterpret_cast<void *>(&data)));
  EXPECT_EQ(data, 2);
  data = 0;
  EXPECT_TRUE(sql.Select("select key1,key2,key3 from " + table_name + " where key1=1 or key2=2;", Callback,
                         reinterpret_cast<void *>(&data)));
  EXPECT_EQ(data, 2);

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, SelectFailedCase) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "1, 1, 1, 1"));
  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "2, 2, 2, 2"));
  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "3, 3, 3, 3"));
  EXPECT_EQ(sql.Count(table_name, primary_key), unsigned(3));

  int data = 0;
  EXPECT_TRUE(sql.Select(table_name, "*", "", nullptr, reinterpret_cast<void *>(&data)));
  EXPECT_EQ(data, 0);

  EXPECT_FALSE(sql.Select(table_name, "", "", Callback, reinterpret_cast<void *>(&data)));
  EXPECT_FALSE(sql.Select("wrong_table", "*", "", Callback, reinterpret_cast<void *>(&data)));

  EXPECT_FALSE(sql.Select("select * from wrong_table;", Callback, reinterpret_cast<void *>(&data)));

  EXPECT_TRUE(sql.Close());
  EXPECT_FALSE(sql.Select(table_name, "*", "key1=1 or key2=2", Callback, reinterpret_cast<void *>(&data)));
  EXPECT_FALSE(sql.Select("select * from " + table_name + " where key1=1 or key2=2;", Callback,
                          reinterpret_cast<void *>(&data)));
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, FindMin) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "1, 10, 15, 3"));
  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "2, 1, 10, 15"));
  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "3, 15, 2, 10"));
  EXPECT_EQ(sql.Count(table_name, primary_key), unsigned(3));

  EXPECT_EQ(sql.FindMin(table_name, "key1"), unsigned(1));
  EXPECT_EQ(sql.FindMin(table_name, "key2"), unsigned(2));
  EXPECT_EQ(sql.FindMin(table_name, "key3"), unsigned(3));

  EXPECT_EQ(sql.FindMin(table_name, "key3", primary_key + ">1"), unsigned(10));

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, FindMinInvalid) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  EXPECT_EQ(sql.FindMin("wrong_table", "key1"), ~(size_t(0)));
  EXPECT_EQ(sql.FindMin(table_name, "wrong_key"), ~(size_t(0)));

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, FindMax) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "1, 10, 22, 1"));
  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "2, 1, 10, 15"));
  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "3, 21, 1, 23"));
  EXPECT_EQ(sql.Count(table_name, primary_key), unsigned(3));

  EXPECT_EQ(sql.FindMax(table_name, "key1"), unsigned(21));
  EXPECT_EQ(sql.FindMax(table_name, "key2"), unsigned(22));
  EXPECT_EQ(sql.FindMax(table_name, "key3"), unsigned(23));

  EXPECT_EQ(sql.FindMax(table_name, "key3", primary_key + "<3"), unsigned(15));

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, FindMaxInvalid) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  EXPECT_EQ(sql.FindMax("wrong_table", "key1"), unsigned(0));
  EXPECT_EQ(sql.FindMax(table_name, "wrong_key"), unsigned(0));

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, Count) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  uint32_t cnt = 1000;
  for (uint32_t i = 0; i < cnt; i++) {
    EXPECT_TRUE(sql.Insert(table_name, primary_key, std::to_string(i)));
  }
  EXPECT_EQ(sql.Count(table_name, primary_key), cnt);
  EXPECT_EQ(sql.Count(table_name, primary_key, primary_key + ">=300"), cnt - 300);
  EXPECT_EQ(sql.Count(table_name, primary_key, primary_key + ">=300 and " + primary_key + "<800"), cnt - 500);

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, CountInvalid) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  EXPECT_TRUE(sql.Insert(table_name, primary_key + ", key1, key2, key3", "1, 10, 22, 1"));
  EXPECT_EQ(sql.Count("wrong_table", primary_key), unsigned(0));
  EXPECT_EQ(sql.Count(table_name, "wrong_key"), unsigned(0));

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

TEST(PerfSqlite, BeginCommit) {
  EXPECT_TRUE(CreateDir(gTestPerfDir));
  remove(gTestPerfFile.c_str());
  Sqlite sql(gTestPerfFile);
  EXPECT_TRUE(sql.Connect());

  std::string table_name = "my_table";
  std::string primary_key = "id";
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  EXPECT_TRUE(sql.CreateTable(table_name, primary_key, keys));

  sql.Begin();
  uint32_t cnt = 1000;
  for (uint32_t i = 0; i < cnt; i++) {
    EXPECT_TRUE(sql.Insert(table_name, primary_key, std::to_string(i)));
  }
  sql.Commit();
  EXPECT_EQ(sql.Count(table_name, primary_key), cnt);

  EXPECT_TRUE(sql.Close());
  remove(gTestPerfFile.c_str());
}

}  // namespace cnstream
