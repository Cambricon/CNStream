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
 *  \file cnstream_eventbus.hpp
 *
 *  This file contains a declaration of class EventBus.
 */

#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <thread>

#include "cnstream_common.hpp"

namespace cnstream {

class Pipeline;
class Module;
class EventBusPrivate;

/************************************************************************
 * @brief Flag to specify event type.
 ************************************************************************/
enum EventType {
  EVENT_INVALID,  ///> Invalid event type.
  EVENT_ERROR,    ///> Error event.
  EVENT_WARNING,  ///> Warning event.
  EVENT_EOS,      ///> EOS event.
  EVENT_STOP,     ///> Stop event, raised by application layer usually.
  EVENT_TYPE_END  ///> Remaining for user custom event
};

/************************************************************************
 * @brief Flag to specify the way in which bus watcher handled one event.
 ************************************************************************/
enum EventHandleFlag {
  EVENT_HANDLE_NULL,          ///> Event not handled.
  EVENT_HANDLE_INTERCEPTION,  ///> Watcher informed and intercept event.
  EVENT_HANDLE_SYNCED,        ///> Watcher informed and inform other watchers.
  EVENT_HANDLE_STOP           ///> Stop poll event.
};

/************************************************************************
 * @brief Structure to store event information.
 ************************************************************************/
struct Event {
  EventType type;             ///> Event type.
  std::string message;        ///> Additional event message.
  const Module *module;       ///> Module that post this event.
  std::thread::id thread_id;  ///> Thread id from which event is posted.
};                            // struct Event

/************************************************************************
 * Bus watcher function
 *
 * @param event Event polled from eventbus.
 * @param module Module that is watching.
 *
 * @return Flag indicated how the event was handled.
 ************************************************************************/
using BusWatcher = std::function<EventHandleFlag(const Event &, Module *)>;

/**
 * Event bus, transmit event from modules to pipeline.
 */
class EventBus {
 public:
  friend class Pipeline;
  /************************************************************************
   * Post event to bus.
   *
   * @param event Event to be posted.
   *
   * @return Return true for success.
   ************************************************************************/
  bool PostEvent(Event event);

  /************************************************************************
   * Add watcher to event bus.
   *
   * @param func Bus watcher to added.
   * @param watch_module Module which add this bus watcher.
   *
   * @return Number of bus watchers that has been added to this event bus.
   ************************************************************************/
  uint32_t AddBusWatch(BusWatcher func, Module *watch_module);

 private:
#ifdef TEST
 public:
#endif
  EventBus();
  ~EventBus();

  /************************************************************************
   * Poll a event from bus [block].
   *
   * @note Block until get a event or bus stopped.
   ************************************************************************/
  Event PollEvent();
  const std::list<std::pair<BusWatcher, Module *>> &GetBusWatchers() const;
  /************************************************************************
   * Remove all bus watchers.
   ************************************************************************/
  void ClearAllWatchers();
  inline bool IsRunning() const { return running_; }
  bool running_ = false;

 private:
  std::mutex watcher_mut_;

  DECLARE_PRIVATE(d_ptr_, EventBus);
  DISABLE_COPY_AND_ASSIGN(EventBus);
};  // class EventBus

}  // namespace cnstream

#endif  // CNSTREAM_EVENT_BUS_HPP_
