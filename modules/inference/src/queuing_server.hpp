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

#ifndef MODULES_INFERENCE_SRC_QUEUING_SERVER_HPP_
#define MODULES_INFERENCE_SRC_QUEUING_SERVER_HPP_

#include <future>
#include <mutex>
#include <queue>

namespace cnstream {

struct QueuingTicketRoot {
  std::promise<void> root;
  uint32_t reserved_time = 0;
};
using QueuingTicket = std::shared_future<void>;

class QueuingServerTest;
class QueuingServer {
 public:
  friend class QueuingServerTest;
  QueuingTicket PickUpTicket(bool reserve = false);
  QueuingTicket PickUpNewTicket(bool reserve = false);
  void DeallingDone();
  void WaitByTicket(QueuingTicket* pticket);

 private:
  void Call();
  std::queue<QueuingTicketRoot> tickets_q_;
  QueuingTicket reserved_ticket_;
  bool reserved_ = false;
  std::mutex mtx_;
};

}  // namespace cnstream

#endif  // MODULES_INFERENCE_SRC_QUEUING_SERVER_HPP_
