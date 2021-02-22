#ifndef CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_TRACE_HPP_
#define CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_TRACE_HPP_

#include <chrono>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cnstream {

/**
 * Class Clock represents a monotonic clock.
 * It will be used to get time when logging events.
 **/
using Clock    = std::chrono::steady_clock;

/**
 * Class Duration represents the length of a period of time.
 **/
using Duration = std::chrono::duration<double, std::milli>;

/**
 * Time type.
 **/
using Time     = Clock::time_point;

/**
 * Unique identification of a frame in tracing and profiling.
 * Usually, first: stream_name(CNFrameInfo::stream_id), second: pts(CNFrameInfo::timestamp).
 **/
using RecordKey = std::pair<std::string, int64_t>;

/**
 * Class TraceEvent represents an trace event.
 **/
struct TraceEvent {
  RecordKey key;                 ///< Unique identification of a frame.
  std::string module_name;       ///< Module name.
  std::string process_name;      ///< Process name. A process can be a function call or a piece of code.
  Time time;                     ///< Event time.
  enum Level {
    PIPELINE = 0,                ///< Event in pipeline.
    MODULE                       ///< Event in module.
  } level = PIPELINE;            ///< Event level.
  enum Type {
    START = 1 << 0,              ///< Process start event.
    END = 1 << 1                 ///< Process end event.
  } type = START;                ///< Event type.

  /*
   * TraceEvent constructor.
   */
  TraceEvent() = default;
  /**
   * TraceEvent constructor.
   *
   * @param key Unique identification of a frame.
   */
  explicit TraceEvent(const RecordKey& key);
  /**
   * TraceEvent constructor.
   *
   * @param key Unique identification of a frame.
   */
  explicit TraceEvent(RecordKey&& key);
  /**
   * TraceEvent copy constructor.
   *
   * @param other which instance copy from.
   */
  TraceEvent(const TraceEvent& other) = default;
  /**
   * TraceEvent operator =.
   *
   * @param other Which instance copy from.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& operator=(const TraceEvent& other) = default;
  /**
   * TraceEvent move constructor.
   *
   * @param other which instance move from.
   */
  TraceEvent(TraceEvent&& other);
  /**
   * TraceEvent operator =.
   *
   * @param other Which instance move from.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& operator=(TraceEvent&& other);
  /**
   * Set unique identification of a frame.
   *
   * @param key Unique identification of a frame.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetKey(const RecordKey& key);
  /**
   * Set unique identification of a frame.
   *
   * @param key Unique identification of a frame.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetKey(RecordKey&& key);
  /**
   * Set module name.
   *
   * @param module_name Module name.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetModuleName(const std::string& module_name);
  /**
   * Set module name.
   *
   * @param module_name Module name.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetModuleName(std::string&& module_name);
  /**
   * Set process name.
   *
   * @param process_name Process name.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetProcessName(const std::string& process_name);
  /**
   * Set process name.
   *
   * @param process_name Process name.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetProcessName(std::string&& process_name);
  /**
   * Set time.
   *
   * @param time Time.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetTime(const Time& time);
  /**
   * Set time.
   *
   * @param time Time.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetTime(Time&& time);
  /**
   * Set event level.
   *
   * @param level event level.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetLevel(const Level& level);
  /**
   * Set event type.
   *
   * @param type event type.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceEvent& SetType(const Type& type);
};  // struct TraceEvent

struct TraceElem {
  RecordKey key;                ///< Unique identification of a frame.
  Time time;                    ///< Event time.
  TraceEvent::Type type;        ///< Event type. Process start or process end.

  /*
   * TraceElem constructor.
   */
  TraceElem() = default;
  /**
   * TraceElem copy constructor.
   *
   * @param other which instance copy from.
   */
  TraceElem(const TraceElem& other) = default;
  /**
   * TraceElem operator =.
   *
   * @param other Which instance copy from.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceElem& operator=(const TraceElem& other) = default;
  /**
   * TraceElem move constructor.
   *
   * @param other which instance move from.
   */
  TraceElem(TraceElem&& other);
  /**
   * TraceElem operator =.
   *
   * @param other Which instance move from.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  TraceElem& operator=(TraceElem&& other);
  /**
   * TraceElem constructor.
   *
   * @param event Trace event.
   */
  explicit TraceElem(const TraceEvent& event);
  /**
   * TraceElem constructor.
   *
   * @param event Trace event.
   */
  explicit TraceElem(TraceEvent&& event);
};  // struct TraceElem

/**
 * Type of trace data for a process.
 **/
using ProcessTrace = std::vector<TraceElem>;

/**
 * Type of trace data for a module.
 **/
using ModuleTrace = std::unordered_map<std::string, ProcessTrace>;

/**
 * Trace data for a pipeline.
 **/
struct PipelineTrace {
  std::unordered_map<std::string, ProcessTrace> process_traces;  ///> process traces
  std::unordered_map<std::string, ModuleTrace> module_traces;    ///> module traces
  /*
   * PipelineTrace constructor.
   */
  PipelineTrace() = default;
  /**
   * PipelineTrace copy constructor.
   *
   * @param other which instance copy from.
   */
  PipelineTrace(const PipelineTrace& other) = default;
  /**
   * PipelineTrace operator =.
   *
   * @param other Which instance copy from.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  PipelineTrace& operator=(const PipelineTrace& other) = default;
  /**
   * PipelineTrace move constructor.
   *
   * @param other which instance move from.
   */
  PipelineTrace(PipelineTrace&& other);
  /**
   * PipelineTrace operator =.
   *
   * @param other Which instance move from.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  PipelineTrace& operator=(PipelineTrace&& other);
};  // struct PipelineTrace

inline TraceEvent::TraceEvent(const RecordKey& key) : key(key) {}

inline TraceEvent::TraceEvent(RecordKey&& key) : key(std::forward<RecordKey>(key)) {}

inline TraceEvent::TraceEvent(TraceEvent&& other) {
  *this = std::forward<TraceEvent>(other);
}

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

inline TraceElem::TraceElem(TraceElem&& other) {
  *this = std::forward<TraceElem>(other);
}

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

inline PipelineTrace::PipelineTrace(PipelineTrace&& other) {
  *this = std::forward<PipelineTrace>(other);
}

inline PipelineTrace& PipelineTrace::operator=(PipelineTrace&& other) {
  process_traces = std::move(other.process_traces);
  module_traces = std::move(other.module_traces);
  return *this;
}

}  // namespace cnstream

#endif  // CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_TRACE_HPP_
