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

#include "data_handler.hpp"
#include "easyinfer/mlu_context.h"
#include "fr_controller.hpp"
#include "glog/logging.h"

namespace cnstream {

bool DataHandler::Open() {
  if (!this->module_) {
    LOG(ERROR) << "module_ null";
    return false;
  }

  perf_manager_ = module_->GetPerfManager(stream_id_);

  // default value
  dev_ctx_.dev_type = DevContext::MLU;
  dev_ctx_.dev_id = 0;

  // updated with paramSet
  DataSource *source = dynamic_cast<DataSource *>(module_);
  param_ = source->GetSourceParam();
  if (param_.output_type_ == OUTPUT_CPU) {
    dev_ctx_.dev_type = DevContext::CPU;
    dev_ctx_.dev_id = -1;
  } else if (param_.output_type_ == OUTPUT_MLU) {
    dev_ctx_.dev_type = DevContext::MLU;
    dev_ctx_.dev_id = param_.device_id_;
  } else {
    LOG(ERROR) << "output_type not supported:" << param_.output_type_;
    return false;
  }

  size_t chn_idx = stream_index_;
  if (chn_idx == cnstream::INVALID_STREAM_IDX) {
    LOG(ERROR) << "invalid chn_idx(stream_idx)";
    return false;
  }
  dev_ctx_.ddr_channel = chn_idx % 4;  // FIXME

  this->interval_ = param_.interval_;

  // start demuxer
  running_.store(1);
  thread_ = std::move(std::thread(&DataHandler::Loop, this));
  return true;
}

void DataHandler::Close() {
  if (running_.load()) {
    running_.store(0);
    if (thread_.joinable()) {
      thread_.join();
    }
  }
}

void DataHandler::Loop() {
  /*meet cnrt requirement*/
  if (dev_ctx_.dev_id != DevContext::INVALID) {
    try {
      edk::MluContext mlu_ctx;
      mlu_ctx.SetDeviceId(dev_ctx_.dev_id);
      mlu_ctx.SetChannelId(dev_ctx_.ddr_channel);
      mlu_ctx.ConfigureForThisThread();
    } catch (edk::Exception &e) {
      if (nullptr != module_)
        module_->PostEvent(EVENT_ERROR, "stream_id " + stream_id_ + " failed to setup dev/channel.");
      return;
    }
  }

  if (!PrepareResources()) {
    if (nullptr != module_)
      module_->PostEvent(
          EVENT_ERROR, "stream_id " + stream_id_ + "Prepare codec resources failed, maybe codec resources not enough.");
    return;
  }

  FrController controller(frame_rate_);
  if (frame_rate_ > 0) controller.Start();

  while (running_.load()) {
    if (!this->Process()) {
      break;
    }
    if (frame_rate_ > 0) controller.Control();
  }

  ClearResources();
}

}  // namespace cnstream
