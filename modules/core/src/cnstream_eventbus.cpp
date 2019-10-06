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
#include <utility>

#include "cnstream_pipeline.hpp"
#include "threadsafe_queue.hpp"

namespace cnstream {

class EventBusPrivate {
 private:
  explicit EventBusPrivate(EventBus *d) : q_ptr_(d) {}

  ThreadSafeQueue<Event> queue_;
  std::list<std::pair<BusWatcher, Module *>> bus_watchers_;

  DECLARE_PUBLIC(q_ptr_, EventBus);
  DISABLE_COPY_AND_ASSIGN(EventBusPrivate);
};  // class EventBusPrivate

EventBus::EventBus() { d_ptr_ = new EventBusPrivate(this); }

EventBus::~EventBus() { delete d_ptr_; }

// return number of bus watchers
uint32_t EventBus::AddBusWatch(BusWatcher func, Module *watch_module) {
  std::unique_lock<std::mutex> lk(watcher_mut_);
  d_ptr_->bus_watchers_.push_front(std::make_pair(func, watch_module));
  return d_ptr_->bus_watchers_.size();
}

void EventBus::ClearAllWatchers() {
  std::lock_guard<std::mutex> lk(watcher_mut_);
  d_ptr_->bus_watchers_.clear();
}

const std::list<std::pair<BusWatcher, Module *>> &EventBus::GetBusWatchers() const { return d_ptr_->bus_watchers_; }

bool EventBus::PostEvent(Event event) {
  if (!running_) {
    LOG(WARNING) << "Post event failed, pipeline not running";
    return false;
  }
  LOG(INFO) << "Recieve Event from [" << event.module->GetName() << "] :" << event.message;
  d_ptr_->queue_.Push(event);
  return true;
}

Event EventBus::PollEvent() {
  Event event;
  event.type = EVENT_INVALID;
  while (running_) {
    if (d_ptr_->queue_.WaitAndTryPop(event, std::chrono::milliseconds(100))) {
      break;
    }
  }
  if (!running_) event.type = EVENT_STOP;
  return event;
}

}  // namespace cnstream
