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

namespace cnstream {

class Pipeline;
class Module;
class EventBusPrivate;

/**
 * @brief Flag to specify how bus watcher handle a single event.
 */
enum EventType {
  EVENT_INVALID,  ///< An invalid event type.
  EVENT_ERROR,    ///< An error event.
  EVENT_WARNING,  ///< A warning event.
  EVENT_EOS,      ///< An EOS event.
  EVENT_STOP,     ///< Stops an event that is called by application layer usually.
  EVENT_TYPE_END  ///< Reserved for your custom events.
};

/**
 * @brief Flag to specify the way in which bus watcher handled one event.
 */
enum EventHandleFlag {
  EVENT_HANDLE_NULL,          ///< Event is not handled.
  EVENT_HANDLE_INTERCEPTION,  ///< Watcher is informed and intercept the event.
  EVENT_HANDLE_SYNCED,        ///< Watcher is informed and informed other watchers.
  EVENT_HANDLE_STOP           ///< Stops a poll event.
};

/**
 * @brief The structure holding the event information.
 */
struct Event {
  EventType type;             ///< The event type.
  std::string message;        ///< Additional event messages.
  const Module *module;       ///< The module that posts this event.
  std::thread::id thread_id;  ///< The thread id from which the event is posted.
};

/**
 * @brief The bus watcher function
 *
 * @param event The event polled from the event bus.
 * @param module The module that is watching.
 *
 * @return Flag specifies how the event is handled.
 */
using BusWatcher = std::function<EventHandleFlag(const Event &, Module *)>;

/**
 * @brief The event bus that transmits events from modules to a pipeline.
 */
class EventBus {
 public:
  friend class Pipeline;
  /**
   * @brief Posts an event to bus.
   *
   * @param event The event to be posted.
   *
   * @return Returns true if this function run successfully.
   */
  bool PostEvent(Event event);

  /**
   * @brief Adds the watcher to the event bus.
   *
   * @param func The bus watcher to be added.
   * @param watch_module The module that adds this bus watcher.
   *
   * @return The number of bus watchers that has been added to this event bus.
   */
  uint32_t AddBusWatch(BusWatcher func, Module *watch_module);

 private:
#ifdef UNIT_TEST

 public:
#endif
  EventBus();
  ~EventBus();

  /**
   * @brief Polls an event from a bus [block].
   *
   * @note Block until an event or a bus is stopped.
   */
  Event PollEvent();

  /**
   * @brief Get all bus watchers from the event bus.
   *
   * @return A list with pairs of bus_watcher and module.
   */
  const std::list<std::pair<BusWatcher, Module *>> &GetBusWatchers() const;

  /**
   * @brief Removes all bus watchers.
   */
  void ClearAllWatchers();

  /**
   * @brief Check the event bus is running or not.
   *
   * @return Return true if the event bus is running, false if it's not.
   */
  inline bool IsRunning() const { return running_.load(); }
  std::atomic<bool> running_{false};

 private:
  std::mutex watcher_mut_;

  DECLARE_PRIVATE(d_ptr_, EventBus);
  DISABLE_COPY_AND_ASSIGN(EventBus);
};  // class EventBus

}  // namespace cnstream

#endif  // CNSTREAM_EVENT_BUS_HPP_
