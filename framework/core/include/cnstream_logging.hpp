/*************************************************************************
* Copyright (C) 2019 by Cambricon, Inc. All rights reserved
*
* This source code is licensed under the Apache-2.0 license found in the
* LICENSE file in the root directory of this source tree.
*
* A part of this source code is referenced from glog project.
* https://github.com/google/glog/blob/master/src/logging.cc
*
* Copyright (c) 1999, Google Inc.
*
* This source code is licensed under the BSD 3-Clause license found in the
* LICENSE file in the root directory of this source tree.
*
*************************************************************************/

#ifndef CNSTREAM_CORE_LOGGING_HPP_
#define CNSTREAM_CORE_LOGGING_HPP_

#include <gflags/gflags.h>
#include <time.h>

#include <string>
#include <streambuf>
#include <ostream>

/**
 * @brief Log filter.
 *
 * Usage:
 * 1 ./app --log_filter=SOURCE:2,INFERENCE:3 ...
 * 2 export CNSTREAM_log_filter=SOURCE:2,INFERENCE:3 ...
 */
DECLARE_string(log_filter);

/**
 * @brief Min category log level, default LOG_INFO
 */
DECLARE_int32(min_log_level);

/**
 * @brief Flush log file time, in second, default 30s
 */
DECLARE_int32(flush_log_file_secs);

/**
 * @brief Log messages go to stderr, default true
 */
DECLARE_bool(log_to_stderr);

/**
 * @brief Log messages go to log file, default false
 */
DECLARE_bool(log_to_file);

#define STR(src) #src

#define LOGF(category) \
  cnstream::LogMessage(STR(category), __FILE__, __LINE__, cnstream::LogSeverity::LOG_FATAL).stream()

#define LOGF_IF(category, condition)                                                     \
  !(condition) ? (void) 0 : cnstream::LogMessageVoidify() & LOGF(category)

#define LOGE(category) \
  cnstream::LogMessage(STR(category), __FILE__, __LINE__, cnstream::LogSeverity::LOG_ERROR).stream()
#define LOGE_IF(category, condition)                                                     \
  !(condition) ? (void) 0 : cnstream::LogMessageVoidify() & LOGE(category)

#define LOGW(category) \
  cnstream::LogMessage(STR(category), __FILE__, __LINE__, cnstream::LogSeverity::LOG_WARNING).stream()

#define LOGW_IF(category, condition)                                                     \
  !(condition) ? (void) 0 : cnstream::LogMessageVoidify() & LOGW(category)

#define LOGI(category) cnstream::LogMessage(STR(category), __FILE__, __LINE__, cnstream::LogSeverity::LOG_INFO).stream()

#define LOGI_IF(category, condition)                                                     \
  !(condition) ? (void) 0 : cnstream::LogMessageVoidify() & LOGI(category)

#define LOGD(category) \
  cnstream::LogMessage(STR(category), __FILE__, __LINE__, cnstream::LogSeverity::LOG_DEBUG).stream()

#define LOGD_IF(category, condition)                                                     \
  !(condition) ? (void) 0 : cnstream::LogMessageVoidify() & LOGD(category)

#define LOGT(category) \
  cnstream::LogMessage(STR(category), __FILE__, __LINE__, cnstream::LogSeverity::LOG_TRACE).stream()

#define LOGT_IF(category, condition)                                                     \
  !(condition) ? (void) 0 : cnstream::LogMessageVoidify() & LOGT(category)

#define LOGA(category) cnstream::LogMessage(STR(category), __FILE__, __LINE__, cnstream::LogSeverity::LOG_ALL).stream()

#define LOGA_IF(category, condition)                                                     \
  !(condition) ? (void) 0 : cnstream::LogMessageVoidify() & LOGA(category)

namespace cnstream {

/**
 * @brief log severity
 * 0, FATAL
 * 1, ERROR
 * 2, WARNING
 * 3, INFO
 * 4, DEBUG
 * 5, TRACE
 * 6, ALL
 */
enum class LogSeverity { LOG_FATAL = 0, LOG_ERROR, LOG_WARNING, LOG_INFO, LOG_DEBUG, LOG_TRACE, LOG_ALL };

class LogSink {
 public:
  virtual ~LogSink() { }
  virtual void Send(LogSeverity severity, const char* category,
                    const char* filename, int line,
                    const struct ::tm* tm_time, int32_t usecs,
                    const char* message, size_t message_len) = 0;

  virtual void WaitTillSent() { }  // noop default
  static std::string ToString(LogSeverity severity, const char* category,
                              const char* filename, int line,
                              const struct ::tm* tm_time, int32_t usecs,
                              const char* message, size_t message_len);
};  // class LogSink

class LogMessageVoidify {
 public:
  LogMessageVoidify() { }
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&) { }
};

class LogMessage {
 public:
  class LogStreamBuf : public std::streambuf {
   public:
    // REQUIREMENTS: "len" must be >= 2 to account for the '\n' and '\0'.
    LogStreamBuf(char *buf, int len) {
      setp(buf, buf + len - 2);
    }

    // This effectively ignores overflow.
    virtual int_type overflow(int_type ch) {
      return ch;
    }

    // Legacy public ostrstream method.
    size_t pcount() const { return pptr() - pbase(); }
    char* pbase() const { return std::streambuf::pbase(); }
  };  // class LogStreamBuf

  class LogStream : public std::ostream {
   public:
    LogStream(char *buf, int len)
        : std::ostream(NULL),
          streambuf_(buf, len) {
      rdbuf(&streambuf_);
    }

    // Legacy std::streambuf methods.
    size_t pcount() const { return streambuf_.pcount(); }
    char* pbase() const { return streambuf_.pbase(); }
    char* str() const { return pbase(); }

   private:
    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;
    LogStreamBuf streambuf_;
  };  // class LogStream

  LogMessage(const char* category, const char* file, int line, LogSeverity severity);
  ~LogMessage();
  void Init(const char* category, const char* file, int line, LogSeverity severity);
  std::ostream& stream();
  struct LogMessageData;

 private:
  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;
  void Flush();
  void SendToLog();
  LogMessageData* data_;
  LogMessageData* allocated_;
  static const size_t MaxLogMsgLen;
};  // class LogMessage

void InitCNStreamLogging(const char* log_dir);

void AddLogSink(LogSink* log_sink);

void RemoveLogSink(LogSink* log_sink);

void ShutdownCNStreamLogging();

}  // namespace cnstream

#endif  // CNSTREAM_CORE_LOGGING_HPP_
