/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#if defined(linux) || defined(__linux) || defined(__linux__)
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#endif

#include <string>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <utility>
#include <thread>

#include "cnstream_logging.hpp"

#define EnvToString(envname, dflt)   \
  (!getenv(envname) ? (dflt) : getenv(envname))

#define EnvToInt(envname, dflt)  \
  (!getenv(envname) ? (dflt) : strtol(getenv(envname), NULL, 10))

#define CNSTREAM_DEFINE_ENV_string(name, value, meaning) \
  DEFINE_string(name, EnvToString("CNSTREAM_" #name, value), meaning)

#define CNSTREAM_DEFINE_ENV_int32(name, value, meaning) \
  DEFINE_int32(name, EnvToInt("CNSTREAM_" #name, value), meaning)

CNSTREAM_DEFINE_ENV_string(log_filter, "", "log filter");

CNSTREAM_DEFINE_ENV_int32(minmloglevel, -1, "min module log level");

// Based on: https://github.com/google/glog/blob/master/src/utilities.cc
static pid_t GetTID() {
  // On Linux we try to use gettid().
#if defined(linux) || defined(__linux) || defined(__linux__)
#ifndef __NR_gettid
#if !defined __i386__
#error "Must define __NR_gettid for non-x86 platforms"
#else
#define __NR_gettid 224
#endif
#endif
  static bool lacks_gettid = false;
  if (!lacks_gettid) {
    pid_t tid = syscall(__NR_gettid);
    if (tid != -1) {
      return tid;
    }
    // Technically, this variable has to be volatile, but there is a small
    // performance penalty in accessing volatile variables and there should
    // not be any serious adverse effect if a thread does not immediately see
    // the value change to "true".
    lacks_gettid = true;
  }
#endif  // OS_LINUX

  // If gettid() could not be used, we use one of the following.
#if defined(linux) || defined(__linux) || defined(__linux__)
  return getpid();  // Linux:  getpid returns thread ID when gettid is absent
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
  return GetCurrentThreadId();
#else
  // If none of the techniques above worked, we use pthread_self().
  return (pid_t)(uintptr_t)pthread_self();
#endif
}

/**
 * @brief Remove all spaces in the string
 */
static std::string StringTrim(const std::string& str) {
  std::string::size_type index = 0;
  std::string result = str;

  while ((index = result.find(' ', index)) != std::string::npos) {
    result.erase(index, 1);
  }

  return result;
}

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
struct timeval {
  long tv_sec, tv_usec;  // NOLINT
};

// Based on: http://www.google.com/codesearch/p?hl=en#dR3YEbitojA/os_win32.c&q=GetSystemTimeAsFileTime%20license:bsd
// See COPYING for copyright information.
static int gettimeofday(struct timeval *tv, void* tz) {
#define EPOCHFILETIME (116444736000000000ULL)
  FILETIME ft;
  LARGE_INTEGER li;
  uint64 tt;

  GetSystemTimeAsFileTime(&ft);
  li.LowPart = ft.dwLowDateTime;
  li.HighPart = ft.dwHighDateTime;
  tt = (li.QuadPart - EPOCHFILETIME) / 10;
  tv->tv_sec = tt / 1000000;
  tv->tv_usec = tt % 1000000;

  return 0;
}
#endif

namespace cnstream {

const int NUM_SEVERITIES = 7;
const char* const LogSeverityNames[NUM_SEVERITIES] = {
  "FATAL", "ERROR", "WARNING", "INFO", "DEBUG", "TRACE", "ALL"
};

const char* finename_prefix = "CNSTREAM_";

LogPrefix::LogPrefix(const char* module, int severity) {
  module_ = std::string(module);
  severity_ = severity;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double now = (static_cast<int64_t>(tv.tv_sec) * 1000000 + tv.tv_usec) * 0.000001;
  time_t timestamp = static_cast<time_t>(now);
  localtime_r(&timestamp, &tm_time_);
  usecs_ = static_cast<int32_t>((now - timestamp) * 1000000);
}

std::ostream& operator<<(std::ostream& os, const LogPrefix& log) {
  os << "CNSTREAM " << log.module_
    << ' '
    << LogSeverityNames[log.severity_ - 1][0]
    // << std::setw(4) << 1900 + log.tm_time_.tm_year
    << std::setw(2) << 1 + log.tm_time_.tm_mon
    << std::setw(2) << log.tm_time_.tm_mday
    << ' '
    << std::setw(2) << log.tm_time_.tm_hour << ':'
    << std::setw(2) << log.tm_time_.tm_min << ':'
    << std::setw(2) << log.tm_time_.tm_sec << "."
    << std::setw(6) << log.usecs_
    << ' '
    << std::setfill(' ') << std::setw(5)
    << static_cast<unsigned int>(GetTID())
    << "] ";
  return os;
}

using ModuleFilterMaps = std::unordered_map<std::string, int>;

class Logger {
 public:
  /**
   * @brief Creates or gets the instance of the Logger class.
   * @brief Don't returning Logger pointers, avoid delete instance.
   * @brief It's ThreadSafe.
   *
   * @return Returns the reference instance of the Logger class, 
   */
  static Logger& Instance() {
    static Logger logger;
    return logger;
  }

  ~Logger() {
    if (init_glog_) {
      google::ShutdownGoogleLogging();
    }
  }

  // disable copy and assign
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  void Init(const char* user_program, const char* log_dir);

  bool inline ModuleActivated(const char* module, int severity) {
    if (filter_maps_) {
      auto it = filter_maps_->find(std::string(module));
      return it != filter_maps_->end() && it->second >= severity;
    }
    return min_log_level_ >= severity;
  }

  inline void AddLogSink(LogSink* log_sink) {
    if (log_sink) {
      google::AddLogSink(log_sink);
    }
  }

  inline void RemoveLogSink(LogSink* log_sink) {
    if (log_sink) {
      google::RemoveLogSink(log_sink);
    }
  }

 private:
  Logger() {}
  std::shared_ptr<ModuleFilterMaps> CreateFilterMaps();

  bool init_glog_ = false;
  std::shared_ptr<ModuleFilterMaps> filter_maps_ = nullptr;
  int min_log_level_ = 4;  // default log severity of INFO
};  // class Logger

void Logger::Init(const char* user_program, const char* log_dir) {
  google::InitGoogleLogging(user_program);

  // only available when logtostd not set
  if (log_dir && strlen(log_dir)) {
    google::SetLogDestination(google::GLOG_INFO, log_dir);
    google::SetLogFilenameExtension(finename_prefix);
  }

  FLAGS_colorlogtostderr = true;
  FLAGS_alsologtostderr = true;

  if (FLAGS_minmloglevel != -1) {
    min_log_level_ = FLAGS_minmloglevel;
  }

  // init log filter, create filter maps if log_filter is enable
  filter_maps_ = CreateFilterMaps();

  google::InstallFailureSignalHandler();
  init_glog_ = true;
}

std::shared_ptr<ModuleFilterMaps> Logger::CreateFilterMaps() {
  std::string filter_str = StringTrim(FLAGS_log_filter);
  if (filter_str.empty()) {
    return nullptr;
  }

  std::shared_ptr<ModuleFilterMaps> maps = std::make_shared<ModuleFilterMaps>();

  const char* module = filter_str.c_str();
  const char* sep;
  while ((sep = strchr(module, ':')) != NULL) {
    std::string pattern(module, sep - module);
    std::transform(pattern.begin(), pattern.end(), pattern.begin(),
        [](unsigned char c) { return std::toupper(c); });
    int module_level = min_log_level_;
    if (sscanf(sep, ":%d", &module_level) != 1) {
      LOG(WARNING) << "Parse " << pattern << " log level failed, will set to " << module_level;
    }
    maps->insert(std::make_pair(pattern, module_level));
    // Skip past this entry
    module = strchr(sep, ',');
    if (module == nullptr) break;
    module++;  // Skip past ","
  }
  return maps;
}

void InitCNStreamLogging(const char* user_program, const char* log_dir) {
  Logger::Instance().Init(user_program, log_dir);
}

void AddLogSink(LogSink* log_sink) {
  Logger::Instance().AddLogSink(log_sink);
}

void RemoveLogSink(LogSink* log_sink) {
  Logger::Instance().RemoveLogSink(log_sink);
}

bool ModuleActivated(const char* module, int severity) {
  return Logger::Instance().ModuleActivated(module, severity);
}

}  // namespace cnstream
