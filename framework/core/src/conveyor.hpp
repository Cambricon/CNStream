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

#ifndef MODULES_CORE_INCLUDE_CONVEYOR_HPP_
#define MODULES_CORE_INCLUDE_CONVEYOR_HPP_

#include <memory>
#include <vector>

#include "cnstream_frame.hpp"
#include "util/cnstream_queue.hpp"

namespace cnstream {

using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;

class Connector;

/**
 * @brief Conveyor is used to transmit data between two modules.
 *
 * Conveyor belongs to Connector.
 * Each Connect could have several conveyors which depends on the paramllelism of each module.
 *
 * Conveyor has one buffer queue for transmitting data from one module to another.
 * The upstream node module will push data to buffer queue, and the downstream node will pop data from buffer queue.
 *
 * The capacity of buffer queue could be set in configuration json file (see README for more information of
 * configuration json file). If there is no element in buffer queue, the downstream node will wait to pop and
 * be blocked. On contrary, if the queue is full, the upstream node will wait to push and be blocked.
 */
class Conveyor {
 public:
  friend class Connector;

  ~Conveyor() = default;
  void PushDataBuffer(CNFrameInfoPtr data);
  CNFrameInfoPtr PopDataBuffer();
  std::vector<CNFrameInfoPtr> PopAllDataBuffer();
  uint32_t GetBufferSize();

 private:
#ifdef UNIT_TEST
 public:
#endif
  Conveyor(Connector* container, size_t max_size, bool enable_drop = false);

 private:
  DISABLE_COPY_AND_ASSIGN(Conveyor);

  ThreadSafeQueue<CNFrameInfoPtr> dataq_;
  Connector* container_;
  size_t max_size_;
  bool enable_drop_;
};  // class Conveyor

}  // namespace cnstream

#endif  // MODULES_CORE_INCLUDE_CONVEYOR_HPP_
