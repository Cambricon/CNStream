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

#include "conveyor.hpp"

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "connector.hpp"

namespace cnstream {

Conveyor::Conveyor(Connector* container, size_t max_size, bool enable_drop)
    : container_(container), max_size_(max_size), enable_drop_(enable_drop) {
  LOG_IF(FATAL, nullptr == container) << "container should not be nullptr.";
}

uint32_t Conveyor::GetBufferSize() const { return dataq_.Size(); }

void Conveyor::PushDataBuffer(CNFrameInfoPtr data) {
  while (!container_->IsStopped() && dataq_.Size() >= max_size_) {
    if (enable_drop_) {
      CNFrameInfoPtr drop;
      dataq_.TryPop(drop);
      break;
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }
  if (container_->IsStopped()) return;
  dataq_.Push(data);
}

CNFrameInfoPtr Conveyor::PopDataBuffer() {
  CNFrameInfoPtr data;
  while (!container_->IsStopped()) {
    if (dataq_.WaitAndTryPop(data, std::chrono::milliseconds(20))) {
      break;
    }
  }
  if (container_->IsStopped()) {
    return nullptr;
  }
  return data;
}

std::vector<CNFrameInfoPtr> Conveyor::PopAllDataBuffer() {
  std::vector<CNFrameInfoPtr> vec_data;
  CNFrameInfoPtr data;
  while (!dataq_.Empty()) {
    dataq_.TryPop(data);
    vec_data.push_back(data);
  }
  return vec_data;
}

}  // namespace cnstream
