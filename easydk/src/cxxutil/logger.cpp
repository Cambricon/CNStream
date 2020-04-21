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

#include <sys/time.h>
#include <iostream>
#include <sstream>
#include <string>

namespace edk {

bool Logger::to_file_ = false;
bool Logger::to_screen_ = true;

char Logger::log_string_[2048];

SpinLock Logger::lock_;

const char *Logger::log_level_str_[] = {"ERROR", "WARNING", "INFO", "TRACE"};

static constexpr uint32_t MAX_CACHE_CNT = 1024;

class LogPrivate {
 public:
  std::ofstream logfile_;
  std::ofstream error_logfile_;
  std::stringstream err_cache_;
  std::stringstream log_cache_;
  uint32_t err_cache_cnt_ = 0;
  uint32_t log_cache_cnt_ = 0;
  uint64_t cnt = 0;

  inline void Flush(int file) {
    if (file) {
      if (log_cache_cnt_) logfile_ << log_cache_.rdbuf();
    } else {
      if (err_cache_cnt_) error_logfile_ << err_cache_.rdbuf();
    }
  }
};

void Logger::Flush() {
  if (to_file_) {
    d_ptr_->Flush(0);
    d_ptr_->Flush(1);
    d_ptr_->logfile_.flush();
    d_ptr_->error_logfile_.flush();
  }
}

#define TIME_STRING_LENGTH 128

void Logger::WriteLog(int level, const char *log) {
  d_ptr_->cnt++;
  static char time_string[TIME_STRING_LENGTH];
  tm t;
  timeval curtime;
  gettimeofday(&curtime, NULL);
  localtime_r(&curtime.tv_sec, &t);
  snprintf(time_string, TIME_STRING_LENGTH, "%02d.%02d %02d:%02d:%02d.%06ld", t.tm_mon + 1, t.tm_mday, t.tm_hour,
           t.tm_min, t.tm_sec, curtime.tv_usec);
  static std::ostream *print_stream;
  if (level < 2) {
    // warning and error
    if (to_file_) {
      d_ptr_->err_cache_ << time_string << log << std::endl;
      if (d_ptr_->err_cache_cnt_++ > MAX_CACHE_CNT) {
        d_ptr_->Flush(0);
        d_ptr_->err_cache_cnt_ = 0;
      }
    }
    print_stream = &std::cerr;
  } else {
    // info and debug
    print_stream = &std::cout;
  }
  if (to_file_) {
    d_ptr_->log_cache_ << time_string << log << std::endl;
    if (d_ptr_->log_cache_cnt_++ > MAX_CACHE_CNT) {
      d_ptr_->Flush(1);
      d_ptr_->log_cache_cnt_ = 0;
    }
  }
  if (to_screen_) *print_stream << time_string << log << std::endl;
}

void Logger::SetLogPattern(const bool &to_screen, const bool &to_file) {
  SpinLockGuard lk(lock_);
  Logger::to_screen_ = to_screen;
  Logger::to_file_ = to_file;
}

Logger *Logger::GetInstance() {
  static Logger instance;
  return &instance;
}

Logger::Logger() {
  d_ptr_ = new LogPrivate;
  char *level = getenv("EDK_DEBUG");
  if (level == nullptr) {
    level_ = 1;
  } else {
    level_ = level[0] - '0';
    if (level_ < 0 || level_ > 3) {
      std::cerr << "EDK_DEBUG must have a value between 0 and 3" << std::endl;
      level_ = 1;
    }
  }
  time_t tm;
  time(&tm);
  char time_string[128];
  strftime(time_string, 128, "EDK_%Y-%m.%d-%T", localtime(&tm));
  std::string str_name(time_string);
  if (to_file_) {
    d_ptr_->logfile_.open((str_name + ".log").c_str());
    d_ptr_->error_logfile_.open((str_name + "_error.log").c_str());
  }
}

Logger::~Logger() {
  Flush();
  d_ptr_->logfile_.close();
  d_ptr_->error_logfile_.close();
  delete d_ptr_;
  d_ptr_ = nullptr;
}

}  // namespace edk
