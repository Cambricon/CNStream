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

#include "infer_engine.hpp"
#include <cxxutil/exception.h>
#include <easyinfer/mlu_context.h>
#include <easyinfer/model_loader.h>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "batching_done_stage.hpp"
#include "batching_stage.hpp"
#include "infer_resource.hpp"
#include "infer_thread_pool.hpp"

namespace cnstream {

void InferEngine::ResultWaitingCard::WaitForCall() { promise_->get_future().share().get(); }

InferEngine::InferEngine(int dev_id, std::shared_ptr<edk::ModelLoader> model, std::shared_ptr<Preproc> preprocessor,
                         std::shared_ptr<Postproc> postprocessor, uint32_t batchsize, float batching_timeout,
                         bool use_scaler, const std::function<void(const std::string& err_msg)>& error_func)
    : model_(model),
      preprocessor_(preprocessor),
      postprocessor_(postprocessor),
      batchsize_(batchsize),
      batching_timeout_(batching_timeout),
      error_func_(error_func),
      dev_id_(dev_id),
      use_scaler_(use_scaler) {
  try {
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(dev_id);
    mlu_ctx.ConfigureForThisThread();
    tp_ = std::make_shared<InferThreadPool>();
    tp_->SetErrorHandleFunc(error_func);
    tp_->Init(dev_id, batchsize * 3 + 4);
    cpu_input_res_ = std::make_shared<CpuInputResource>(model, batchsize);
    cpu_output_res_ = std::make_shared<CpuOutputResource>(model, batchsize);
    mlu_input_res_ = std::make_shared<MluInputResource>(model, batchsize);
    mlu_output_res_ = std::make_shared<MluOutputResource>(model, batchsize);
    if (mlu_ctx.GetCoreVersion() == edk::CoreVersion::MLU270) {
      use_scaler_ = false;
    }
    if (!use_scaler_) rcop_res_ = std::make_shared<RCOpResource>(model, batchsize);
    cpu_input_res_->Init();
    cpu_output_res_->Init();
    mlu_input_res_->Init();
    mlu_output_res_->Init();
    StageAssemble();
    timeout_helper_.SetTimeout(batching_timeout);
  } catch (CnstreamError& e) {
    if (error_func_) {
      error_func_(e.what());
    } else {
      LOG(FATAL) << "Not handled error: " << std::string(e.what());
    }
  } catch (edk::Exception& e) {
    if (error_func_) {
      error_func_(e.what());
    } else {
      LOG(FATAL) << "Not handled error: " << std::string(e.what());
    }
  }
}

InferEngine::~InferEngine() {
  // make sure timeout is not active before release resources.
  std::lock_guard<std::mutex> lk(mtx_);
  timeout_helper_.Reset(NULL);
  try {
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(dev_id_);
    mlu_ctx.ConfigureForThisThread();
    tp_->Destroy();
    cpu_input_res_->Destroy();
    cpu_output_res_->Destroy();
    mlu_input_res_->Destroy();
    mlu_output_res_->Destroy();
    if (rcop_res_.get()) rcop_res_->Destroy();
    DLOG(INFO) << "Destroied resources";
  } catch (CnstreamError& e) {
    if (error_func_) {
      error_func_(e.what());
    } else {
      LOG(FATAL) << "Not handled error: " << std::string(e.what());
    }
  } catch (edk::Exception& e) {
    if (error_func_) {
      error_func_(e.what());
    } else {
      LOG(FATAL) << "Not handled error: " << std::string(e.what());
    }
  }
}

InferEngine::ResultWaitingCard InferEngine::FeedData(std::shared_ptr<CNFrameInfo> finfo) {
  std::lock_guard<std::mutex> lk(mtx_);
  InferTaskSptr task = batching_stage_->Batching(finfo);
  tp_->SubmitTask(task);
  auto ret_promise = std::make_shared<std::promise<void>>();
  ResultWaitingCard card(ret_promise);
  batched_finfos_.push_back(std::make_pair(finfo, ret_promise));
  if (batched_finfos_.size() == batchsize_) {
    BatchingDone();
    timeout_helper_.Reset(NULL);
  } else {
    timeout_helper_.Reset([this]() -> void {
      std::lock_guard<std::mutex> lk(mtx_);
      BatchingDone();
    });
  }
  return card;
}

static bool IsYAndUVSplit(const std::shared_ptr<edk::ModelLoader>& model) {
  auto shapes = model->InputShapes();
  return shapes.size() == 2 && shapes[0].c == 1 && shapes[0].c == shapes[1].c &&
         1.0 * shapes[0].hw() / shapes[1].hw() == 2.0;
}

static bool IsYUVPacked(const std::shared_ptr<edk::ModelLoader>& model) {
  auto shapes = model->InputShapes();
  return shapes.size() == 1 && shapes[0].c == 1;
}

void InferEngine::StageAssemble() {
  if (preprocessor_.get()) {
    // 1. cpu preprocessing
    batching_stage_ =
        std::make_shared<CpuPreprocessingBatchingStage>(model_, batchsize_, preprocessor_, cpu_input_res_);
    std::shared_ptr<BatchingDoneStage> h2d_stage =
        std::make_shared<H2DBatchingDoneStage>(model_, batchsize_, dev_id_, cpu_input_res_, mlu_input_res_);
    batching_done_stages_.push_back(h2d_stage);
  } else {
    // 2. mlu preprocessing
    if (IsYAndUVSplit(model_)) {
      // 2.1 y and uv split, use d2d
      batching_stage_ = std::make_shared<YUVSplitBatchingStage>(model_, batchsize_, mlu_input_res_);
    } else if (IsYUVPacked(model_)) {
      // 2.2 y and uv packed, use d2d
      batching_stage_ = std::make_shared<YUVPackedBatchingStage>(model_, batchsize_, mlu_input_res_);
    } else {
      // 2.3 rgb0 input.
      if (use_scaler_) {
        // 2.3.1 use scaler (MLU220)
        batching_stage_ = std::make_shared<ScalerBatchingStage>(model_, batchsize_, mlu_input_res_);
      } else {
        // 2.3.2 use resize convert
        batching_stage_ = std::make_shared<ResizeConvertBatchingStage>(model_, batchsize_, dev_id_, rcop_res_);
        std::shared_ptr<BatchingDoneStage> rc_done_stage =
            std::make_shared<ResizeConvertBatchingDoneStage>(model_, batchsize_, dev_id_, rcop_res_, mlu_input_res_);
        batching_done_stages_.push_back(rc_done_stage);
      }
    }
  }
  std::shared_ptr<BatchingDoneStage> infer_stage =
      std::make_shared<InferBatchingDoneStage>(model_, batchsize_, dev_id_, mlu_input_res_, mlu_output_res_);
  auto mlu_queue = dynamic_cast<InferBatchingDoneStage*>(infer_stage.get())->SharedMluQueue();
  if (rcop_res_.get()) rcop_res_->SetMluQueue(mlu_queue);  // multiplexing cnrtQueue from EasyInfer.
  std::shared_ptr<BatchingDoneStage> d2h_stage =
      std::make_shared<D2HBatchingDoneStage>(model_, batchsize_, dev_id_, mlu_output_res_, cpu_output_res_);
  std::shared_ptr<BatchingDoneStage> postproc_stage =
      std::make_shared<PostprocessingBatchingDoneStage>(model_, batchsize_, dev_id_, postprocessor_, cpu_output_res_);
  batching_done_stages_.push_back(infer_stage);
  batching_done_stages_.push_back(d2h_stage);
  batching_done_stages_.push_back(postproc_stage);
}

void InferEngine::BatchingDone() {
  if (!batched_finfos_.empty()) {
    for (auto& it : batching_done_stages_) {
      std::vector<InferTaskSptr> tasks = it->BatchingDone(batched_finfos_);
      tp_->SubmitTask(tasks);
    }
    batched_finfos_.clear();
  }
}

}  // namespace cnstream
