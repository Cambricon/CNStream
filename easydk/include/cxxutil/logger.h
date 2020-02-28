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

/**
 * @file logger.h
 *
 * This file contains a declaration of the Logger class and helper log macro
 */

#ifndef CXXUTIL_LOGGER_H_
#define CXXUTIL_LOGGER_H_

#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

#include "cxxutil/spinlock.h"

/**
 * @brief log information at specified level
 * @param level[in] specified log level
 * @param __VA_ARGS__[in] log message in C-style formated string
 * @see LogLevel
 */
#define LOG(level, ...)                                                                        \
  do {                                                                                         \
    edk::Logger::GetInstance()->Record(edk::LogLevel::level, __LINE__, __FILE__, __VA_ARGS__); \
  } while (0)

namespace edk {

/**
 * @brief log level enumeration
 */
enum class LogLevel {
  ERROR = 0,    ///< log errors, output to error log file or cerr
  WARNING = 1,  ///< log warnings, output to error log file or cerr
  INFO = 2,     ///< log informations, output to normal log file or cout
  TRACE = 3     ///< log trace informations for debug, output to normal log file or cout
};

class LogPrivate;

/**
 * @brief logger class to log information to different sink according to log level and settings
 */
class Logger {
 public:
  /**
   *  @brief set the pattern of logger
   *  @param to_screen[in] output to screen
   *  @param to_file[in] output to file
   */
  static void SetLogPattern(const bool &to_screen, const bool &to_file);

  /**
   *  @brief get the instance
   *  @return pointer to logger instance
   */
  static Logger *GetInstance();

  /**
   *  @brief write message to file
   *
   *  @param level[in] log level
   *  @param line[in] code line number
   *  @param filename[in] file name
   *  @param info[in] output message
   *  @param args[in] arguments to fill string
   */
  template <typename... Args>
  void Record(LogLevel level, const int line, const std::string &filename, const std::string &info, Args... args) {
    if (!to_file_ && !to_screen_) return;
    static int level_int;
    static size_t string_size;
    level_int = static_cast<int>(level);
    if (level_int <= level_) {
      SpinLockGuard lk(lock_);

      // generate log string
      std::string file = filename.substr(filename.find_last_of('/') + 1);
      std::string format_str = std::string(" %s:%d [%s] ") + info;
      string_size = snprintf(log_string_, 2048, format_str.c_str(), file.c_str(), line, log_level_str_[level_int], args...);
      if (string_size > 2048) std::cerr << "[WARNING] Logger: The excessive log beyond 2048 bytes will be cut off";

      WriteLog(level_int, log_string_);
    }
  }

 public:
  ~Logger();

 private:
  Logger();
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;
  void WriteLog(int level_int, const char *log);

 private:
  static char log_string_[2048];
  static const char *log_level_str_[4];
  static bool to_file_;
  static bool to_screen_;

  SpinLock lock_;
  int level_;
  LogPrivate *d_ptr_ = nullptr;
};  // class Logger

}  // namespace edk

#endif  // CXXUTIL_LOGGER_H_
