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

#include <gtest/gtest.h>
#include <stdio.h>

#include <fstream>
#include <string>

#include "cnstream_config.hpp"

namespace cnstream {

TEST(CoreProfilerConfig, ParseByJSONStr) {
  ProfilerConfig config;
  std::string jstr = "{ \"enable_profiling\": true, \"enable_tracing\": true, \"trace_event_capacity\": 1}";
  std::string wrong_jstr0 = "{ \"enable_profiling\": true, \"enable_tracing\": true, \"trace_event_capacity\":";
  std::string wrong_jstr1 = "{ \"enable_profiling\": \"ds\", \"enable_tracing\": true, \"trace_event_capacity\": 1}";
  std::string wrong_jstr2 = "{ \"enable_profiling\": true, \"enable_tracing\": \"ss\", \"trace_event_capacity\": 1}";
  std::string wrong_jstr3 = "{ \"enable_profiling\": true, \"enable_tracing\": true, \"trace_event_capacity\": \"f\"}";
  std::string wrong_jstr4 = "{ \"enable_profiling\": true, \"abc\": true}";
  EXPECT_FALSE(config.ParseByJSONStr(wrong_jstr0));
  EXPECT_FALSE(config.ParseByJSONStr(wrong_jstr1));
  EXPECT_FALSE(config.ParseByJSONStr(wrong_jstr2));
  EXPECT_FALSE(config.ParseByJSONStr(wrong_jstr3));
  EXPECT_FALSE(config.ParseByJSONStr(wrong_jstr4));

  EXPECT_TRUE(config.ParseByJSONStr(jstr));
  EXPECT_TRUE(config.enable_profiling);
  EXPECT_TRUE(config.enable_tracing);
  EXPECT_EQ(1, config.trace_event_capacity);
}

TEST(CoreProfilerConfig, ParseByJSONFile) {
  ProfilerConfig config;
  static const std::string config_path = "profiler_config_test.json";
  remove(config_path.c_str());
  EXPECT_FALSE(config.ParseByJSONFile(config_path));
  std::string jstr = "{ \"enable_profiling\": true, \"enable_tracing\": true, \"trace_event_capacity\": 1}";
  std::string wrong_jstr1 = "{ \"enable_profiling\": \"ds\", \"enable_tracing\": true, \"trace_event_capacity\": 1}";
  std::ofstream ofs(config_path);
  ofs << wrong_jstr1;
  ofs.close();
  EXPECT_FALSE(config.ParseByJSONFile(config_path));

  ofs.open(config_path);
  ofs << jstr;
  ofs.close();
  EXPECT_TRUE(config.ParseByJSONFile(config_path));
  EXPECT_TRUE(config.enable_profiling);
  EXPECT_TRUE(config.enable_tracing);
  EXPECT_EQ(1, config.trace_event_capacity);

  remove(config_path.c_str());
}

}  // namespace cnstream
