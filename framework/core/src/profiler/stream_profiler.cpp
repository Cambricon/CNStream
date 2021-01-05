#include <string>
#include <utility>

#include "profiler/stream_profiler.hpp"

namespace cnstream {

StreamProfiler::StreamProfiler(const std::string& stream_name)
    : stream_name_(stream_name) {}

StreamProfile StreamProfiler::GetProfile() {
  StreamProfile profile;
  profile.stream_name = GetName();
  profile.completed = completed_;
  profile.dropped = dropped_;
  profile.counter = profile.completed + profile.dropped;
  double total_latency_ms = total_latency_.count();
  double total_phy_time_ms = total_phy_time_.count();
  profile.latency = -1;
  profile.fps = -1;
  if (total_phy_time_ms)
    profile.fps = 1e3 / total_phy_time_ms * profile.counter;
  if (latency_add_times_) {
    profile.latency = total_latency_ms / latency_add_times_;
    profile.maximum_latency = maximum_latency_.count();
    profile.minimum_latency = minimum_latency_.count();
  }
  return profile;
}

}  // namespace cnstream
