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

class DataHandler : public SourceHandler {
 public:
  explicit DataHandler(DataSource *module, const std::string &stream_id, int frame_rate, bool loop)
      : SourceHandler(module, stream_id, frame_rate, loop) {}

  virtual bool Open() override;
  virtual void Close() override;

 public:
  DevContext GetDevContext() const { return dev_ctx_; }
  void EnableFlowEos(bool enable) {
    if (enable)
      send_flow_eos_.store(1);
    else
      send_flow_eos_.store(0);
  }
  void SendFlowEos() {
    if (eos_sent_) return;
    if (send_flow_eos_.load()) {
      auto data = CNFrameInfo::Create(stream_id_, true);
      if (!data) {
        LOG(ERROR) << "SendFlowEos: Create CNFrameInfo failed while received eos. stream id is " << stream_id_;
        return;
      }
      data->channel_idx = stream_index_;

      SendData(data);
      eos_sent_ = true;
    }
  }
  bool GetDemuxEos() const { return demux_eos_.load() ? true : false; }
  bool ReuseCNDecBuf() const { return param_.reuse_cndec_buf; }
  size_t Output_w() { return param_.output_w; }
  size_t Output_h() { return param_.output_h; }
  uint32_t InputBufNumber() { return param_.input_buf_number_; }
  uint32_t OutputBufNumber() { return param_.output_buf_number_; }

 protected:
  DataSourceParam param_;
  DevContext dev_ctx_;
  size_t interval_ = 1;
  std::atomic<int> demux_eos_{0};

 private:
  std::atomic<int> running_{0};
  std::thread thread_;
  void Loop();
  /*the below three funcs are in the same thread*/
  virtual bool PrepareResources(bool demux_only = false) = 0;
  virtual void ClearResources(bool demux_only = false) = 0;
  virtual bool Process() = 0;
  /**/
  std::atomic<int> send_flow_eos_{0};
  bool eos_sent_ = false;
};

}  // namespace cnstream

#endif  // MODULES_SOURCE_DATA_HANDLER_HPP_
