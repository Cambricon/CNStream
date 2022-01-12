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

#include <cassert>
#include <list>
#include <string>
#include <vector>
#include <map>
#include <utility>

#include "profiler/process_profiler.hpp"
#include "profiler/stream_profiler.hpp"
#include "profiler/trace.hpp"

namespace cnstream {

/**
 * RecordPolicy records start times of each stream
 * and counting the number of dropped frame depends on the MaxDpbSize(H.264/H.265)
 **/
class RecordPolicy {
 public:
  using StartRecords = std::list<std::pair<RecordKey, Time>>;
  using StartRecordIter = StartRecords::iterator;

  // The maximum |MaxDpbSize| between H.264 and H.265 is 16.
  static constexpr uint64_t kDEFAULT_MAX_DPB_SIZE = 16;

  // Is there alread records datas of stream named by |stream_name|.
  bool IsStreamExist(const std::string& stream_name) const;

  // Gets records of stream named by stream_name.
  StartRecords& GetRecords(const std::string& stream_name);

  // Find start record by |key|(stream name and timestamp).
  bool FindStartRecord(const RecordKey& key, StartRecordIter* start_record);

  // Sets |MaxDpbSize| for stream named by |stream_name|.
  void SetStreamMaxDpbSize(const std::string& stream_name, uint64_t max_dpb_size);

  // Gets |MaxDpbSize| of stream named by |stream_name|.
  uint64_t GetStreamMaxDpbSize(const std::string& stream_name);

  bool AddStartTime(const RecordKey& key, const Time& time);

  // Remove time record by record iterator.
  // Records recorded earilier than the specified record will increase their corresponding skip counters.
  // When the skip counter is greater than the |MaxDpbSize|, the corresponding record is considered useless
  // and removed.
  // Returns the number of removed records including record specified by |record|.
  // So the return value is at least 1.
  // |record| usually comes from FindStartRecord.
  uint64_t RemoveThisAndOtherUselessRecords(const std::string& stream_name, StartRecordIter* record);

  // This function must be called before records start time of stream named by |stream_name|.
  void OnStreamStart(const std::string& stream_name);

  // Clear records of stream named by |stream_name|
  // and returns the number of remaining records.
  void OnStreamEos(const std::string& stream_name, uint64_t* number_remaining);

 private:
  void RemoveStreamMaxDpbSize(const std::string& stream_name);

 private:
  // Map of start time records for each stream.
  std::map<std::string, StartRecords> start_records_;
  // Map of skip reference counter, key is stream name, value is a list of skip reference counter.
  std::map<std::string, std::list<uint64_t>> skip_refs_;
  // MaxDpbSize of stream, key is stream name, value is MaxDpbSize.
  std::map<std::string, uint64_t> stream_max_dpb_sizes_;
  std::mutex max_dpb_size_lk_;
};  // class RecordPolicy


inline bool RecordPolicy::IsStreamExist(const std::string& stream_name) const {
  return start_records_.find(stream_name) != start_records_.end();
}

inline RecordPolicy::StartRecords& RecordPolicy::GetRecords(const std::string& stream_name) {
  assert(start_records_.find(stream_name) != start_records_.end());
  return start_records_[stream_name];
}

bool RecordPolicy::FindStartRecord(const RecordKey& key, StartRecordIter* start_record) {
  if (!IsStreamExist(key.first)) return false;
  auto& records = GetRecords(key.first);
  for (auto it = records.begin(); it != records.end(); ++it) {
    if (it->first == key) {
      *start_record = it;
      return true;
    }
  }
  return false;
}

bool RecordPolicy::AddStartTime(const RecordKey& key, const Time& time) {
  const std::string& stream_name = key.first;
  if (!IsStreamExist(stream_name)) return false;
  StartRecords& records = GetRecords(stream_name);
  records.emplace_back(key, time);
  skip_refs_[key.first].emplace_back(0);
  return true;
}

uint64_t RecordPolicy::RemoveThisAndOtherUselessRecords(
    const std::string& stream_name, StartRecordIter* record) {
  if (!IsStreamExist(stream_name)) return 0;

  StartRecords& records = GetRecords(stream_name);
  std::list<uint64_t>& skip_ref_records = skip_refs_[stream_name];
  auto end = *record;
  uint64_t remove_counter = 0;

  // update skip reference counter and remove dropped records.
  StartRecordIter start_record_iter = records.begin();
  auto skip_ref_iter = skip_ref_records.begin();
  const uint64_t max_dpb_size = GetStreamMaxDpbSize(stream_name);
  while (start_record_iter != end) {
    // update skip reference.
    (*skip_ref_iter)++;
    if (*skip_ref_iter > max_dpb_size) {
      // remove record whose skip reference counter is greater than max_dpb_size.
      start_record_iter = records.erase(start_record_iter);
      skip_ref_iter = skip_ref_records.erase(skip_ref_iter);
      remove_counter++;
    } else {
      start_record_iter++;
      skip_ref_iter++;
    }
  }

  // remove this record itself
  records.erase(start_record_iter);
  skip_ref_records.erase(skip_ref_iter);

  return remove_counter + 1;
}

void RecordPolicy::OnStreamStart(const std::string& stream_name) {
  if (IsStreamExist(stream_name)) return;
  start_records_[stream_name] = StartRecords();
  skip_refs_[stream_name] = std::list<uint64_t>();
}

void RecordPolicy::OnStreamEos(const std::string& stream_name, uint64_t* number_remaining) {
  if (!IsStreamExist(stream_name)) return;
  *number_remaining = GetRecords(stream_name).size();
  start_records_.erase(stream_name);
  skip_refs_.erase(stream_name);
  RemoveStreamMaxDpbSize(stream_name);
}

void RecordPolicy::SetStreamMaxDpbSize(const std::string& stream_name, uint64_t max_dpb_size) {
  std::lock_guard<std::mutex>  lk_guard(max_dpb_size_lk_);
  stream_max_dpb_sizes_[stream_name] = max_dpb_size;
}

uint64_t RecordPolicy::GetStreamMaxDpbSize(const std::string& stream_name) {
  std::lock_guard<std::mutex>  lk_guard(max_dpb_size_lk_);
  if (stream_max_dpb_sizes_.find(stream_name) != stream_max_dpb_sizes_.end())
    return stream_max_dpb_sizes_[stream_name];
  return stream_max_dpb_sizes_[stream_name] = kDEFAULT_MAX_DPB_SIZE;
}

void RecordPolicy::RemoveStreamMaxDpbSize(const std::string& stream_name) {
  std::lock_guard<std::mutex>  lk_guard(max_dpb_size_lk_);
  stream_max_dpb_sizes_.erase(stream_name);
}


ProcessProfiler::ProcessProfiler(const ProfilerConfig& config,
                                 const std::string& process_name,
                                 PipelineTracer* tracer)
    : config_(config), process_name_(process_name), tracer_(tracer), record_policy_(new RecordPolicy()) {
  if (!tracer) config_.enable_tracing = false;
}

ProcessProfiler::~ProcessProfiler() {
  delete record_policy_;
}

void ProcessProfiler::RecordStart(const RecordKey& key) {
  if (!config_.enable_tracing && !config_.enable_profiling) return;
  std::lock_guard<std::mutex>  lk(lk_);
  Time now = Clock::now();
  if (config_.enable_tracing) Tracing(key, now, TraceEvent::Type::START);
  if (config_.enable_profiling) RecordStart(key, now);
}

void ProcessProfiler::RecordStart(const RecordKey& key, const Time& time) {
  const std::string& stream_name = key.first;
  if (stream_profilers_.find(stream_name) == stream_profilers_.end())
    OnStreamStart(stream_name);

  if (ongoing_)
    AddPhysicalTime(time);

  record_policy_->AddStartTime(key, time);
  last_record_time_ = time;
  ongoing_++;
}

void ProcessProfiler::RecordEnd(const RecordKey& key) {
  if (!config_.enable_tracing && !config_.enable_profiling) return;
  std::lock_guard<std::mutex>  lk(lk_);
  Time now = Clock::now();
  if (config_.enable_tracing) Tracing(key, now, TraceEvent::Type::END);
  if (config_.enable_profiling) RecordEnd(key, now);
}

void ProcessProfiler::RecordEnd(const RecordKey& key, const Time& time) {
  const std::string& stream_name = key.first;
  if (stream_profilers_.find(stream_name) == stream_profilers_.end())
    OnStreamStart(stream_name);

  RecordPolicy::StartRecordIter start_record;
  if (!record_policy_->FindStartRecord(key, &start_record)) {
    if (Time::min() != last_record_time_)
      AddPhysicalTime(time);
  } else {
    if (ongoing_)
      AddPhysicalTime(time);
    Duration latency = time - start_record->second;
    AddLatency(stream_name, latency);

    uint64_t remove_counter = record_policy_->RemoveThisAndOtherUselessRecords(stream_name, &start_record);
    ongoing_ -= remove_counter;
    AddDropped(stream_name, remove_counter - 1);
  }
  last_record_time_ = time;
  stream_profilers_.find(stream_name)->second.AddCompleted();
  completed_++;
}

ProcessProfile ProcessProfiler::GetProfile() {
  ProcessProfile profile;
  profile.process_name = GetName();
  std::lock_guard<std::mutex>  lk(lk_);
  profile.completed = completed_;
  profile.dropped = dropped_;
  profile.counter = profile.completed + profile.dropped;
  profile.ongoing = ongoing_;
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
  auto stream_profilers = GetStreamProfilers();
  for (auto& it : stream_profilers) profile.stream_profiles.emplace_back(it.GetProfile());
  return profile;
}

ProcessProfile ProcessProfiler::GetProfile(const ProcessTrace& trace) const {
  ProcessProfiler profiler(ProfilerConfig(), process_name_, nullptr);
  for (const auto& elem : trace) {
    if (elem.type == TraceEvent::Type::START)
      profiler.RecordStart(elem.key, elem.time);
    else if (elem.type == TraceEvent::Type::END)
      profiler.RecordEnd(elem.key, elem.time);
  }
  return profiler.GetProfile();
}

void ProcessProfiler::OnStreamEos(const std::string& stream_name) {
  if (!config_.enable_tracing && !config_.enable_profiling) return;
  std::lock_guard<std::mutex>  lk(lk_);
  if (stream_profilers_.find(stream_name) == stream_profilers_.end()) return;
  uint64_t number_remaining = 0;
  record_policy_->OnStreamEos(stream_name, &number_remaining);
  AddDropped(stream_name, number_remaining);
  ongoing_ -= number_remaining;
  stream_profilers_.erase(stream_name);
}

void ProcessProfiler::AddPhysicalTime(const Time& now) {
  // physical time summary
  Duration time_increment = now - last_record_time_;
  total_phy_time_ += time_increment;
  for (auto& it : stream_profilers_) {
    it.second.UpdatePhysicalTime(total_phy_time_);
  }
}

void ProcessProfiler::OnStreamStart(const std::string& stream_name) {
  stream_profilers_.emplace(stream_name, StreamProfiler(stream_name));
  record_policy_->OnStreamStart(stream_name);
}

std::vector<StreamProfiler> ProcessProfiler::GetStreamProfilers() {
  std::vector<StreamProfiler> profilers;
  for (const auto& it : stream_profilers_) profilers.emplace_back(it.second);
  return profilers;
}

}  // namespace cnstream
