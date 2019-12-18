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

#include "cxxutil/logger.h"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

namespace edk {

bool Logger::out_to_file_ = false;
bool Logger::out_to_screen_ = true;

char Logger::time_string_[128];
char Logger::log_string_[2048];

const char *Logger::log_level_str_[] = { "ERROR", "WARNING", "INFO", "TRACE" };

class LogPrivate {
 public:
  std::ofstream logfile_;
  std::ofstream error_logfile_;
};

void Logger::WriteLog(int level, const char *log) {
  static std::ostream *print_stream;
  if (level < 2) {
    // warning and error
    if (out_to_file_) d_ptr_->error_logfile_ << log_string_ << std::endl;
    print_stream = &std::cerr;
  } else {
    // info and debug
    print_stream = &std::cout;
  }
  if (out_to_file_) d_ptr_->logfile_ << log_string_ << std::endl;
  if (out_to_screen_) *print_stream << log_string_ << std::endl;
}

void Logger::SetLogPattern(const bool &to_screen, const bool &to_file) {
  Logger::out_to_screen_ = to_screen;
  Logger::out_to_file_ = to_file;
}

Logger *Logger::GetInstance() {
  static Logger instance;
  return &instance;
}

Logger::Logger() {
  d_ptr_ = new LogPrivate;
  char *level = getenv("TOOLKIT_DEBUG");
  if (level == nullptr) {
    level_ = 1;
  } else {
    level_ = level[0] - '0';
    if (level_ < 0 || level_ > 3) {
      std::cerr << "TOOLKIT_DEBUG must have a value between 0 and 3" << std::endl;
      level_ = 1;
    }
  }
  time_t tm;
  time(&tm);
  char time_string[128];
  strftime(time_string, 128, "Tookit_%Y-%m.%d-%T", localtime(&tm));
  std::string str_name(time_string);
  if (out_to_file_) {
    d_ptr_->logfile_.open((str_name + ".log").c_str());
    d_ptr_->error_logfile_.open((str_name + "_error.log").c_str());
  }
}

Logger::~Logger() {
  d_ptr_->logfile_.close();
  d_ptr_->error_logfile_.close();
  delete d_ptr_;
  d_ptr_ = nullptr;
}

}  // namespace edk
