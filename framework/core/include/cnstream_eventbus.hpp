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

#ifndef CNSTREAM_EVENT_BUS_HPP_
#define CNSTREAM_EVENT_BUS_HPP_

/**
 *  @file cnstream_eventbus.hpp
 *
 *  This file contains a declaration of the EventBus class.
 */

#include <atomic>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "cnstream_common.hpp"
#include "util/cnstream_queue.hpp"

namespace cnstream {

class Pipeline;

/**
 * @brief Flags to specify the way in which bus watchers handled one event.
 */
enum EventHandleFlag {
  EVENT_HANDLE_NULL,          ///< The event is not handled.
  EVENT_HANDLE_INTERCEPTION,  ///< The bus watcher is informed, and the event is intercepted.
  EVENT_HANDLE_SYNCED,        ///< The bus watcher is informed, and then other bus watchers are informed.
  EVENT_HANDLE_STOP           ///< A poll event is stopped.
};

/**
 * @brief A structure holding the event information.
 */
struct Event {
  EventType type;             ///< The event type.
  std::string stream_id;      ///< The stream that posts this event.
  std::string message;        ///< Additional event messages.
  std::string module_name;    ///< The module that posts this event.
  std::thread::id thread_id;  ///< The thread id from which the event is posted.
};

/**
 * @brief The bus watcher function.
 *
 * @param event The event polled from the event bus.
 * @param Pipeline The module that is watching.
 *
 * @return Returns the flag that specifies how the event is handled.
 */
using BusWatcher = std::function<EventHandleFlag(const Event &)>;

/**
 * @brief The event bus that transmits events from modules to a pipeline.
 */
class EventBus : private NonCopyable {
 public:
  friend class Pipeline;
  /**
   * @brief Starts an event bus thread.
   */
  bool Start();
  void Stop();
  /**
   * @brief Adds the watcher to the event bus.
   *
   * @param func The bus watcher to be added.
   *
   * @return The number of bus watchers that has been added to this event bus.
   */
  uint32_t AddBusWatch(BusWatcher func);

  /**
   * @brief Posts an event to a bus.
   *
   * @param event The event to be posted.
   *
   * @return Returns true if this function run successfully. Otherwise, returns false.
   */
  bool PostEvent(Event event);

 private:
#ifdef UNIT_TEST

 public:
  Event PollEventToTest();
#endif
  EventBus() = default;

  ~EventBus();

  /**
   * @brief Polls an event from a bus [block].
   *
   * @note This function is blocked until an event or a bus is stopped.
   */
  Event PollEvent();

  /**
   * @brief Gets all bus watchers from the event bus.
   *
   * @return A list with pairs of bus watcher and module.
   */
  const std::list<BusWatcher> &GetBusWatchers() const;

  /**
   * @brief Removes all bus watchers.
   */
  void ClearAllWatchers();

  /**
   * @brief Checks if the event bus is running.
   *
   * @return Returns true if the event bus is running. Otherwise, returns false.
   */
  bool IsRunning();

  void EventLoop();

 private:

  std::mutex watcher_mtx_;
  ThreadSafeQueue<Event> queue_;
#ifdef UNIT_TEST
  ThreadSafeQueue<Event> test_eventq_;
  bool unit_test = true;
#endif
  std::list<BusWatcher> bus_watchers_;
  std::thread event_thread_;
  std::atomic<bool> running_{false};
};  // class EventBus

}  // namespace cnstream

#endif  // CNSTREAM_EVENT_BUS_HPP_
