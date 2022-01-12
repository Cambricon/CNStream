/*************************************************************************
 * Copyright (C) [2020-2021] by Cambricon, Inc. All rights reserved
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

#ifndef CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_TRACE_HPP_
#define CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_TRACE_HPP_

#include <chrono>
#include <string>
#include <map>
#include <utility>
#include <vector>

/*!
 *  @file trace.hpp
 *
 *  This file contains declarations of the TraceEvent class, the TraceElem struct and the TraceElem struct.
 */
namespace cnstream {

/*!
 * Defines an alias for the std::chrono::steady_clock.
 */
using Clock = std::chrono::steady_clock;

/*!
 * Defines an alias for the std::chrono::duration<double, std::milli>.
 */
using Duration = std::chrono::duration<double, std::milli>;

/*!
 * Defines an alias for the std::chrono::steady_clock::timepoint.
 */
using Time = Clock::time_point;

/*!
 * Defines an alias for the std::pair<std::string, int64_t>. RecordKey now denotes a pair of the stream name
 * ``CNFrameInfo::stream_id`` and pts ``CNFrameInfo::timestamp``.
 */
using RecordKey = std::pair<std::string, int64_t>;

/*!
 * @class TraceEvent
 *
 * @brief TraceEvent is a class representing a trace event used by Profile.
 */
class TraceEvent {
 public:
  RecordKey key;             /*!< The unique identification of a frame. */
  std::string module_name;   /*!< The name of a module. */
  std::string process_name;  /*!< The name of a process. A process can be a function call or a piece of code. */
  Time time;                 /*!< The timestamp of an event. */
  /*!
   * @enum Level
   *
   * @brief Enumeration variables describing the level of an event. The default level is 0 (pipeline's event).
   */
  enum class Level {
    PIPELINE = 0,  /*!< A event of a pipeline. */
    MODULE         /*!< An event of a module. */
  } level = Level::PIPELINE;
  /*!
   * @enum Type
   *
   * @brief Enumeration variables describing the type of an event. The default type is 1 (START).
   */
  enum class Type {
    START = 1 << 0,  /*!< A process-start event. */
    END = 1 << 1     /*!< A process-end event. */
  } type = Type::START;

  /*!
   * @brief Constructs a TraceEvent object by using default constructor.
   *
   * @return No return value.
   */
  TraceEvent() = default;
  /*!
   * @brief Constructs a TraceEvent object with a RecordKey instance.
   *
   * @param[in] key The unique identification of a frame.
   *
   * @return No return value.
   */
  explicit TraceEvent(const RecordKey& key);
  /*!
   * @brief Constructs a TraceEvent object with a RecordKey using move semantics.
   *
   * @param[in] key The unique identification of a frame.
   *
   * @return No return value.
   */
  explicit TraceEvent(RecordKey&& key);
  /*!
   * @brief Constructs a TraceEvent object with the copy of the contents of another object.
   *
   * @param[in] other Another object used to initialize an object.
   *
   * @return No return value.
   */
  TraceEvent(const TraceEvent& other) = default;
  /*!
   * @brief Replaces the contents with a copy of the contents of another TraceEvent object.
   *
   * @param[in] other Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& operator=(const TraceEvent& other) = default;
  /*!
   * @brief Constructs a TraceEvent object with the contents of another object using move semantics.
   *
   * @param[in] other Another object used to initialize an object.
   *
   * @return No return value.
   */
  TraceEvent(TraceEvent&& other);
  /*!
   * @brief Replaces the contents with those of another TraceEvent object using move semantics.
   *
   * @param[in] other Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& operator=(TraceEvent&& other);
  /*!
   * @brief Sets a unique identification for a frame.
   *
   * @param[in] key The unique identification of a frame.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetKey(const RecordKey& key);
  /*!
   * @brief Sets a unique identification for a frame using move semantics.
   *
   * @param[in] key The unique identification of a frame.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetKey(RecordKey&& key);
  /*!
   * @brief Sets the name of a module.
   *
   * @param[in] module_name The name of a module.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetModuleName(const std::string& module_name);
  /*!
   * @brief Sets the name of a module using move semantics.
   *
   * @param[in] module_name The name of a module.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetModuleName(std::string&& module_name);
  /*!
   * @brief Sets the name of a process.
   *
   * @param[in] process_name The name of a process.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetProcessName(const std::string& process_name);
  /*!
   * @brief Sets the name of a process using move semantics.
   *
   * @param[in] process_name The name of a process.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetProcessName(std::string&& process_name);
  /*!
   * @brief Sets the timestamp of this event.
   *
   * @param[in] time The timestamp of the event.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetTime(const Time& time);
  /*!
   * @brief Sets the timestamp of this event using move semantics.
   *
   * @param[in] time The timestamp of the event.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetTime(Time&& time);
  /*!
   * @brief Sets the level of this event.
   *
   * @param[in] level the level of the event.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetLevel(const Level& level);
  /*!
   * @brief Sets the type of this event.
   *
   * @param[in] type The type of th event.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetType(const Type& type);
};  // class TraceEvent

/*!
 * @struct TraceElem
 *
 * @brief The TraceElem is a structure describing a trace element used by profilers.
 */
struct TraceElem {
  RecordKey key;          /*!< The unique identification of a frame. */
  Time time;              /*!< The timestamp of an event. */
  TraceEvent::Type type;  /*!< The type of an event. It could be START or END. */

  /*!
   * @brief Constructs a TraceElem object by using default constructor.
   *
   * @return No return value.
   */
  TraceElem() = default;
  /*!
   * @brief Constructs a TraceElem object with the copy of the contents of another object.
   *
   * @param[in] other Another object used to initialize an object.
   *
   * @return No return value.
   */
  TraceElem(const TraceElem& other) = default;
  /*!
   * @brief Replaces the contents with a copy of the contents of another TraceElem object.
   *
   * @param[in] other Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceElem& operator=(const TraceElem& other) = default;
  /*!
   * @brief Constructs a TraceElem object with the contents of another object using move semantics.
   *
   * @param[in] other Another object used to initialize an object.
   *
   * @return No return value.
   */
  TraceElem(TraceElem&& other);
  /*!
   * @brief Replaces the contents with those of another TraceElem object using move semantics.
   *
   * @param[in] other Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceElem& operator=(TraceElem&& other);
  /*!
   * @brief Constructs a TraceElem object with a trace event.
   *
   * @param[in] event A specific trace event instance.
   *
   * @return No return value.
   */
  explicit TraceElem(const TraceEvent& event);
  /*!
   * @brief Constructs a TraceElem object with a trace event using move semantics.
   *
   * @param[in] event A specific trace event instance.
   *
   * @return No return value.
   */
  explicit TraceElem(TraceEvent&& event);
};  // struct TraceElem

/*!
 * Defines an alias for the std::vector<TraceElem>. ProcessTrace now denotes a vector which contains trace elements for
 * a process.
 */
using ProcessTrace = std::vector<TraceElem>;


/*!
 * Defines an alias for the std::map<std::string, ProcessTrace>. ModuleTrace now denotes an unordered map
 * which contains the pairs of the process name and the ProcessTrace object for a module.
 */
using ModuleTrace = std::map<std::string, ProcessTrace>;

/*!
 * @struct PipelineTrace
 *
 * @brief The PipelineTrace is a structure describing the trace data of a pipeline.
 */
struct PipelineTrace {
  std::map<std::string, ProcessTrace> process_traces;  /*!< The trace data of processes. */
  std::map<std::string, ModuleTrace> module_traces;    /*!< The trace data of modules. */
  /*!
   * @brief Constructs a PipelineTrace object by using default constructor.
   *
   * @return No return value.
   */
  PipelineTrace() = default;
  /*!
   * @brief Constructs a PipelineTrace object with the copy of the contents of another object.
   *
   * @param[in] other Another object used to initialize an object.
   *
   * @return No return value.
   */
  PipelineTrace(const PipelineTrace& other) = default;
  /*!
   * @brief Replaces the contents with a copy of the contents of another PipelineTrace object.
   *
   * @param[in] other Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  PipelineTrace& operator=(const PipelineTrace& other) = default;
  /*!
   * @brief Constructs a PipelineTrace object with the contents of another object using move semantics.
   *
   * @param[in] other Another object used to initialize an object.
   *
   * @return No return value.
   */
  PipelineTrace(PipelineTrace&& other);
  /*!
   * @brief Replaces the contents with those of another PipelineTrace object using move semantics.
   *
   * @param[in] other Another object used to initialize the current object.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  PipelineTrace& operator=(PipelineTrace&& other);
};  // struct PipelineTrace

inline TraceEvent::TraceEvent(const RecordKey& key) : key(key) {}

inline TraceEvent::TraceEvent(RecordKey&& key) : key(std::forward<RecordKey>(key)) {}

inline TraceEvent::TraceEvent(TraceEvent&& other) { *this = std::forward<TraceEvent>(other); }

inline TraceEvent& TraceEvent::operator=(TraceEvent&& other) {
  key = std::move(other.key);
  module_name = std::move(other.module_name);
  process_name = std::move(other.process_name);
  time = std::move(other.time);
  level = other.level;
  type = other.type;
  return *this;
}

inline TraceEvent& TraceEvent::SetKey(const RecordKey& key) {
  this->key = key;
  return *this;
}

inline TraceEvent& TraceEvent::SetKey(RecordKey&& key) {
  this->key = std::forward<RecordKey>(key);
  return *this;
}

inline TraceEvent& TraceEvent::SetModuleName(const std::string& module_name) {
  this->module_name = module_name;
  return *this;
}

inline TraceEvent& TraceEvent::SetModuleName(std::string&& module_name) {
  this->module_name = std::forward<std::string>(module_name);
  return *this;
}

inline TraceEvent& TraceEvent::SetProcessName(const std::string& process_name) {
  this->process_name = process_name;
  return *this;
}

inline TraceEvent& TraceEvent::SetProcessName(std::string&& process_name) {
  this->process_name = std::forward<std::string>(process_name);
  return *this;
}

inline TraceEvent& TraceEvent::SetTime(const Time& time) {
  this->time = time;
  return *this;
}

inline TraceEvent& TraceEvent::SetTime(Time&& time) {
  this->time = std::forward<Time>(time);
  return *this;
}

inline TraceEvent& TraceEvent::SetLevel(const Level& level) {
  this->level = level;
  return *this;
}

inline TraceEvent& TraceEvent::SetType(const Type& type) {
  this->type = type;
  return *this;
}

inline TraceElem::TraceElem(TraceElem&& other) { *this = std::forward<TraceElem>(other); }

inline TraceElem& TraceElem::operator=(TraceElem&& other) {
  key = std::move(other.key);
  time = std::move(other.time);
  type = other.type;
  return *this;
}

inline TraceElem::TraceElem(const TraceEvent& event) {
  key = event.key;
  time = event.time;
  type = event.type;
}

inline TraceElem::TraceElem(TraceEvent&& event) {
  key = std::move(event.key);
  time = std::move(event.time);
  type = event.type;
}

inline PipelineTrace::PipelineTrace(PipelineTrace&& other) { *this = std::forward<PipelineTrace>(other); }

inline PipelineTrace& PipelineTrace::operator=(PipelineTrace&& other) {
  process_traces = std::move(other.process_traces);
  module_traces = std::move(other.module_traces);
  return *this;
}

}  // namespace cnstream

#endif  // CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_TRACE_HPP_
