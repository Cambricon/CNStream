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

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <mutex>
#include <thread>

#include "queuing_server.hpp"

namespace cnstream {

class QueuingServerTest {
 public:
  explicit QueuingServerTest(QueuingServer* server) : server_(server) {}
  int GetTicketSize() {
    std::lock_guard<std::mutex> lk(server_->mtx_);
    return static_cast<int>(server_->tickets_q_.size());
  }

 private:
  QueuingServer* server_ = nullptr;
};  // class QueuingServerTest

TEST(Inferencer, QueuingServer_PickUpTicket) {
  QueuingServer qserver;
  QueuingServerTest qserver_test(&qserver);
  QueuingTicket ticket;
  EXPECT_NO_THROW(ticket = qserver.PickUpTicket());
  EXPECT_EQ(1, qserver_test.GetTicketSize());
}

TEST(Inferencer, QueuingServer_DeallingDone) {
  QueuingServer qserver;
  QueuingServerTest qserver_test(&qserver);
  /* no tickets, no throw */
  EXPECT_NO_THROW(qserver.DeallingDone());
  /* one tickets, no throw , ticket size from 1 to 0 */
  QueuingTicket ticket = qserver.PickUpTicket();
  ASSERT_EQ(1, qserver_test.GetTicketSize());
  qserver.DeallingDone();
  EXPECT_EQ(0, qserver_test.GetTicketSize());
}

TEST(Inferencer, QueuingServer_WaitByTicket) {
  QueuingServer qserver;
  QueuingServerTest qserver_test(&qserver);
  QueuingTicket ticket1 = qserver.PickUpTicket();
  // ticket1 called immediately
  QueuingTicket ticket = qserver.PickUpTicket();
  /* check wait time */
  int wait_time = 100;  // ms
  std::atomic<bool> task_run(false);
  std::function<double()> wait_task([&]() -> double {
    task_run.store(true);
    auto stime = std::chrono::high_resolution_clock::now();
    qserver.WaitByTicket(&ticket);
    auto etime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> diff = etime - stime;
    return diff.count();
  });
  std::future<double> task_future = std::async(std::launch::async, wait_task);
  while (!task_run.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
  qserver.DeallingDone();
  double real_wait_time = task_future.get();
  EXPECT_GE(real_wait_time, static_cast<double>(wait_time));
}

}  // namespace cnstream
