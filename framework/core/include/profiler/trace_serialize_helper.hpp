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

#ifndef CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_TRACE_SERIALIZE_HELPER_HPP_
#define CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_TRACE_SERIALIZE_HELPER_HPP_

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <fstream>
#include <string>

#include "cnstream_common.hpp"
#include "cnstream_logging.hpp"
#include "trace.hpp"

namespace cnstream {

/**
 * Serialize trace data into json format.
 * You can load json file by chrome-tracing to show the trace data.
 */
class TraceSerializeHelper {
 public:
  /**
   * Deserialize from json string.
   * 
   * @param jsonstr Json string.
   * @param pout Output pointer.
   * 
   * @return True for deserialized successfully. False for deserialized failed.
   **/
  static bool DeserializeFromJSONStr(const std::string& jsonstr, TraceSerializeHelper* pout);

  /**
   * Deserialize from json file.
   * 
   * @param jsonstr Json file path.
   * @param pout Output pointer.
   * 
   * @return True for deserialized successfully. False for deserialized failed.
   **/
  static bool DeserializeFromJSONFile(const std::string& filename, TraceSerializeHelper* pout);
  /*
   * TraceSerializeHelper constructor.
   */
  TraceSerializeHelper();
  /**
   * TraceSerializeHelper copy constructor.
   *
   * @param t which instance copy from.
   */
  TraceSerializeHelper(const TraceSerializeHelper& t);
  /**
   * TraceSerializeHelper move constructor.
   *
   * @param t which instance move from.
   */
  TraceSerializeHelper(TraceSerializeHelper&& t);
  /**
   * TraceSerializeHelper operator =.
   *
   * @param t Which instance copy from.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceSerializeHelper& operator=(const TraceSerializeHelper& t);
  /**
   * TraceSerializeHelper operator =.
   *
   * @param t Which instance move from.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceSerializeHelper& operator=(TraceSerializeHelper&& t);
  /*
   * TraceSerializeHelper destructor.
   */
  ~TraceSerializeHelper() = default;

  /**
   * Serialize trace data.
   * 
   * @param pipeline_trace Trace data, you can get it by pipeline.GetTracer()->GetTrace().
   **/
  void Serialize(const PipelineTrace& pipeline_trace);

  /**
   * Merge a trace serialize helper tool's data.
   *
   * @param t the trace serialize helper tool to be merged.
   **/
  void Merge(const TraceSerializeHelper& t);

  /**
   * Serialize to json string.
   * 
   * @return Return a json string.
   **/
  std::string ToJsonStr() const;

  /**
   * Serialize to json file.
   * 
   * @param filename Json file name.
   * 
   * @return True for success, false for failed(The possible reason is that there is no write file permission).
   **/
  bool ToFile(const std::string& filename) const;

  /**
   * Reset serialize helper. Clear datas and free up memory.
   **/
  void Reset();

 private:
  rapidjson::Document doc_;
};  // class TraceSerializeHelper

inline
bool TraceSerializeHelper::ToFile(const std::string& filename) const {
  std::ofstream ofs(filename);
  if (!ofs.is_open()) {
    LOGE(PROFILER) << "Open or create file failed. filename: " << filename;
    return false;
  }

  ofs << ToJsonStr();
  ofs.close();
  return true;
}

}  // namespace cnstream

#endif  // CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_TRACE_SERIALIZE_HELPER_HPP_
