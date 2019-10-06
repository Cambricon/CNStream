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

#include <vector>

#include "conveyor.hpp"

namespace cnstream {

class ConnectorPrivate {
 private:
  explicit ConnectorPrivate(Connector* q);
  ~ConnectorPrivate();
  Conveyor* GetConveyorByIdx(int idx) const;

  DECLARE_PUBLIC(q_ptr_, Connector);
  std::vector<Conveyor*> vec_conveyor_;
  size_t conveyor_capacity_ = 20;
  bool stop_ = false;
  DISABLE_COPY_AND_ASSIGN(ConnectorPrivate);
};  // class ConnectorPrivate

Connector::Connector(const size_t conveyor_count, size_t conveyor_capacity) : d_ptr_(new ConnectorPrivate(this)) {
  d_ptr_->conveyor_capacity_ = conveyor_capacity;
  d_ptr_->vec_conveyor_.reserve(conveyor_count);
  for (size_t i = 0; i < conveyor_count; ++i) {
    d_ptr_->vec_conveyor_.push_back(new Conveyor(this, conveyor_capacity));
  }
}

Connector::~Connector() { delete d_ptr_; }

const size_t Connector::GetConveyorCount() const { return d_ptr_->vec_conveyor_.size(); }

Conveyor* Connector::GetConveyor(int conveyor_idx) const { return d_ptr_->GetConveyorByIdx(conveyor_idx); }

size_t Connector::GetConveyorCapacity() const { return d_ptr_->conveyor_capacity_; }

CNFrameInfoPtr Connector::PopDataBufferFromConveyor(int conveyor_idx) {
  return GetConveyor(conveyor_idx)->PopDataBuffer();
}

void Connector::PushDataBufferToConveyor(int conveyor_idx, CNFrameInfoPtr data) {
  GetConveyor(conveyor_idx)->PushDataBuffer(data);
}

bool Connector::IsStopped() const { return d_ptr_->stop_; }

void Connector::Start() { d_ptr_->stop_ = false; }

void Connector::Stop() { d_ptr_->stop_ = true; }

ConnectorPrivate::ConnectorPrivate(Connector* q) : q_ptr_(q), stop_(false) {}

ConnectorPrivate::~ConnectorPrivate() {
  for (Conveyor* it : vec_conveyor_) {
    delete it;
  }
}

Conveyor* ConnectorPrivate::GetConveyorByIdx(int idx) const {
  CHECK_GE(idx, 0);
  CHECK_LT(idx, vec_conveyor_.size());
  return vec_conveyor_[idx];
}

void Connector::EmptyDataQueue() {
  size_t conveyor_cnt = GetConveyorCount();
  for (size_t conveyor_idx = 0; conveyor_idx < conveyor_cnt; ++conveyor_idx) {
    d_ptr_->GetConveyorByIdx(conveyor_idx)->PopAllDataBuffer();
  }
}

}  // namespace cnstream
