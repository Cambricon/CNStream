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

#include "sqlite_db.hpp"

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "glog/logging.h"

namespace cnstream {

#ifdef HAVE_SQLITE
bool Sqlite::Connect() {
  if (sqlite3_open_v2(db_name_.c_str(), &db_, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX,
                      NULL) != SQLITE_OK) {
    connected_ = false;
    LOG(ERROR) << "Open " << db_name_ << " failed.";
    return false;
  }
  connected_ = true;
  if (!Execution("PRAGMA synchronous = OFF;")) {
    LOG(ERROR) << "Set PRAGMA synchronous to normal falied.";
    return false;
  }
  if (!Execution("PRAGMA cache_size = 8000;")) {
    LOG(ERROR) << "Set PRAGMA set cache size  to 8000 falied.";
    return false;
  }
  // LOG(INFO) << "Successfully connect to sqlite database (" << db_name_ << ")";
  return true;
}

bool Sqlite::Close() {
  if (sqlite3_close_v2(db_) == SQLITE_OK) {
    connected_ = false;
    db_ = nullptr;
    return true;
  }
  return false;
}

bool Sqlite::Execution(std::string sql) {
  if (!connected_) {
    LOG(ERROR) << "SQL is not connected.";
    return false;
  }
  char* err_msg;
  if (sqlite3_exec(db_, sql.c_str(), 0, 0, &err_msg) != SQLITE_OK) {
    LOG(ERROR) << "(" << db_name_ << ") execute statement falied.\nSQL STATEMENT:\n  " << sql
               << "\nError message: " << err_msg;
    return false;
  }
  return true;
}

bool Sqlite::CreateTable(std::string table_name, std::string primary_key, std::vector<std::string> key_names) {
  std::string sql;
  if (primary_key.empty()) {
    sql = "CREATE TABLE " + table_name + "(id integer PRIMARY KEY autoincrement,";
  } else {
    sql = "CREATE TABLE " + table_name + "(" + primary_key + " STRING PRIMARY KEY NOT NULL,";
  }
  for (auto it : key_names) {
    sql += it + " STRING,";
  }
  sql.pop_back();
  sql += " );";

  return Execution(sql);
}

bool Sqlite::Insert(std::string table_name, std::string key_names, std::string values) {
  std::string sql_statement = "INSERT INTO " + table_name + " (" + key_names + ") VALUES (" + values + "); ";
  return Execution(sql_statement.c_str());
}

bool Sqlite::Update(std::string table_name, std::string condition_key, std::string condition_value,
                    std::string update_key, std::string update_value) {
  std::string sql_statement = "UPDATE " + table_name + " set " + update_key + " = " + update_value + " where " +
                              condition_key + " = " + condition_value + "; ";
  return Execution(sql_statement.c_str());
}

bool Sqlite::Delete(std::string table_name, std::string key_name, std::string value) {
  std::string sql_statement = "DELETE FROM " + table_name + " WHERE " + key_name + " = " + value + "; ";
  return Execution(sql_statement.c_str());
}

bool Sqlite::Select(std::string table_name, std::string key_name, std::string condition,
                    int (*callback)(void*, int, char**, char**), void* data) {
  if (!connected_) {
    LOG(ERROR) << "SQL is not connected.";
    return false;
  }
  char* err_msg;
  std::string sql_statement = "SELECT " + key_name + " from " + table_name;
  if (condition != "") {
    sql_statement += " where " + condition;
  }
  sql_statement += ";";

  if (sqlite3_exec(db_, sql_statement.c_str(), callback, data, &err_msg) == SQLITE_OK) {
    return true;
  }
  LOG(ERROR) << "Select data from table (" << table_name << ") falied.\nSQL STATEMENT:\n  " << sql_statement
             << "\nError message: " << err_msg;
  return false;
}

bool Sqlite::Select(std::string condition, int (*callback)(void*, int, char**, char**), void* data) {
  if (!connected_) {
    LOG(ERROR) << "SQL is not connected.";
    return false;
  }
  char* err_msg;
  if (condition == "") {
    return false;
  }

  if (sqlite3_exec(db_, condition.c_str(), callback, data, &err_msg) == SQLITE_OK) {
    return true;
  }
  LOG(ERROR) << "Select data falied.\nSQL STATEMENT:\n  " << condition << "\nError message: " << err_msg;
  return false;
}

static int SingleValueCallback(void* data, int argc, char** argv, char** azColName) {
  if (argv[0]) {
    *reinterpret_cast<size_t*>(data) = atoll(argv[0]);
  }
  return 0;
}

size_t Sqlite::FindMin(std::string table_name, std::string key_name, std::string condition) {
  size_t min = ~((size_t)0);
  std::string sql_statement = "SELECT MIN(" + key_name + ") from " + table_name;
  if (!condition.empty()) {
    sql_statement += " where " + condition + "; ";
  }
  sqlite3_exec(db_, sql_statement.c_str(), SingleValueCallback, reinterpret_cast<void*>(&min), 0);
  return min;
}

size_t Sqlite::FindMax(std::string table_name, std::string key_name, std::string condition) {
  size_t max = 0;
  std::string sql_statement = "SELECT MAX(" + key_name + ") from " + table_name;
  if (!condition.empty()) {
    sql_statement += " where " + condition + "; ";
  }
  sqlite3_exec(db_, sql_statement.c_str(), SingleValueCallback, reinterpret_cast<void*>(&max), 0);
  return max;
}

size_t Sqlite::Count(std::string table_name, std::string key_name, std::string condition) {
  size_t count = 0;
  std::string sql_statement = "SELECT COUNT(" + key_name + ") from " + table_name;
  if (condition != "") {
    sql_statement += " where " + condition + "; ";
  }
  sqlite3_exec(db_, sql_statement.c_str(), SingleValueCallback, reinterpret_cast<void*>(&count), 0);
  return count;
}

void Sqlite::Begin() { sqlite3_exec(db_, "begin transaction", 0, 0, 0); }

void Sqlite::Commit() { sqlite3_exec(db_, "commit transaction", 0, 0, 0); }

bool Sqlite::SetDbName(const std::string& db_name) {
  if (db_ || db_name == "") {
    return false;
  }
  db_name_ = db_name;
  return true;
}

std::string Sqlite::GetDbName() { return db_name_; }

#else
bool Sqlite::Connect() { return true; }

bool Sqlite::Close() { return true; }

bool Sqlite::Execution(std::string sql) { return true; }

bool Sqlite::CreateTable(std::string table_name, std::string primary_key, std::vector<std::string> key_names) {
  return true;
}

bool Sqlite::Insert(std::string table_name, std::string key_names, std::string values) { return true; }

bool Sqlite::Update(std::string table_name, std::string condition_key, std::string condition_value,
                    std::string update_key, std::string update_value) {
  return true;
}

bool Sqlite::Delete(std::string table_name, std::string key_names, std::string values) { return true; }

bool Sqlite::Select(std::string table_name, std::string key_name, std::string condition,
                    int (*callback)(void*, int, char**, char**), void* data) {
  return true;
}

bool Sqlite::Select(std::string condition, int (*callback)(void*, int, char**, char**), void* data) { return true; }

size_t Sqlite::FindMin(std::string table_name, std::string key_name, std::string condition) { return 0; }

size_t Sqlite::FindMax(std::string table_name, std::string key_name, std::string condition) { return 0; }

size_t Sqlite::Count(std::string table_name, std::string key_name, std::string condition) { return 0; }

void Sqlite::Begin() {}

void Sqlite::Commit() {}

bool Sqlite::SetDbName(const std::string& db_name) { return true; }

std::string Sqlite::GetDbName() { return ""; }
#endif

}  // namespace cnstream
