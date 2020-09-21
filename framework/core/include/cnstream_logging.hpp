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

#ifndef CNSTREAM_CORE_LOGGING_HPP_
#define CNSTREAM_CORE_LOGGING_HPP_

#include <glog/logging.h>
#include <gflags/gflags.h>

#include <string>

/**
 * @brief Log filter.
 *
 * Usage:
 * 1 ./app --log_filter=SOURCE:2,INFERENCE:3 ...
 * 2 export CNSTREAM_log_filter=SOURCE:2,INFERENCE:3 ...
 */
DECLARE_string(log_filter);

/**
 * @brief Min module log level.
 */
DECLARE_int32(minmloglevel);

#define MLOG_IS_ON(module, severity)            \
  ::cnstream::ModuleActivated(#module, severity)

#define GLOG_STREAM_FATAL()                                                                     \
  google::LogMessageFatal(__FILE__, google::LogMessage::kNoLogPrefix).stream()

#define GLOG_STREAM_ERROR()                                                                     \
  google::LogMessage(__FILE__, google::LogMessage::kNoLogPrefix, google::GLOG_ERROR).stream()

#define GLOG_STREAM_WARNING()                                                                   \
  google::LogMessage(__FILE__, google::LogMessage::kNoLogPrefix, google::GLOG_WARNING).stream()

#define GLOG_STREAM_INFO()                                                                      \
  google::LogMessage(__FILE__, google::LogMessage::kNoLogPrefix).stream()

#define STR(src) #src

#define COMPACT_LOG_FATAL(module)                                                             \
  !(MLOG_IS_ON(module, 1)) ? (void) 0 :                                                       \
  google::LogMessageVoidify() & GLOG_STREAM_FATAL() << cnstream::LogPrefix(STR(module), 1)

#define COMPACT_LOG_ERROR(module)                                                             \
  !(MLOG_IS_ON(module, 2)) ? (void) 0 :                                                       \
  google::LogMessageVoidify() & GLOG_STREAM_ERROR() << cnstream::LogPrefix(STR(module), 2)

#define COMPACT_LOG_WARNING(module)                                                           \
  !(MLOG_IS_ON(module, 3)) ? (void) 0 :                                                       \
  google::LogMessageVoidify() & GLOG_STREAM_WARNING() << cnstream::LogPrefix(STR(module), 3)

#define COMPACT_LOG_INFO(module)                                                              \
  !(MLOG_IS_ON(module, 4)) ? (void) 0 :                                                       \
  google::LogMessageVoidify() & GLOG_STREAM_INFO() << cnstream::LogPrefix(STR(module), 4)

#define COMPACT_LOG_DEBUG(module)                                                             \
  !(MLOG_IS_ON(module, 5)) ? (void) 0 :                                                       \
  google::LogMessageVoidify() & GLOG_STREAM_INFO() << cnstream::LogPrefix(STR(module), 5)

#define COMPACT_LOG_TRACE(module)                                                             \
  !(MLOG_IS_ON(module, 6)) ? (void) 0 :                                                       \
  google::LogMessageVoidify() & GLOG_STREAM_INFO() << cnstream::LogPrefix(STR(module), 6)

#define COMPACT_LOG_ALL(module)                                                               \
  !(MLOG_IS_ON(module, 7)) ? (void) 0 :                                                       \
  google::LogMessageVoidify() & GLOG_STREAM_INFO() << cnstream::LogPrefix(STR(module), 7)

/*
 * @brief Before using MLOG, you should define DEFAULT_MODULE_CATEGORY your own module.
 *       If DEFAULT_MODULE_CATEGORY is not defined, MLOG will print to the default module.
 *
 * Usage:
 * #define DEFAULT_MODULE_CATEGORY mymodule
 */

#define MLOG(severity) COMPACT_LOG_ ## severity(DEFAULT_MODULE_CATEGORY)

#define MLOG_IF(severity, condition)            \
  !(condition) ? (void) 0 : COMPACT_LOG_ ## severity(DEFAULT_MODULE_CATEGORY)


namespace cnstream {

class LogPrefix {
 public:
  LogPrefix(const char* module, int severity);
  friend std::ostream& operator<<(std::ostream& os, const LogPrefix& log);

 private:
  std::string module_;
  int severity_;
  struct ::tm tm_time_;
  int32_t usecs_;
};

/**
 * @brief log severity
 * 1, FATAL
 * 2, ERROR
 * 3, WARNING
 * 4, INFO
 * 5, DEBUG
 * 6, TRACE
 * 7, ALL
 */

using LogSeverity = google::LogSeverity;

class LogSink : public google::LogSink {
 public:
  virtual ~LogSink() = default;
  virtual void Send(const std::string& message_string) = 0;
  virtual void WaitTillSent() {}

 protected:
  void send(LogSeverity severity, const char* full_filename,
            const char* base_filename, int line,
            const struct ::tm* tm_time, const char* message,
            size_t message_len) override {
    Send(this->ToString(severity, base_filename, line, tm_time, message, message_len));
  }
};  // class LogSink

void InitCNStreamLogging(const char* user_program, const char* log_dir);

void AddLogSink(LogSink* log_sink);

void RemoveLogSink(LogSink* log_sink);

bool ModuleActivated(const char* module, int severity);

}  // namespace cnstream

#endif  // CNSTREAM_CORE_LOGGING_HPP_
