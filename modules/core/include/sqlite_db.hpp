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

#ifndef MODULES_CORE_INCLUDE_SQLITE_DB_HPP_
#define MODULES_CORE_INCLUDE_SQLITE_DB_HPP_

#include <string>
#include <vector>

#ifdef HAVE_SQLITE
#include "sqlite3.h"
#endif

namespace cnstream {

class Sqlite {
 public:
  explicit Sqlite(std::string name) {
    db_name_ = name;
  }
  bool Connect();
  bool Close();
  bool Execution(std::string sql_statement);
  bool CreateTable(std::string table_name, std::string primary_key, std::vector<std::string> key_names);
  bool Insert(std::string table_name, std::string key_names, std::string values);
  bool Update(std::string table_name, std::string condition_key, std::string condition_value,
              std::string update_key, std::string update_value);
  bool Delete(std::string table_name, std::string key_names, std::string values);
  bool Select(std::string table_name, std::string key_name, std::string condition,
              int (*callback)(void*, int, char**, char**), void* data);

  size_t FindMin(std::string table_name, std::string key_name);
  size_t FindMax(std::string table_name, std::string key_name);
  size_t Count(std::string table_name, std::string key_name, std::string condition = "");

  void Begin();
  void Commit();
  bool SetDbName(const std::string& db_name);
  std::string GetDbName();

 private:
#ifdef HAVE_SQLITE
  sqlite3 *db_ = nullptr;
#endif
  std::string db_name_;
};  // class Sqlite

}  // namespace cnstream

#endif  // MODULES_CORE_INCLUDE_SQLITE_DB_HPP_
