/*************************************************************************
 * copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_CORE_INCLUDE_CNSTREAM_TIME_UTILITY_HPP_
#define MODULES_CORE_INCLUDE_CNSTREAM_TIME_UTILITY_HPP_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <ratio>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::steady_clock;
using std::chrono::time_point;

namespace cnstream {

/// Timestamp utilities
/****************************************************************************
  class TimeStamp provides a way to generate unique timestamps which based on
  the epoch time. The default precision of timestamps is in microseconds and
  configurable in class TimeStampBase.
  Samples are as follows:

  uint64_t time_stamp = TimeStamp::Current();
  std::string ts_str = TimeStamp::CurrentToString();

  uint64_t ts2 = TimeStampBase<std::chrono::nanoseconds>::Current();

  ***************************************************************************/

/**
 * @brief a timestamp generator
 */
template <typename precision = microseconds>
class TimeStampBase {
 public:
  /**
   * @brief generate timestamp
   * @return timestamp as uint64_t
   */
  static uint64_t Current() {
    return duration_cast<precision>(
        steady_clock::now().time_since_epoch()).count();
  }

  /**
   * @brief generate timestamp
   * @return timestamp as string
   */
  static std::string CurrentToString() {
    return std::to_string(Current());
  }
};

/**
 * @brief simplified interface
 */
using TimeStamp = TimeStampBase<>;

/// Clock utilities
/****************************************************************************
  Clock classes provides two kinds of clocks. TickClock is a ticker-tape clock
  and TickTockClock is a duration recorder. The default precision is in
  microseconds and configurable in class ClockBase.
  Samples are as follows:

  TickClock tick_clock;
  for (int i = 0; i < 10; ++i) {
    tick_clock.Tick();
    // do something...
  }
  double average_execute_time = tick_clock.ElapsedAverageAsDouble();

  TickTockClock duration_recorder;
  void foo() {
    duration_recorder.Tick();
    // do something...
    duration_recorder.Tock();
  }
  for (int i = 0; i < 10; ++i) {
    foo();
  }
  double average_duration = duration_recorder.ElapsedAverageAsDouble();

  ***************************************************************************/

enum struct ClockType {
  Tick,
  TickTock,
};

template <ClockType type, typename precision = std::micro>
class ClockBase {
 public:
  using Elapsed_t = duration<double, precision>;

  /**
   * @brief calculate total elapsed time
   * @return elapsed duration
   */
  Elapsed_t ElapsedTotal() const {
    return total_;
  }

  /**
   * @brief calculate total elapsed time
   * @return elapsed duration as double
   */
  double ElapsedTotalAsDouble() const {
    return total_.count();
  }

  /**
   * @brief calculate average elapsed time
   * @return average elapsed duration
   */
  Elapsed_t ElapsedAverage() const {
    return times_ == 0 ? Elapsed_t::zero() : total_ / times_;
  }

  /**
   * @brief calculate average elapsed time
   * @return average elapsed duration as double
   */
  double ElapsedAverageAsDouble() const {
    return ElapsedAverage().count();
  }

  /**
   * @brief clear records
   * @return void
   */
  void Clear() {
    total_ = Elapsed_t::zero();
    times_ = 0;
  }

 protected:
  Elapsed_t total_ = Elapsed_t::zero();
  uint32_t times_ = 0;
};

/**
 * @brief a ticker-tape clock
 */
class TickClock final : public ClockBase<ClockType::Tick> {
 public:
  /**
   * @brief tick
   * @return void
   */
  void Tick() {
    curr_ = steady_clock::now();
    if (!started_) {
      started_ = true;
    } else {
      total_ += curr_ - prev_;
      ++times_;
    }
    prev_ = curr_;
  }

 private:
  time_point<steady_clock> prev_, curr_;
  bool started_ = false;
};

/**
 * @brief a duration recorder
 */
class TickTockClock final : public ClockBase<ClockType::TickTock> {
 public:
  /**
   * @brief record start time
   * @return void
   */
  void Tick() {
    start_ = steady_clock::now();
  }

  /**
   * @brief record end time
   * @return void
   */
  void Tock() {
    end_ = steady_clock::now();
    total_ += end_ - start_;
    ++times_;
  }

 private:
  time_point<steady_clock> start_, end_;
};

/// Timer utilities
/****************************************************************************
  class Timer provides a way to execute delayed tasks. Tasks start with a
  duration and an expiry action. The default update frequency of timer is in
  100 microseconds and configurable in its constructor.
  Samples are as follows:

  void foo(void *p) { }
  void bar(int a, double b) { }

  Timer timer(std::chrono::seconds(1));
  Timer::ExpiryAction f1 = std::bind(&foo, nullptr);
  Timer::ExpiryAction f2 = std::bind(&bar, 1, 3.5);
  timer.StartOne(std::chrono::seconds(1), f1);
  timer.StartOne(std::chrono::seconds(2), f2);

  **************************************************************************/

/**
 * @brief a async timed task executor
 */
class Timer {
 public:
  using ExpiryAction = std::function<void()>;

  /**
   * @brief constructor
   *
   * @param update_frequency
   */
  explicit Timer(microseconds update_frequency = microseconds(100))
    :update_frequency_(update_frequency) {
    main_loop_ = std::thread(&Timer::MainLoop, this);
    for (auto &th : executors_) {
      th = std::thread(&Timer::ThreadFunc, this);
    }
  }

  ~Timer() {
    stop_ = true;
    main_loop_.join();
    action_cond_.notify_all();
    for (auto &th : executors_) {
      th.join();
    }
  }

  /**
   * @brief start one task
   *
   * @param interval deffered time
   * @param task deffered task
   * @return void
   */
  void Start(ExpiryAction task, microseconds interval = microseconds(0))  {
    std::lock_guard<std::mutex> lock(task_mtx_);
    tasks_.push_back(std::make_pair(interval, task));
  }

 private:
  void MainLoop() {
    while (!tasks_.empty() || !stop_) {
      std::this_thread::sleep_for(update_frequency_);

      std::unique_lock<std::mutex> lock(task_mtx_);
      for (auto it = tasks_.begin(); it != tasks_.end();) {
        it->first -= update_frequency_;
        if (it->first.count() <= 0) {
          action_queue_.push(it->second);
          action_cond_.notify_one();
          it = tasks_.erase(it);
        } else {
          ++it;
        }
      }
    }
    main_loop_exit_ = true;
  }

  void ThreadFunc() {
    std::unique_lock<std::mutex> lock(action_mtx_);
    while (!action_queue_.empty() || !main_loop_exit_) {
      action_cond_.wait(lock, [this] {
          return !action_queue_.empty() || main_loop_exit_;});

      if (!action_queue_.empty()) {
        ExpiryAction action = action_queue_.front();
        action_queue_.pop();
        lock.unlock();
        action();
        lock.lock();
      }
    }
  }

  std::thread main_loop_;
  std::thread executors_[5];
  std::mutex task_mtx_, action_mtx_;
  std::condition_variable action_cond_;
  std::vector<std::pair<microseconds, ExpiryAction>> tasks_;
  std::queue<ExpiryAction> action_queue_;
  microseconds update_frequency_;
  std::atomic<bool> stop_{false};
  std::atomic<bool> main_loop_exit_{false};
};

}  // namespace cnstream

#endif  // MODULES_CORE_INCLUDE_CNSTREAM_TIME_UTILITY_HPP_
