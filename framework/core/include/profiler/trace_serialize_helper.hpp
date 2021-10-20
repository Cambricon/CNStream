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

/*!
 *  @file trace_serialize_helper.hpp
 *
 *  This file contains a declaration of the TraceSerializeHelper class.
 */
namespace cnstream {

/*!
 * @class TraceSerializeHelper
 *
 * @brief Serializes trace data into JSON format. You can load JSON file by chrome-tracing to show the trace data.
 */
class TraceSerializeHelper {
 public:
  /*!
   * @brief Deserializes a JSON string.
   *
   * @param[in] jsonstr The JSON string.
   * @param[out] pout The output pointer stores the results.
   *
   * @return Returns true if the JSON string is deserialized successfully, otherwise returns false.
   */
  static bool DeserializeFromJSONStr(const std::string& jsonstr, TraceSerializeHelper* pout);
  /*!
   * @brief Deserializes a JSON file.
   *
   * @param[in] jsonstr The JSON file path.
   * @param[out] pout The output pointer stores the results.
   *
   * @return Returns true if the JSON string is deserialized successfully, otherwise returns false.
   */
  static bool DeserializeFromJSONFile(const std::string& filename, TraceSerializeHelper* pout);
  /*!
   * @brief Constructs a TraceSerializeHelper object.
   *
   * @return No return value.
   */
  TraceSerializeHelper();
  /*!
   * @brief Constructs a TraceSerializeHelper object with the copy of the contents of another object.
   *
   * @param[in] other Another object used to initialize an object.
   *
   * @return No return value.
   */
  TraceSerializeHelper(const TraceSerializeHelper& other);
  /*!
   * @brief Constructs a TraceSerializeHelper object with the contents of another object using move semantics.
   *
   * @param[in] other Another object used to initialize an object.
   *
   * @return No return value.
   */
  TraceSerializeHelper(TraceSerializeHelper&& other);
  /*!
   * @brief Replaces the contents with a copy of the contents of another TraceSerializeHelper object.
   *
   * @param[in] other Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceSerializeHelper& operator=(const TraceSerializeHelper& other);
  /*!
   * @brief Replaces the contents with those of another TraceSerializeHelper object using move semantics.
   *
   * @param[in] other Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceSerializeHelper& operator=(TraceSerializeHelper&& other);
  /*!
   * @brief Destructs a TraceSerializeHelper object by using default constructor.
   *
   * @return No return value.
   */
  ~TraceSerializeHelper() = default;
  /*!
   * @brief Serializes trace data.
   *
   * @param[in] pipeline_trace The trace data. Get it by ``pipeline.GetTracer()->GetTrace()``.
   *
   * @return No return value.
   */
  void Serialize(const PipelineTrace& pipeline_trace);

  /*!
   * @brief Merges another trace serialization helper tool data.
   *
   * @param[in] t The trace serialization helper tool to be merged.
   *
   * @return No return value.
   */
  void Merge(const TraceSerializeHelper& t);

  /*!
   * @brief Serializes to a JSON string.
   *
   * @return Returns a JSON string.
   */
  std::string ToJsonStr() const;

  /*!
   * @brief Serializes to a JSON file.
   *
   * @param[in] filename The JSON file name.
   *
   * @return Returns true if the serialization is successful, otherwise returns false.
   *
   * @note the possible reason of serialization failure is that writing to the file is not permitted.
   */
  bool ToFile(const std::string& filename) const;

  /*!
   * @brief Resets serialization helper. Clears data and frees up memory.
   *
   * @return No return value.
   */
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
