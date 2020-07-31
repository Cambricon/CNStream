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
#include <glog/logging.h>

#include <memory>

#include "easyinfer/easy_infer.h"
#include "internal/mlu_task_queue.h"
#include "model_loader_internal.h"

namespace edk {

#define CALL_CNRT_FUNC(func, msg)                                       \
  do {                                                                  \
    int ret = (func);                                                   \
    if (0 != ret) {                                                     \
      LOG(ERROR) << (msg) << " error code: " << ret;                    \
      throw EasyInferError(msg " error code : " + std::to_string(ret)); \
    }                                                                   \
  } while (0)

class EasyInferPrivate {
 public:
  std::shared_ptr<ModelLoader> ploader_ = nullptr;
  cnrtFunction_t function_ = nullptr;
  MluTaskQueue_t queue_ = nullptr;
  void** param_ = nullptr;
  int batch_size_ = 1;
  cnrtRuntimeContext_t runtime_context_ = nullptr;
  cnrtNotifier_t notifier_start_ = nullptr, notifier_end_ = nullptr;
};

EasyInfer::EasyInfer() {
  d_ptr_ = new EasyInferPrivate;
  d_ptr_->queue_ = std::make_shared<MluTaskQueue>();
}

EasyInfer::~EasyInfer() {
  if (d_ptr_->runtime_context_ != nullptr) {
    cnrtDestroyRuntimeContext(d_ptr_->runtime_context_);
  }
  if (nullptr != d_ptr_->function_) {
    cnrtDestroyFunction(d_ptr_->function_);
  }
  if (nullptr != d_ptr_->notifier_start_) {
    cnrtDestroyNotifier(&d_ptr_->notifier_start_);
  }
  if (nullptr != d_ptr_->notifier_end_) {
    cnrtDestroyNotifier(&d_ptr_->notifier_end_);
  }
  if (nullptr != d_ptr_->param_) {
    delete[] d_ptr_->param_;
  }
  delete d_ptr_;
}

void EasyInfer::Init(std::shared_ptr<ModelLoader> ploader, int batch_size, int dev_id) {
  d_ptr_->ploader_ = ploader;
  ModelLoaderInternalInterface interface(d_ptr_->ploader_.get());

  LOG(INFO) << "Init inference context, batch size: " << batch_size << " device id: " << dev_id;

  // init function
  CALL_CNRT_FUNC(cnrtCreateFunction(&d_ptr_->function_), "Create function failed.");

  CALL_CNRT_FUNC(cnrtCopyFunction(&d_ptr_->function_, interface.Function()), "Copy function failed.");

  d_ptr_->batch_size_ = 1;
  cnrtChannelType_t channel = CNRT_CHANNEL_TYPE_NONE;
  CALL_CNRT_FUNC(cnrtCreateRuntimeContext(&d_ptr_->runtime_context_, d_ptr_->function_, NULL),
                 "Create runtime context failed!");

  CALL_CNRT_FUNC(cnrtSetRuntimeContextChannel(d_ptr_->runtime_context_, channel),
                 "Set Runtime Context Channel failed!");
  CALL_CNRT_FUNC(cnrtSetRuntimeContextDeviceId(d_ptr_->runtime_context_, dev_id),
                 "Set Runtime Context Device Id failed!");
  CALL_CNRT_FUNC(cnrtInitRuntimeContext(d_ptr_->runtime_context_, NULL), "Init runtime context failed!");

  LOG(INFO) << "Create MLU task queue from runtime context";
  CALL_CNRT_FUNC(cnrtRuntimeContextCreateQueue(d_ptr_->runtime_context_, &d_ptr_->queue_->queue),
                 "Runtime Context Create Queue failed");
  // init param
  d_ptr_->param_ = new void*[d_ptr_->ploader_->InputNum() + d_ptr_->ploader_->OutputNum()];
  // create event for hardware time
  CALL_CNRT_FUNC(cnrtCreateNotifier(&d_ptr_->notifier_start_), "Create notifier failed");
  CALL_CNRT_FUNC(cnrtCreateNotifier(&d_ptr_->notifier_end_), "Create notifier failed");
}

void EasyInfer::Run(void** input, void** output, float* hw_time) const {
  int i_num = d_ptr_->ploader_->InputNum();
  int o_num = d_ptr_->ploader_->OutputNum();

  VLOG(5) << "Process inference on one frame, input num: " << i_num << " output num: " << o_num;
  VLOG(5) << "Inference, input: " << input << " output: " << output;
  // prepare params for invokefunction
  for (int i = 0; i < i_num; ++i) {
    d_ptr_->param_[i] = input[i];
  }
  for (int i = 0; i < o_num; ++i) {
    d_ptr_->param_[i_num + i] = output[i];
  }
  if (hw_time) {
    // place start event
    CALL_CNRT_FUNC(cnrtPlaceNotifier(d_ptr_->notifier_start_, d_ptr_->queue_->queue), "Place event failed");
  }

  CALL_CNRT_FUNC(cnrtInvokeRuntimeContext(d_ptr_->runtime_context_, d_ptr_->param_, d_ptr_->queue_->queue, NULL),
                 "Invoke Runtime Context failed");

  if (hw_time) {
    // place end event
    CALL_CNRT_FUNC(cnrtPlaceNotifier(d_ptr_->notifier_end_, d_ptr_->queue_->queue), "Place event failed");
  }
  CALL_CNRT_FUNC(cnrtSyncQueue(d_ptr_->queue_->queue), "Sync queue failed.");
  if (hw_time) {
    CALL_CNRT_FUNC(cnrtNotifierDuration(d_ptr_->notifier_start_, d_ptr_->notifier_end_, hw_time),
                   "Calculate elapsed time failed.");
    *hw_time /= 1000.0f;
    VLOG(3) << "Inference hardware time " << *hw_time << " ms";
  }
}

std::shared_ptr<ModelLoader> EasyInfer::Loader() const { return d_ptr_->ploader_; }

int EasyInfer::BatchSize() const { return d_ptr_->batch_size_; }

MluTaskQueue_t EasyInfer::GetMluQueue() const { return d_ptr_->queue_; }

}  // namespace edk
