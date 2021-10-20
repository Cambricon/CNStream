#ifndef CPPTIME_H_
#define CPPTIME_H_

/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Michael Egli
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * \author    Michael Egli
 * \copyright Michael Egli
 * \date      11-Jul-2015
 *
 * \file cpptime.h
 *
 * C++11 timer component
 * =====================
 *
 * A portable, header-only C++11 timer component.
 *
 * Overview
 * --------
 *
 * This component can be used to manage a set of timeouts. It is implemented in
 * pure C++11. It is therefore very portable given a compliant compiler.
 *
 * A timeout can be added with one of the `add` functions, and removed with the
 * `remove` function. A timeout can be either one-shot or periodic. In case a
 * timeout is one-shot, the callback is invoked once and the timeout event is
 * then automatically removed. If the timer is periodic, it is never
 * automatically removed, but always renewed.
 *
 * Removing a timeout is possible even from within the callback.
 *
 * Timeout Units
 * -------------
 *
 * The preferred functions for adding timeouts are those that take a
 * `std::chrono::...` argument. However, for convenience, there is also an API
 * that takes a uint64_t. When using this API, all values are expected to be
 * given in microseconds (us).
 *
 * For periodic timeouts, a separate timeout can be specified for the initial
 * (first) timeout, and the periodicity after that.
 *
 * To avoid drifts, times are added by simply adding the period to the intially
 * calculated (or provided) time. Also, we use `wait until` type of API to wait
 * for a timeout instead of a `wait for` API.
 *
 * Data Structure
 * --------------
 *
 * Internally, a std::vector is used to store timeout events. The timer_id
 * returned from the `add` functions are used as index to this vector.
 *
 * In addition, a std::multiset is used that holds all time points when
 * timeouts expire.
 *
 * Using a vector to store timeout events has some implications. It is very
 * fast to remove an event, because the timer_id is the vector's index. On the
 * other hand, this makes it also more complicated to manage the timer_ids. The
 * current solution is to keep track of ids that are freed in order to re-use
 * them. A stack is used for this.
 *
 * Examples
 * --------
 *
 * More examples can be found in the `tests` folder.
 *
 * ~~~
 * CppTime::Timer t;
 * t.add(std::chrono::seconds(1), [](CppTime::timer_id){ std::cout << "got it!"; });
 * std::this_thread::sleep_for(std::chrono::seconds(2));
 * ~~~
 */

// Includes
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <set>
#include <stack>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace /*CppTime*/ cnstream {

// Public types
using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::steady_clock;
using std::chrono::time_point;

using timer_id = std::size_t;
using handler_t = std::function<void(timer_id)>;
using clock = std::chrono::steady_clock;
using timestamp = std::chrono::time_point<clock>;
using duration = std::chrono::microseconds;

// Private definitions. Do not rely on this namespace.
namespace detail {

// The event structure that holds the information about a timer.
struct Event {
  timer_id id;
  timestamp start;
  duration period;
  handler_t handler;
  bool valid;
  Event() : id(0), start(duration::zero()), period(duration::zero()), handler(nullptr), valid(false) {}
  template <typename Func>
  Event(timer_id id, timestamp start, duration period, Func &&handler)
      : id(id), start(start), period(period), handler(std::forward<Func>(handler)), valid(true) {}
  Event(Event &&r) = default;
  Event &operator=(Event &&ev) = default;
  Event(const Event &r) = delete;
  Event &operator=(const Event &r) = delete;
};

// A time event structure that holds the next timeout and a reference to its
// Event struct.
struct Time_event {
  timestamp next;
  timer_id ref;
};

inline bool operator<(const Time_event &l, const Time_event &r) { return l.next < r.next; }

}  // end namespace detail

class Timer {
  using scoped_m = std::unique_lock<std::mutex>;

  // Thread and locking variables.
  std::mutex m;
  std::condition_variable cond;
  std::thread worker;

  // Use to terminate the timer thread.
  bool done = false;

  // The vector that holds all active events.
  std::vector<detail::Event> events;
  // Sorted queue that has the next timeout at its top.
  std::multiset<detail::Time_event> time_events;

  // A list of ids to be re-used. If possible, ids are used from this pool.
  std::stack<cnstream::timer_id> free_ids;

 public:
  Timer() : m{}, cond{}, worker{}, events{}, time_events{}, free_ids{} {
    scoped_m lock(m);
    done = false;
    worker = std::thread([this] { run(); });
  }

  ~Timer() {
    scoped_m lock(m);
    done = true;
    lock.unlock();
    cond.notify_all();
    worker.join();
    events.clear();
    time_events.clear();
    while (!free_ids.empty()) {
      free_ids.pop();
    }
  }

  /**
   * Add a new timer.
   *
   * \param when The time at which the handler is invoked.
   * \param handler The callable that is invoked when the timer fires.
   * \param period The periodicity at which the timer fires. Only used for periodic timers.
   */
  timer_id add(const timestamp &when, handler_t &&handler, const duration &period = duration::zero()) {
    scoped_m lock(m);
    timer_id id = 0;
    // Add a new event. Prefer an existing and free id. If none is available, add
    // a new one.
    if (free_ids.empty()) {
      id = events.size();
      detail::Event e(id, when, period, std::move(handler));
      events.push_back(std::move(e));
    } else {
      id = free_ids.top();
      free_ids.pop();
      detail::Event e(id, when, period, std::move(handler));
      events[id] = std::move(e);
    }
    time_events.insert(detail::Time_event{when, id});
    lock.unlock();
    cond.notify_all();
    return id;
  }

  /**
   * Overloaded `add` function that uses a `std::chrono::duration` instead of a
   * `time_point` for the first timeout.
   */
  template <class Rep, class Period>
  inline timer_id add(const std::chrono::duration<Rep, Period> &when, handler_t &&handler,
                      const duration &period = duration::zero()) {
    return add(clock::now() + std::chrono::duration_cast<std::chrono::microseconds>(when), std::move(handler), period);
  }

  /**
   * Overloaded `add` function that uses a uint64_t instead of a `time_point` for
   * the first timeout and the period.
   */
  inline timer_id add(const uint64_t when, handler_t &&handler, const uint64_t period = 0) {
    return add(duration(when), std::move(handler), duration(period));
  }

  /**
   * Removes the timer with the given id.
   */
  bool remove(timer_id id) {
    scoped_m lock(m);
    if (events.size() == 0 || events.size() < id) {
      return false;
    }
    events[id].valid = false;
    auto it = std::find_if(time_events.begin(), time_events.end(),
                           [&](const detail::Time_event &te) { return te.ref == id; });
    if (it != time_events.end()) {
      free_ids.push(it->ref);
      time_events.erase(it);
    }
    lock.unlock();
    cond.notify_all();
    return true;
  }

 private:
  void run() {
    scoped_m lock(m);

    while (!done) {
      if (time_events.empty()) {
        // Wait for work
        cond.wait(lock);
      } else {
        detail::Time_event te = *time_events.begin();
        if (cnstream::clock::now() >= te.next) {
          // Remove time event
          time_events.erase(time_events.begin());

          // Invoke the handler
          lock.unlock();
          events[te.ref].handler(te.ref);
          lock.lock();

          if (events[te.ref].valid && events[te.ref].period.count() > 0) {
            // The event is valid and a periodic timer.
            te.next += events[te.ref].period;
            time_events.insert(te);
          } else {
            // The event is either no longer valid because it was removed in the
            // callback, or it is a one-shot timer.
            events[te.ref].valid = false;
            free_ids.push(te.ref);
          }
        } else {
          cond.wait_until(lock, te.next);
        }
      }
    }
  }
};

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
    return duration_cast<precision>(std::chrono::system_clock::now().time_since_epoch()).count();
  }

  /**
   * @brief generate timestamp
   * @return timestamp as string
   */
  static std::string CurrentToString() { return std::to_string(Current()); }
  /**
   * @brief generate timestamp and format it to date
   * @return date as string
   */
  static std::string CurrentToDate() {
    uint64_t now = Current();
    time_t now_in_sec = now / 1e6;
    uint64_t remainder = now % 1000000;
    struct tm now_time;
    char buf[80];
    localtime_r(&now_in_sec, &now_time);
    strftime(buf, sizeof(buf), "%Y-%m-%d-%H.%M.%S", &now_time);
    std::string result = buf;
    return result + "." + std::to_string(remainder);
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

enum class ClockType {
  Tick,
  TickTock,
};

template <ClockType type, typename precision = std::micro>
class ClockBase {
 public:
  using Elapsed_t = std::chrono::duration<double, precision>;

  /**
   * @brief calculate total elapsed time
   * @return elapsed duration
   */
  Elapsed_t ElapsedTotal() const { return total_; }

  /**
   * @brief calculate total elapsed time
   * @return elapsed duration as double
   */
  double ElapsedTotalAsDouble() const { return total_.count(); }

  /**
   * @brief calculate average elapsed time
   * @return average elapsed duration
   */
  Elapsed_t ElapsedAverage() const { return times_ == 0 ? Elapsed_t::zero() : total_ / times_; }

  /**
   * @brief calculate average elapsed time
   * @return average elapsed duration as double
   */
  double ElapsedAverageAsDouble() const { return ElapsedAverage().count(); }

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
  void Tick() { start_ = steady_clock::now(); }

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


}  // namespace cnstream

#endif  // CPPTIME_H_
