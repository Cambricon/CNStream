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

#ifndef CNSTREAM_SOURCE_HPP_
#define CNSTREAM_SOURCE_HPP_

/**
 * @file cnstream_source.hpp
 *
 * This file contains a declaration of the Source Module class.
 */

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cnstream_common.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

class SourceHandler;
class SourceModule : public Module {
 public:
  explicit SourceModule(const std::string &name) : Module(name) { hasTransmit_.store(1); }
  virtual ~SourceModule() { RemoveSources(); }
  /**
   * @brief Add one stream to DataSource module, should be called after pipeline starts.
   * @param
   *   handler[in]: source handler
   * @return
   *    0: success,
   *   -1: error occurs
   */
  int AddSource(std::shared_ptr<SourceHandler> handler);

  std::shared_ptr<SourceHandler> GetSourceHandler(const std::string &stream_id);

  /**
   * @brief Remove one stream from DataSource module,should be called before pipeline stops.
   * @param
   *   handler[in]: source handler.
   * @return
   *    0: success (always success by now)
   */
  int RemoveSource(std::shared_ptr<SourceHandler> handler);
  int RemoveSource(const std::string &stream_id);

  /**
   * @hidebrief Remove all streams from DataSource module
   * @param
   * @return
   *    0: success (always success by now)
   */
  int RemoveSources();

  int Process(std::shared_ptr<CNFrameInfo> data) override {
    (void)data;
    LOG(ERROR) << "As a source module, Process() should not be invoked\n";
    return 0;
  }

 protected:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif

  friend class SourceHandler;

  uint32_t GetStreamIndex(const std::string &stream_id);
  void ReturnStreamIndex(const std::string &stream_id);

 private:
  uint64_t source_idx_ = 0;
  std::mutex mutex_;
  std::unordered_map<std::string /*stream_id*/, std::shared_ptr<SourceHandler>> source_map_;
};

class SourceHandler {
 public:
  explicit SourceHandler(SourceModule *module, const std::string &stream_id) : module_(module), stream_id_(stream_id) {
    if (module_) {
      stream_index_ = module_->GetStreamIndex(stream_id_);
    }
  }
  virtual ~SourceHandler() {
    if (module_) {
      module_->ReturnStreamIndex(stream_id_);
    }
  }

  virtual bool Open() = 0;
  virtual void Close() = 0;

  std::string GetStreamId() const { return stream_id_; }
  void SetStreamUniqueIdx(uint64_t idx) { stream_unique_idx_ = idx; }
  uint64_t GetStreamUniqueIdx() const { return stream_unique_idx_; }

 public:
  std::shared_ptr<CNFrameInfo> CreateFrameInfo(bool eos = false) {
    std::shared_ptr<CNFrameInfo> data = CNFrameInfo::Create(stream_id_, eos);
    if (data) {
      data->SetStreamIndex(stream_index_);
    }
    return data;
  }
  bool SendData(std::shared_ptr<CNFrameInfo> data) {
    if (this->module_) {
      return this->module_->TransmitData(data);
    }
    return false;
  }

 protected:
  SourceModule *module_ = nullptr;
  mutable std::string stream_id_;
  uint64_t stream_unique_idx_;
  uint32_t stream_index_ = INVALID_STREAM_IDX;
};

}  // namespace cnstream

#endif  // CNSTREAM_SOURCE_HPP_
