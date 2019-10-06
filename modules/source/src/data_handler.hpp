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

#ifndef MODULES_SOURCE_DATA_HANDLER_HPP_
#define MODULES_SOURCE_DATA_HANDLER_HPP_

#include <string>
#include <thread>

#include "cnstream_frame.hpp"
#include "data_source.hpp"

namespace cnstream {

class DataHandler {
 public:
  explicit DataHandler(DataSource *module, const std::string &stream_id, int frame_rate, bool loop)
      : module_(module), stream_id_(stream_id), frame_rate_(frame_rate), loop_(loop) {
    streamIndex_ = this->GetStreamIndex();
  }
  virtual ~DataHandler() {
    this->ReturnStreamIndex();
    Close();
  }

  bool Open();
  void Close();

 public:
  std::string GetStreamId() const { return stream_id_; }
  size_t GetStreamIndex();
  static const size_t INVALID_STREAM_ID = -1;
  DevContext GetDevContext() const { return dev_ctx_; }
  bool SendData(std::shared_ptr<CNFrameInfo> data) {
    if (this->module_) {
      return this->module_->SendData(data);
    }
    return false;
  }
  void EnableFlowEos(bool enable) {
    if (enable)
      send_flow_eos_.store(1);
    else
      send_flow_eos_.store(0);
  }
  void SendFlowEos() {
    auto data = CNFrameInfo::Create(stream_id_, true);
    data->channel_idx = streamIndex_;
    // LOG(INFO) << "[Source]  " << stream_id_ << " receive eos.";
    if (this->module_ && send_flow_eos_.load()) {
      this->module_->SendData(data);
    }
  }
  bool GetDemuxEos() const { return demux_eos_.load() ? true : false; }
  bool ReuseCNDecBuf() const { return param_.reuse_cndec_buf; }

 protected:
  DataSource *module_ = nullptr;
  std::string stream_id_;
  int frame_rate_;
  bool loop_;
  DataSourceParam param_;
  DevContext dev_ctx_;
  size_t interval_ = 1;
  std::atomic<int> demux_eos_{0};

 private:
  size_t streamIndex_ = INVALID_STREAM_ID;
  void ReturnStreamIndex() const;
  static std::mutex index_mutex_;
  static uint64_t index_mask_;

  std::atomic<int> running_{0};
  std::thread thread_;
  void Loop();
  /*the below three funcs are in the same thread*/
  virtual bool PrepareResources() = 0;
  virtual void ClearResources() = 0;
  virtual bool Process() = 0;
  /**/
  std::atomic<int> send_flow_eos_{0};
};

}  // namespace cnstream

#endif  // MODULES_SOURCE_DATA_HANDLER_HPP_
