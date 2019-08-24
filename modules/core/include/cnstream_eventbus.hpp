/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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

enum EventType {
  EVENT_INVALID,
  EVENT_ERROR,
  EVENT_WARNING,
  EVENT_EOS,
  EVENT_STOP,
  // remaining for custom event
  EVENT_TYPE_END
};

enum EventHandleFlag {
  // event not handled
  EVENT_HANDLE_NULL,
  // watcher informed and intercept event
  EVENT_HANDLE_INTERCEPTION,
  // watcher informed and inform other watchers
  EVENT_HANDLE_SYNCED,
  // stop poll event
  EVENT_HANDLE_STOP
};

struct Event {
  EventType type;
  std::string message;
  const Module *module;
  std::thread::id thread_id;
};  // struct Event

using BusWatcher = std::function<EventHandleFlag(const Event &, Module *)>;

class EventBus {
 public:
  friend class Pipeline;
  /************************************************************************
   * @brief post event to bus
   * @param
   *   event[in]: event to be posted
   ************************************************************************/
  bool PostEvent(Event event);

  /************************************************************************
   * @brief add watcher to event bus
   * @param
   *   func[in]: bus watcher to added
   *   watch_module[in]: module which add this bus watcher
   * @return number of bus watchers that has been added to this event bus
   ************************************************************************/
  uint32_t AddBusWatch(BusWatcher func, Module *watch_module);

 private:
#ifdef TEST
 public:
#endif
  EventBus();
  ~EventBus();
  Event PollEvent();
  const std::list<std::pair<BusWatcher, Module *>> &GetBusWatchers() const;
  void ClearAllWatchers();
  inline bool IsRunning() { return running_; }

 private:
  EventBusPrivate *d_ptr_;
  bool running_ = false;
  std::mutex watcher_mut_;

  DECLARE_PRIVATE(d_ptr_, EventBus);
  DISABLE_COPY_AND_ASSIGN(EventBus);
};  // class EventBus

}  // namespace cnstream

#endif  // CNSTREAM_EVENT_BUS_HPP_
