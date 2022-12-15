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

#include <fstream>
#include <string>
#include <utility>

#include "profiler/trace_serialize_helper.hpp"

namespace cnstream {

TEST(CoreTraceSerializeHelper, DeserializeFromJSONStr) {
  TraceSerializeHelper helper;
  const std::string jstr = "[{\"name\":\"process\",\"id\":0,\"cat\":\"stream0\",\"ts\":200}]";
  EXPECT_TRUE(TraceSerializeHelper::DeserializeFromJSONStr(jstr, &helper));
  EXPECT_EQ(helper.ToJsonStr(), jstr);
  const std::string wrong_jstr1 = "{\"name\":\"abc\"}";
  EXPECT_FALSE(TraceSerializeHelper::DeserializeFromJSONStr(wrong_jstr1, &helper));
  const std::string wrong_jstr2 = "{\"name\":\"abc\",}";
  EXPECT_FALSE(TraceSerializeHelper::DeserializeFromJSONStr(wrong_jstr2, &helper));
}

TEST(CoreTraceSerializeHelper, DeserializeFromJSONFile) {
  TraceSerializeHelper helper;
  const std::string test_filename = "_test_trace_serialize_helper_.json";
  std::ofstream ofs;
  const std::string jstr = "[{\"name\":\"process\",\"id\":0,\"cat\":\"stream0\",\"ts\":200}]";
  ofs.open(test_filename);
  if (!ofs.is_open()) return;
  ofs << jstr;
  ofs.close();
  EXPECT_TRUE(TraceSerializeHelper::DeserializeFromJSONFile(test_filename, &helper));
  EXPECT_EQ(helper.ToJsonStr(), jstr);
  const std::string wrong_jstr = "{\"name\":\"abc\"}";
  ofs.open(test_filename);
  ofs << wrong_jstr;
  ofs.close();
  EXPECT_FALSE(TraceSerializeHelper::DeserializeFromJSONFile(test_filename, &helper));
  remove(test_filename.c_str());
  EXPECT_FALSE(TraceSerializeHelper::DeserializeFromJSONFile(test_filename, &helper));
}

TEST(CoreTraceSerializeHelper, CopyConstructor) {
  TraceSerializeHelper helper;
  const std::string jstr = "[{\"name\":\"process\",\"id\":0,\"cat\":\"stream0\",\"ts\":200}]";
  EXPECT_TRUE(TraceSerializeHelper::DeserializeFromJSONStr(jstr, &helper));
  TraceSerializeHelper t(helper);
  EXPECT_EQ(t.ToJsonStr(), jstr);
  TraceSerializeHelper t1 = std::move(helper);
  EXPECT_EQ(t1.ToJsonStr(), jstr);
}

TEST(CoreTraceSerializeHelper, Serialize) {
  PipelineTrace trace;
  TraceElem elem;
  elem.key = std::make_pair("stream0", 0);
  elem.time = Clock::now();
  elem.type = TraceEvent::Type::START;
  trace.module_traces["module"]["process"].push_back(elem);
  trace.process_traces["overall"].push_back(elem);
  elem.type = TraceEvent::Type::END;
  trace.module_traces["module"]["process"].push_back(elem);
  trace.process_traces["overall"].push_back(elem);
  TraceSerializeHelper helper;
  helper.Serialize(trace);
  const std::string ret_jstr = helper.ToJsonStr();
  rapidjson::Document doc;
  ASSERT_FALSE(doc.Parse<rapidjson::kParseCommentsFlag>(ret_jstr.c_str()).HasParseError());
  ASSERT_TRUE(doc.IsArray());
  EXPECT_EQ(doc.GetArray().Size(), 4);
}

TEST(CoreTraceSerializeHelper, Merge) {
  TraceSerializeHelper t1;
  const std::string jstr = "[{\"name\":\"process\",\"id\":0,\"cat\":\"stream0\",\"ts\":200}]";
  EXPECT_TRUE(TraceSerializeHelper::DeserializeFromJSONStr(jstr, &t1));
  TraceSerializeHelper t2 = t1;
  t1.Merge(t2);
  const std::string ret_jstr = t1.ToJsonStr();

  rapidjson::Document doc;
  ASSERT_FALSE(doc.Parse<rapidjson::kParseCommentsFlag>(ret_jstr.c_str()).HasParseError());
  ASSERT_TRUE(doc.IsArray());
  EXPECT_EQ(doc.GetArray().Size(), 2);
}

TEST(CoreTraceSerializeHelper, ToJsonStr) {
  TraceSerializeHelper helper;
  const std::string jstr = "[{\"name\":\"process\",\"id\":0,\"cat\":\"stream0\",\"ts\":200}]";
  EXPECT_TRUE(TraceSerializeHelper::DeserializeFromJSONStr(jstr, &helper));
  EXPECT_EQ(helper.ToJsonStr(), jstr);
}

TEST(CoreTraceSerializeHelper, ToFile) {
  TraceSerializeHelper helper;
  const std::string test_filename = "_test_trace_serialize_helper_.json";
  const std::string jstr = "[{\"name\":\"process\",\"id\":0,\"cat\":\"stream0\",\"ts\":200}]";
  EXPECT_TRUE(TraceSerializeHelper::DeserializeFromJSONStr(jstr, &helper));
  EXPECT_TRUE(helper.ToFile(test_filename));
}

TEST(CoreTraceSerializeHelper, Reset) {
  TraceSerializeHelper helper;
  const std::string jstr = "[{\"name\":\"process\",\"id\":0,\"cat\":\"stream0\",\"ts\":200}]";
  EXPECT_TRUE(TraceSerializeHelper::DeserializeFromJSONStr(jstr, &helper));
  ASSERT_EQ(helper.ToJsonStr(), jstr);
  helper.Reset();
  EXPECT_EQ("[]", helper.ToJsonStr());
}

}  // namespace cnstream
