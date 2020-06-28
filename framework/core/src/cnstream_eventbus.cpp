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
#include "util/cnstream_queue.hpp"

namespace cnstream {

class EventBusPrivate {
 private:
  explicit EventBusPrivate(EventBus *d) : q_ptr_(d) {}

  ThreadSafeQueue<Event> queue_;
  std::list<std::pair<BusWatcher, Pipeline *>> bus_watchers_;
  std::thread event_thread_;
  std::atomic<bool> running_{false};
  DECLARE_PUBLIC(q_ptr_, EventBus);
  DISABLE_COPY_AND_ASSIGN(EventBusPrivate);
};  // class EventBusPrivate

EventBus::EventBus() {
  d_ptr_ = new (std::nothrow) EventBusPrivate(this);
  LOG_IF(FATAL, nullptr == d_ptr_) << "EventBus::EventBus() new EventBusPrivate failed.";
}

EventBus::~EventBus() {
  Stop();
  delete d_ptr_;
}

bool EventBus::IsRunning() {
  if (d_ptr_) {
    return d_ptr_->running_.load();
  }
  return false;
}

bool EventBus::Start() {
  d_ptr_->running_.store(true);
  d_ptr_->event_thread_ = std::thread(&EventBus::EventLoop, this);
  return true;
}

void EventBus::Stop() {
  if (IsRunning()) {
    d_ptr_->running_.store(false);
    if (d_ptr_->event_thread_.joinable()) {
      d_ptr_->event_thread_.join();
    }
  }
}

// return number of bus watchers
uint32_t EventBus::AddBusWatch(BusWatcher func, Pipeline *watcher) {
  std::unique_lock<std::mutex> lk(watcher_mut_);
  d_ptr_->bus_watchers_.push_front(std::make_pair(func, watcher));
  return d_ptr_->bus_watchers_.size();
}

void EventBus::ClearAllWatchers() {
  std::lock_guard<std::mutex> lk(watcher_mut_);
  d_ptr_->bus_watchers_.clear();
}

const std::list<std::pair<BusWatcher, Pipeline *>> &EventBus::GetBusWatchers() const { return d_ptr_->bus_watchers_; }

bool EventBus::PostEvent(Event event) {
  if (!d_ptr_->running_.load()) {
    LOG(WARNING) << "Post event failed, pipeline not running";
    return false;
  }
  // LOG(INFO) << "Recieve Event from [" << event.module->GetName() << "] :" << event.message;
  d_ptr_->queue_.Push(event);
  return true;
}

Event EventBus::PollEvent() {
  Event event;
  event.type = EVENT_INVALID;
  while (d_ptr_->running_.load()) {
    if (d_ptr_->queue_.WaitAndTryPop(event, std::chrono::milliseconds(100))) {
      break;
    }
  }
  if (!d_ptr_->running_.load()) event.type = EVENT_STOP;
  return event;
}

void EventBus::EventLoop() {
  const std::list<std::pair<BusWatcher, Pipeline *>> &kWatchers = GetBusWatchers();
  EventHandleFlag flag = EVENT_HANDLE_NULL;

  SetThreadName("cn-EventLoop", pthread_self());
  // start loop
  while (IsRunning()) {
    Event event = PollEvent();
    if (event.type == EVENT_INVALID) {
      LOG(INFO) << "[EventLoop] event type is invalid";
      break;
    } else if (event.type == EVENT_STOP) {
      LOG(INFO) << "[EventLoop] Get stop event";
      break;
    }
    std::unique_lock<std::mutex> lk(watcher_mut_);
    for (auto &watcher : kWatchers) {
      flag = watcher.first(event, watcher.second);
      if (flag == EVENT_HANDLE_INTERCEPTION || flag == EVENT_HANDLE_STOP) {
        break;
      }
    }
    if (flag == EVENT_HANDLE_STOP) {
      break;
    }
  }
  LOG(INFO) << "Event bus exit.";
}

}  // namespace cnstream
