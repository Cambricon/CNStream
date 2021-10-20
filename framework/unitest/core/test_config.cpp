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
#include "test_base.hpp"

namespace cnstream {

TEST(CoreConfig, ParseByJSONFile) {
  class TestConfig : public CNConfigBase {
   public:
    bool ParseByJSONStr(const std::string& jstr) {return true;}
  } test_config;
  auto config_file = CreateTempFile("test_config");
  EXPECT_TRUE(test_config.ParseByJSONFile(config_file.second));
  EXPECT_TRUE(test_config.config_root_dir.empty());
  // wrong path
  EXPECT_FALSE(test_config.ParseByJSONFile("wrong_file_path"));
  unlink(config_file.second.c_str());
  close(config_file.first);
}

TEST(CoreConfig, ProfilerConfig) {
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

TEST(CoreConfig, CNModuleConfig) {
  CNModuleConfig config;
  // case1: wrong json format
  std::string jstr = "{\"parallelism\" : 1,}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case2: no class_name
  jstr = "{\"parallelism\" : 1}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case3: class_name with wrong format
  jstr = "{\"class_name\" : 3}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case4: parallelism with wrong fromat
  jstr = "{\"class_name\" : \"test_class_name\","
      "\"parallelism\" : \"wrong_format\"}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case5: max input queue size with wrong fromat
  jstr = "{\"class_name\" : \"test_class_name\","
      "\"max_input_queue_size\" : \"wrong_format\"}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case6: next modules not an array type
  jstr = "{\"class_name\" : \"test_class_name\","
      "\"next_modules\" : \"wrong_format\"}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case7: next modules not a string array
  jstr = "{\"class_name\" : \"test_class_name\","
      "\"next_modules\" : [1, \"test_next_module\"]}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case8: custom_params not an object type
  jstr = "{\"class_name\" : \"test_class_name\","
      "\"custom_params\" : \"wrong_type\"}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case9: success
  jstr = "{\"class_name\" : \"test_class_name\","
      "\"parallelism\" : 15,"
      "\"max_input_queue_size\" : 30,"
      "\"next_modules\" : [\"next_module1\", \"next_module2\"],"
      "\"custom_params\" : {\"param1\" : 20, \"param2\" : \"param2_value\"}"
      "}";
  config.config_root_dir = "test_root_dir";
  EXPECT_TRUE(config.ParseByJSONStr(jstr));
  EXPECT_EQ(config.className, "test_class_name");
  EXPECT_EQ(config.parallelism, 15);
  EXPECT_EQ(config.maxInputQueueSize, 30);
  EXPECT_EQ(config.next.size(), 2);
  EXPECT_NE(config.next.find("next_module1"), config.next.end());
  EXPECT_NE(config.next.find("next_module2"), config.next.end());
  EXPECT_EQ(config.parameters.size(), 3);
  EXPECT_EQ(config.parameters["param1"], "20");
  EXPECT_EQ(config.parameters["param2"], "param2_value");
  EXPECT_EQ(config.config_root_dir, config.parameters[CNS_JSON_DIR_PARAM_NAME]);
}

TEST(CoreConfig, CNSubgraphConfig) {
  CNSubgraphConfig config;
  // case1: wrong json format
  std::string jstr = "{,}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case2: no config path
  jstr = "{}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case3: config path with wrong format
  jstr = "{\"config_path\": 123}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case4: next modules not an array type
  jstr = "{\"config_path\": \"test_config_path\","
      "\"next_modules\" : \"wrong_format\"}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case5: next modules not a string array
  jstr = "{\"config_path\": \"test_config_path\","
      "\"next_modules\" : [1, \"test_next_module\"]}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case6: success
  jstr = "{\"config_path\": \"test_config_path\","
      "\"next_modules\" : [\"next_module1\", \"next_module2\"]"
      "}";
  config.config_root_dir = "test_root_dir/";
  EXPECT_TRUE(config.ParseByJSONStr(jstr));
  EXPECT_EQ("test_root_dir/test_config_path", config.config_path);
  EXPECT_EQ("test_root_dir/test_config_path", config.config_path);
  EXPECT_EQ(config.next.size(), 2);
  EXPECT_NE(config.next.find("next_module1"), config.next.end());
  EXPECT_NE(config.next.find("next_module2"), config.next.end());
}

TEST(CoreConfig, CNGraphConfig) {
  CNGraphConfig config;
  // case1: wrong json format
  std::string jstr = "{,}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case2: wrong profiler config failed
  jstr = "{\"profiler_config\" : { \"enable_profiling\": \"ds\", \"enable_tracing\": true,"
      " \"trace_event_capacity\": 1}}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case3: wrong subgraph config
  jstr = "{\"subgraph:test_subgraph\" : {}}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case4: wrong module config
  jstr = "{\"test_module\" : {}}";
  EXPECT_FALSE(config.ParseByJSONStr(jstr));
  // case5: success
  jstr = "{"
      "\"profiler_config\" : {"
        "\"enable_profiling\" : true,"
        "\"enable_tracing\" : true"
      "},"
      "\"node1\" : {"
        "\"class_name\" : \"test_class\","
        "\"parallelism\" : 2,"
        "\"max_input_queue_size\" : 15,"
        "\"next_modules\" : [\"subgraph:node2\"]"
      "},"
      "\"subgraph:node2\" : {"
        "\"config_path\" : \"test_config_path\""
      "}"
    "}";
  EXPECT_TRUE(config.ParseByJSONStr(jstr));
  EXPECT_EQ(1, config.module_configs.size());
  EXPECT_EQ(1, config.subgraph_configs.size());
  EXPECT_TRUE(config.profiler_config.enable_profiling);
  EXPECT_TRUE(config.profiler_config.enable_tracing);
}

}  // namespace cnstream
