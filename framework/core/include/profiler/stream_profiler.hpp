#ifndef CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_STREAM_PROFILER_HPP_
#define CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_STREAM_PROFILER_HPP_

#include <algorithm>
#include <chrono>
#include <string>
#include "profiler/profile.hpp"

namespace cnstream {

/**
 * StreamProfiler is responsible for the performance statistics of a certain processing process of a stream.
 * It is used by ProcessProfiler.
 * 
 * @see ProcessProfiler.
 **/
class StreamProfiler {
  using Duration = std::chrono::duration<double, std::milli>;

 public:
  /**
   * StreamProfiler constructor.
   *
   * @param stream_name Stream name.
   */
  explicit StreamProfiler(const std::string& stream_name);

  /**
   * Accumulate latency data.
   *
   * @param latency Latency.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  StreamProfiler& AddLatency(const Duration& latency);

  /**
   * Update pyhsical time this stream used.
   *
   * @param time The pyhsical time this stream used.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  StreamProfiler& UpdatePhysicalTime(const Duration& time);

  /**
   * Accumulate drop frame count.
   *
   * @param dropped drop frame count.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  StreamProfiler& AddDropped(uint64_t dropped);

  /**
   * Accumulate completed frame count with 1.
   *
   * @return Returns a lvalue reference to the current instance.
   */
  StreamProfiler& AddCompleted();

  /**
   * Gets stream name.
   *
   * @return Returns stream name.
   */
  std::string GetName() const;

  /**
   * Gets statistical performance data for this stream.
   *
   * @return Returns statistical performance data for this stream.
   */
  StreamProfile GetProfile();

 private:
  std::string stream_name_ = "";
  uint64_t completed_ = 0;
  uint64_t latency_add_times_ = 0;
  uint64_t dropped_ = 0;
  Duration total_latency_ = Duration::zero();
  Duration maximum_latency_      = Duration::zero();
  Duration minimum_latency_      = Duration::max();
  Duration total_phy_time_ = Duration::zero();
};  // class StreamProfiler

inline StreamProfiler& StreamProfiler::AddLatency(const Duration& latency) {
  latency_add_times_++;
  total_latency_ += latency;
  maximum_latency_ = std::max(latency, maximum_latency_);
  minimum_latency_ = std::min(latency, minimum_latency_);
  return *this;
}

inline StreamProfiler& StreamProfiler::UpdatePhysicalTime(const Duration& time) {
  total_phy_time_ = time;
  return *this;
}

inline StreamProfiler& StreamProfiler::AddDropped(uint64_t dropped) {
  dropped_ += dropped;
  return *this;
}

inline StreamProfiler& StreamProfiler::AddCompleted() {
  completed_++;
  return *this;
}

inline std::string StreamProfiler::GetName() const {
  return stream_name_;
}

}  // namespace cnstream

#endif  // CNSTREAM_FRAMEWORK_CORE_INCLUDE_PROFILER_STREAM_PROFILER_HPP_
