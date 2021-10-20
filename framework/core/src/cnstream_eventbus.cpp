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

#include "cnstream_eventbus.hpp"

#include <list>
#include <memory>
#include <thread>
#include <utility>

#include "cnstream_pipeline.hpp"

namespace cnstream {

EventBus::~EventBus() {
  Stop();
}

bool EventBus::IsRunning() {
  return running_.load();
}

bool EventBus::Start() {
  running_.store(true);
  event_thread_ = std::thread(&EventBus::EventLoop, this);
  return true;
}

void EventBus::Stop() {
  if (IsRunning()) {
    running_.store(false);
    if (event_thread_.joinable()) {
      event_thread_.join();
    }
  }
}

// @return The number of bus watchers that has been added to this event bus.
uint32_t EventBus::AddBusWatch(BusWatcher func) {
  std::lock_guard<std::mutex> lk(watcher_mtx_);
  bus_watchers_.push_front(func);
  return bus_watchers_.size();
}

void EventBus::ClearAllWatchers() {
  std::lock_guard<std::mutex> lk(watcher_mtx_);
  bus_watchers_.clear();
}

const std::list<BusWatcher> &EventBus::GetBusWatchers() const {
  std::lock_guard<std::mutex> lk(watcher_mtx_);
  return bus_watchers_;
}

bool EventBus::PostEvent(Event event) {
  if (!running_.load()) {
    LOGW(CORE) << "Post event failed, pipeline not running";
    return false;
  }
  // LOGI(CORE) << "Receieve event from [" << event.module->GetName() << "] :" << event.message;
  queue_.Push(event);
#ifdef UNIT_TEST
  if (unit_test) {
    test_eventq_.Push(event);
    unit_test = false;
  }
#endif
  return true;
}

Event EventBus::PollEvent() {
  Event event;
  event.type = EventType::EVENT_INVALID;
  while (running_.load()) {
    if (queue_.WaitAndTryPop(event, std::chrono::milliseconds(100))) {
      break;
    }
  }
  if (!running_.load()) event.type = EventType::EVENT_STOP;
  return event;
}

void EventBus::EventLoop() {
  const std::list<BusWatcher> &kWatchers = GetBusWatchers();
  EventHandleFlag flag = EventHandleFlag::EVENT_HANDLE_NULL;

  // start loop
  while (IsRunning()) {
    Event event = PollEvent();
    if (event.type == EventType::EVENT_INVALID) {
      LOGI(CORE) << "[EventLoop] event type is invalid";
      break;
    } else if (event.type == EventType::EVENT_STOP) {
      LOGI(CORE) << "[EventLoop] Get stop event";
      break;
    }
    std::unique_lock<std::mutex> lk(watcher_mtx_);
    for (auto &watcher : kWatchers) {
      flag = watcher(event);
      if (flag == EventHandleFlag::EVENT_HANDLE_INTERCEPTION || flag == EventHandleFlag::EVENT_HANDLE_STOP) {
        break;
      }
    }
    if (flag == EventHandleFlag::EVENT_HANDLE_STOP) {
      break;
    }
  }
  LOGI(CORE) << "Event bus exit.";
}

#ifdef UNIT_TEST
Event EventBus::PollEventToTest() {
  Event event;
  event.type = EventType::EVENT_INVALID;
  while (running_.load()) {
    if (test_eventq_.WaitAndTryPop(event, std::chrono::milliseconds(100))) {
      break;
    }
  }
  if (!running_.load()) event.type = EventType::EVENT_STOP;
  return event;
}
#endif

}  // namespace cnstream
