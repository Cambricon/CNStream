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
#include <memory>
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
  void SetReserved_(bool reserve) { server_->reserved_ = reserve; }
  bool GetReserved_() { return server_->reserved_; }
  int GetTickets_reserved_time() { return static_cast<int>(server_->tickets_q_.back().reserved_time); }
  int GetPreviousTickets_reserved_time(QueuingTicketRoot* qtr) { return static_cast<int>(qtr->reserved_time); }
  QueuingTicketRoot& GetCurrentQueueBack() { return server_->tickets_q_.back(); }
  int Get_shared_with_no_wait(QueuingTicket* pticket) {
    std::future_status status = pticket->wait_for(std::chrono::seconds(1));
    // if has not set_value
    if (status == std::future_status::timeout) {
      std::cout << "Timeout" << std::endl;
      return 1;
    }
    return 0;
  }

 private:
  QueuingServer* server_ = nullptr;
};  // class QueuingServerTest

TEST(Inferencer, QueuingServer_PickUpTicket) {
  std::shared_ptr<QueuingServer> qserver = std::make_shared<QueuingServer>();
  QueuingServerTest qserver_test(qserver.get());
  QueuingTicket ticket1;
  EXPECT_NO_THROW(ticket1 = qserver->PickUpTicket(true));
  EXPECT_EQ(1, qserver_test.GetTicketSize());
  // queue has only one ticket,call at once
  EXPECT_EQ(qserver_test.Get_shared_with_no_wait(&ticket1), 0);

  QueuingTicket ticket2 = qserver->PickUpTicket(true);  // create another new ticket
  QueuingTicket ticket3 = qserver->PickUpTicket();
  EXPECT_EQ(qserver_test.GetTickets_reserved_time(), 2) << "error, should not set the reserved_time";
  // still an old ticket,should be called(shared_future multi call)
  EXPECT_EQ(qserver_test.Get_shared_with_no_wait(&ticket3), 0);

  QueuingTicket ticket4 = qserver->PickUpTicket();
  EXPECT_EQ(qserver_test.GetTickets_reserved_time(), 0);
  // a new ticket, should be not called
  EXPECT_EQ(qserver_test.Get_shared_with_no_wait(&ticket4), 1);
}

TEST(Inferencer, QueuingServer_PickUpNewTicket) {
  std::shared_ptr<QueuingServer> qserver = std::make_shared<QueuingServer>();
  QueuingServerTest qserver_test(qserver.get());
  QueuingTicket ticket1 = qserver->PickUpNewTicket(true);
  EXPECT_EQ(1, qserver_test.GetTicketSize());
  EXPECT_EQ(qserver_test.GetTickets_reserved_time(), 1);
  QueuingTicketRoot& root1 = qserver_test.GetCurrentQueueBack();

  // create another new ticket
  QueuingTicket ticket2 = qserver->PickUpNewTicket();
  // last time reserve but this time create a new ticket
  EXPECT_EQ(0, qserver_test.GetPreviousTickets_reserved_time(&root1));
}

TEST(Inferencer, QueuingServer_DeallingDone) {
  std::shared_ptr<QueuingServer> qserver = std::make_shared<QueuingServer>();
  QueuingServerTest qserver_test(qserver.get());
  /* no tickets, no throw */
  EXPECT_NO_THROW(qserver->DeallingDone());
  /* one tickets, no throw , ticket size from 1 to 0 */
  QueuingTicket ticket1 = qserver->PickUpTicket(false);
  ASSERT_EQ(1, qserver_test.GetTicketSize());

  QueuingTicket ticket2 = qserver->PickUpTicket(false);  // a new ticket
  QueuingTicket ticket3 = qserver->PickUpTicket(true);   // a new ticket
  QueuingTicket ticket4 = qserver->PickUpTicket(true);   // an old ticket
  QueuingTicket ticket5 = qserver->PickUpTicket(false);  // still an old ticket
  int wait_time = 100;
  std::atomic<bool> DeallingDoneCall(false);
  std::function<double()> wait_Dealling([&]() -> double {
    DeallingDoneCall.store(true);
    auto stime = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
    qserver->DeallingDone();
    auto etime = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> diff = etime - stime;
    return diff.count();
  });
  std::future<double> task_future = std::async(std::launch::async, wait_Dealling);
  while (!DeallingDoneCall.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  double real_wait_time = task_future.get();
  EXPECT_GE(real_wait_time, static_cast<double>(wait_time));

  // ticket1 poped, and ticket2 set_value
  EXPECT_EQ(qserver_test.Get_shared_with_no_wait(&ticket2), 0);
  qserver->DeallingDone();
  qserver->DeallingDone();
  EXPECT_EQ(qserver_test.GetTickets_reserved_time(), 1);
  EXPECT_EQ(1, qserver_test.GetTicketSize());
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
    auto stime = std::chrono::steady_clock::now();
    qserver.WaitByTicket(&ticket);
    auto etime = std::chrono::steady_clock::now();
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
