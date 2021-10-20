/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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

#include <gtest/gtest.h>

#include <string>
#include <thread>

#include "cnstream_logging.hpp"

using cnstream::InitCNStreamLogging;
using cnstream::ShutdownCNStreamLogging;

TEST(CoreLog, CreateLogFile) {
  FLAGS_log_to_file = true;
  InitCNStreamLogging("/tmp");
  LOGI(CoreLog) << "Create log file at current directory";
  ShutdownCNStreamLogging();

  InitCNStreamLogging("unexist_directory");
  LOGE(CoreLog) << "Create log file at non-exist directory";
  ShutdownCNStreamLogging();

  std::string longlog(1028, '=');
  LOGW(CoreLog) << "Test long log " << longlog;
}

// This test case sometimes results in hang.
//
// TEST(CoreLog, LogFatal) {
//   InitCNStreamLogging("./");
//   EXPECT_DEATH_IF_SUPPORTED(LOGF(CoreLog) << "Should abort", "");
//   ShutdownCNStreamLogging();
// }

TEST(CoreLog, LogSink) {
  class MyLogSink : public cnstream::LogSink {
    void Send(cnstream::LogSeverity severity, const char* category, const char* filename, int line,
              const struct ::tm* tm_time, int32_t usecs, const char* message, size_t message_len) override {
      EXPECT_EQ(3, static_cast<int>(severity));
      EXPECT_STREQ(category, "CoreLog");
      EXPECT_STREQ("test_logging.cpp", filename);
      EXPECT_STREQ(message, "This log should be transmitted by LogSink::Send\n");
      std::cout << "MyLogSink: " << ToString(severity, category, filename, line, tm_time, usecs, message, message_len)
                << std::endl;
    }

    void WaitTillSent() override { std::cout << "MyLogSink Done" << std::endl; }
  };

  MyLogSink mysink;
  MyLogSink mysink1;

  cnstream::AddLogSink(&mysink);
  cnstream::AddLogSink(&mysink1);
  LOGI(CoreLog) << "This log should be transmitted by LogSink::Send";
  cnstream::RemoveLogSink(&mysink);
  cnstream::RemoveLogSink(&mysink1);
}
