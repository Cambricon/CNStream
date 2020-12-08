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

#include "queuing_server.hpp"
#include "cnstream_logging.hpp"

namespace cnstream {

QueuingTicket QueuingServer::PickUpTicket(bool reserve) {
  std::lock_guard<std::mutex> lk(mtx_);
  QueuingTicket ticket;
  if (reserved_) {
    // last ticket reserved, return it.
    ticket = reserved_ticket_;
  } else {
    // create new ticket.
    tickets_q_.push(QueuingTicketRoot());
    ticket = tickets_q_.back().root.get_future().share();
    if (tickets_q_.size() == 1) {
      // only one ticket, call at once
      Call();
    }
  }
  if (reserve) {
    // reserve current ticket for next pick up.
    reserved_ticket_ = ticket;
    tickets_q_.back().reserved_time++;
    reserved_ = true;
  } else {
    // do not reserve the current ticket
    reserved_ = false;
    // tickets_q_.back().reserved_time = 0;
  }
  return ticket;
}

QueuingTicket QueuingServer::PickUpNewTicket(bool reserve) {
  std::lock_guard<std::mutex> lk(mtx_);
  QueuingTicket ticket;
  if (reserved_) {
    // last ticket reserved, clean it.
    if (0 == tickets_q_.back().reserved_time) {
      LOGF_IF(INFERENCER, static_cast<int>(tickets_q_.size()) != 1) << "Internel error";
      tickets_q_.pop();
    } else {
      tickets_q_.back().reserved_time--;
    }
    reserved_ = false;
  }
  // create new ticket.
  tickets_q_.push(QueuingTicketRoot());
  ticket = tickets_q_.back().root.get_future().share();
  if (tickets_q_.size() == 1) {
    // only one ticket, call at once
    Call();
  }
  if (reserve) {
    // reserve current ticket for next pick up.
    reserved_ticket_ = ticket;
    tickets_q_.back().reserved_time++;
    reserved_ = true;
  }
  return ticket;
}

void QueuingServer::DeallingDone() {
  std::lock_guard<std::mutex> lk(mtx_);
  if (!tickets_q_.empty()) {
    if (0 == tickets_q_.front().reserved_time) {
      tickets_q_.pop();
      Call();
    } else {
      tickets_q_.front().reserved_time--;
    }
  }
}

void QueuingServer::WaitByTicket(QueuingTicket* pticket) { pticket->get(); }

// set a signal
void QueuingServer::Call() {
  if (!tickets_q_.empty()) {
    QueuingTicketRoot& ticket_root = tickets_q_.front();
    ticket_root.root.set_value();
  }
}

}  // namespace cnstream
