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

#include "connector.hpp"

#include <atomic>
#include <vector>
#include "conveyor.hpp"

namespace cnstream {

Connector::Connector(const size_t conveyor_count, size_t conveyor_capacity) {
  conveyor_capacity_ = conveyor_capacity;
  conveyors_.reserve(conveyor_count);
  for (size_t i = 0; i < conveyor_count; ++i) {
    Conveyor* conveyor = new (std::nothrow) Conveyor(this, conveyor_capacity);
    LOG_IF(FATAL, nullptr == conveyor) << "Connector::Connector()  new Conveyor failed.";
    conveyors_.push_back(conveyor);
  }
}

Connector::~Connector() {
  for (Conveyor* conveyor : conveyors_) {
    delete conveyor;
  }
}

const size_t Connector::GetConveyorCount() const {
  return conveyors_.size();
}

Conveyor* Connector::GetConveyor(int conveyor_idx) const {
  return GetConveyorByIdx(conveyor_idx);
}

size_t Connector::GetConveyorCapacity() const {
  return conveyor_capacity_;
}

CNFrameInfoPtr Connector::PopDataBufferFromConveyor(int conveyor_idx) {
  return GetConveyor(conveyor_idx)->PopDataBuffer();
}

void Connector::PushDataBufferToConveyor(int conveyor_idx, CNFrameInfoPtr data) {
  GetConveyor(conveyor_idx)->PushDataBuffer(data);
}

bool Connector::IsStopped() {
  return stop_.load();
}

void Connector::Start() {
  stop_.store(false);
}

void Connector::Stop() {
  stop_.store(true);
}

Conveyor* Connector::GetConveyorByIdx(int idx) const {
  CHECK_GE(idx, 0);
  CHECK_LT(idx, static_cast<int>(conveyors_.size()));
  return conveyors_[idx];
}

void Connector::EmptyDataQueue() {
  size_t conveyor_cnt = GetConveyorCount();
  for (size_t conveyor_idx = 0; conveyor_idx < conveyor_cnt; ++conveyor_idx) {
    GetConveyorByIdx(conveyor_idx)->PopAllDataBuffer();
  }
}

}  // namespace cnstream
