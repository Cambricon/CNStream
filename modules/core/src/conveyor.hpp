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
#include "threadsafe_queue.hpp"

namespace cnstream {

using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;

class Connector;

/****************************************************************************
 * @brief used to transmit data between two modules.
 *
 * Conveyor has two buffer queue, data queue and free queue.
 * A complete transmission process in a conveyor is,
 *
 * module A  -> data queue ->  module B
 *
 * Module A get data from input data queue.
 * Module A process data and push it to data queue.
 * Module B get data from data queue and push down after processing it.
 *
 * If the queue is empty, the module could not pop buffer from it
 * until it is not.
 * Or if the queue is full, the module could not push buffer, unless
 * the other module pop a buffer from it.
 ****************************************************************************/
class Conveyor {
 public:
  friend class Connector;
  // ~Conveyor();
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
  Connector* container_;
  size_t max_size_;
  bool enable_drop_;
  ThreadSafeQueue<CNFrameInfoPtr> dataq_;
  DISABLE_COPY_AND_ASSIGN(Conveyor);
};  // class Conveyor

}  // namespace cnstream

#endif  // MODULES_CORE_INCLUDE_CONVEYOR_HPP_
