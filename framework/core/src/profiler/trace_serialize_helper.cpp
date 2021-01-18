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

#include <string>
#include <utility>

#include "profiler/trace_serialize_helper.hpp"

namespace cnstream {

bool TraceSerializeHelper::DeserializeFromJSONStr(const std::string& jsonstr, TraceSerializeHelper* pout) {
  rapidjson::Document doc;
  if (doc.Parse<rapidjson::kParseCommentsFlag>(jsonstr.c_str()).HasParseError()) {
    LOGE(PROFILER) << "Parse trace data failed. Error code [" << std::to_string(doc.GetParseError()) << "]"
                   << " Offset [" << std::to_string(doc.GetErrorOffset()) << "]";
    return false;
  }

  if (!doc.IsArray()) return false;
  pout->doc_ = std::move(doc);
  return true;
}

bool TraceSerializeHelper::DeserializeFromJSONFile(const std::string& filename, TraceSerializeHelper* pout) {
  std::ifstream ifs(filename);

  if (!ifs.is_open()) {
    LOGE(CORE) << "File open failed :" << filename;
    return false;
  }

  std::string jstr((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  ifs.close();

  if (!TraceSerializeHelper::DeserializeFromJSONStr(jstr, pout)) {
    return false;
  }

  return true;
}

TraceSerializeHelper::TraceSerializeHelper() {
  doc_.SetArray();
}

TraceSerializeHelper::TraceSerializeHelper(const TraceSerializeHelper& t) {
  *this = t;
}

TraceSerializeHelper::TraceSerializeHelper(TraceSerializeHelper&& t) {
  *this = std::forward<TraceSerializeHelper>(t);
}

TraceSerializeHelper& TraceSerializeHelper::operator=(const TraceSerializeHelper& t) {
  doc_.CopyFrom(t.doc_, doc_.GetAllocator());
  return *this;
}

TraceSerializeHelper& TraceSerializeHelper::operator=(TraceSerializeHelper&& t) {
  doc_ = std::move(t.doc_);
  return *this;
}

static inline
uint64_t ToUs(const Time& time) {
  return time.time_since_epoch().count() / 1000;
}

static
rapidjson::Value GenerateValue(rapidjson::Document::AllocatorType* pallocator,
    const TraceElem& elem, const std::string& module_name, const std::string& process_name) {
  rapidjson::Value value;
  value.SetObject();
  rapidjson::Value t;
  t.SetString(process_name.c_str(), process_name.size(), *pallocator);
  value.AddMember("name", t, *pallocator);
  switch (elem.type) {
    case TraceEvent::Type::START:
      value.AddMember("ph", "b", *pallocator);
      break;
    case TraceEvent::Type::END:
      value.AddMember("ph", "e", *pallocator);
      rapidjson::Value args_value;
      args_value.SetObject();
      t.SetString(elem.key.first.c_str(), elem.key.first.size(), *pallocator);
      args_value.AddMember("stream_name", t, *pallocator);
      args_value.AddMember("timestamp", elem.key.second, *pallocator);
      value.AddMember("args", args_value, *pallocator);
      break;
  }
  value.AddMember("ts", ToUs(elem.time), *pallocator);
  t.SetString(module_name.c_str(), module_name.size(), *pallocator);
  value.AddMember("pid", t, *pallocator);
  std::string cat = elem.key.first + "_" + module_name + "_" + process_name;
  t.SetString(cat.c_str(), cat.size(), *pallocator);
  value.AddMember("cat", t, *pallocator);
  value.AddMember("id", elem.key.second, *pallocator);
  return value;
}

void TraceSerializeHelper::Serialize(const PipelineTrace& pipeline_trace) {
  auto& allocator = doc_.GetAllocator();
  // module trace
  for (const auto& module_iter : pipeline_trace.module_traces) {
    const std::string& module_name = module_iter.first;
    const ModuleTrace module_trace = module_iter.second;
    for (const auto& process_iter : module_trace) {
      const std::string& process_name = process_iter.first;
      const ProcessTrace& process_trace = process_iter.second;
      for (const TraceElem& elem : process_trace) {
        doc_.PushBack(GenerateValue(&allocator, elem, module_name, process_name), allocator);
      }
    }
  }

  // pipeline processes trace
  for (const auto& process_iter : pipeline_trace.process_traces) {
    const std::string& process_name = process_iter.first;
    const ProcessTrace& process_trace = process_iter.second;
    for (const TraceElem& elem : process_trace) {
      doc_.PushBack(GenerateValue(&allocator, elem, "pipeline", process_name), allocator);
    }
  }
}

void TraceSerializeHelper::Merge(const TraceSerializeHelper& t) {
  const auto& event_array = t.doc_.GetArray();
  for (auto value_iter = event_array.Begin(); value_iter != event_array.End(); ++value_iter) {
    rapidjson::Value value;
    value.CopyFrom(*value_iter, doc_.GetAllocator());
    doc_.PushBack(value, doc_.GetAllocator());
  }
}

std::string TraceSerializeHelper::ToJsonStr() const {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  doc_.Accept(writer);
  return buffer.GetString();
}

void TraceSerializeHelper::Reset() {
  *this = TraceSerializeHelper();
}

}  // namespace cnstream
