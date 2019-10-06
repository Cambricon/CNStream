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

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <chrono>
#include <ctime>
#include <string>
#include <vector>

#include "cnstream_eventbus.hpp"
#include "cnstream_pipeline.hpp"

namespace cnstream {

const EventType g_type = EVENT_ERROR;
const char *g_message = "test post event";
std::thread::id g_thread_id;

EventHandleFlag TestBusWatcher(const Event &event, Module *module) {
  EXPECT_EQ(event.type, g_type);
  EXPECT_STREQ(event.message.c_str(), g_message);
  EXPECT_EQ(event.module, module);
  EXPECT_EQ(event.thread_id, g_thread_id);
  return EVENT_HANDLE_SYNCED;
}

TEST(CoreEventBus, AddBusWatcher) {
  Pipeline pipe("pipe");
  auto bus = pipe.GetEventBus();
  uint32_t num = bus->AddBusWatch(TestBusWatcher, &pipe);
  EXPECT_EQ(num, (uint32_t)2);
  pipe.Start();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  pipe.Stop();
}

TEST(CoreEventBus, PostEvent) {
  Pipeline pipe("pipe");
  auto bus = pipe.GetEventBus();
  bus->AddBusWatch(TestBusWatcher, &pipe);
  Event event;
  event.type = g_type;
  event.message = g_message;
  event.module = &pipe;
  event.thread_id = std::this_thread::get_id();
  g_thread_id = event.thread_id;
  EXPECT_FALSE(bus->PostEvent(event)) << "bus should reject event while pipeline not running";
  pipe.Start();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_TRUE(bus->PostEvent(event));
  pipe.Stop();
}

TEST(CoreEventBus, PollEvent) {
  Pipeline pipe("pipe");
  auto bus = pipe.GetEventBus();
  Event event;
  event.type = EVENT_WARNING;
  event.message = "test poll";
  event.module = &pipe;
  EXPECT_EQ(bus->PollEvent().type, EVENT_STOP);
  bus->ClearAllWatchers();
  pipe.Start();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_TRUE(bus->PostEvent(event));
  Event poll_e = bus->PollEvent();
  EXPECT_EQ(poll_e.type, event.type);
  EXPECT_EQ(poll_e.message, event.message);
  EXPECT_EQ(poll_e.module, event.module);
  pipe.Stop();
}

TEST(CoreEventBus, ClearAllBusWatchers) {
  Pipeline pipe("pipe");
  auto bus = pipe.GetEventBus();
  EXPECT_EQ(bus->GetBusWatchers().size(), uint32_t(1));
  bus->AddBusWatch(TestBusWatcher, &pipe);
  EXPECT_EQ(bus->GetBusWatchers().size(), uint32_t(2));
  bus->ClearAllWatchers();
  EXPECT_EQ(bus->GetBusWatchers().size(), uint32_t(0));
}

}  // namespace cnstream
