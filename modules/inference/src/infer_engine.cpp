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
#include <device/mlu_context.h>
#include <easyinfer/model_loader.h>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "batching_done_stage.hpp"
#include "batching_stage.hpp"
#include "cnstream_frame_va.hpp"
#include "infer_resource.hpp"
#include "infer_thread_pool.hpp"
#include "obj_batching_stage.hpp"
#include "obj_filter.hpp"

namespace cnstream {

void InferEngine::ResultWaitingCard::WaitForCall() { promise_->get_future().share().get(); }

InferEngine::InferEngine(int dev_id, std::shared_ptr<edk::ModelLoader> model, std::shared_ptr<Preproc> preprocessor,
                         std::shared_ptr<Postproc> postprocessor, uint32_t batchsize, uint32_t batching_timeout,
                         bool use_scaler, std::shared_ptr<PerfManager> perf_manager, std::string infer_thread_id,
                         const std::function<void(const std::string& err_msg)>& error_func, bool keep_aspect_ratio,
                         bool batching_by_obj, const std::shared_ptr<ObjPreproc>& obj_preprocessor,
                         const std::shared_ptr<ObjPostproc>& obj_postprocessor,
                         const std::shared_ptr<ObjFilter>& obj_filter,
                         std::string dump_resized_image_dir)
     :model_(model),
      preprocessor_(preprocessor),
      postprocessor_(postprocessor),
      batchsize_(batchsize),
      batching_timeout_(batching_timeout),
      error_func_(error_func),
      dev_id_(dev_id),
      use_scaler_(use_scaler),
      batching_by_obj_(batching_by_obj),
      obj_preprocessor_(obj_preprocessor),
      obj_postprocessor_(obj_postprocessor),
      obj_filter_(obj_filter),
      infer_perf_manager_(perf_manager),
      infer_thread_id_(infer_thread_id),
      dump_resized_image_dir_(dump_resized_image_dir) {
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
    if (!use_scaler_) rcop_res_ = std::make_shared<RCOpResource>(model, batchsize, keep_aspect_ratio);
    cpu_input_res_->Init();
    cpu_output_res_->Init();
    mlu_input_res_->Init();
    mlu_output_res_->Init();
    StageAssemble();
    timeout_helper_.SetTimeout(batching_timeout_);
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
  timeout_helper_.LockOperator();
  timeout_helper_.Reset(NULL);
  timeout_helper_.UnlockOperator();
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
  auto ret_promise = std::make_shared<std::promise<void>>();
  ResultWaitingCard card(ret_promise);

  auto auto_set_done = std::make_shared<AutoSetDone>(ret_promise);
  if (batching_by_obj_) {
    if (finfo->datas.find(CNObjsVecKey) == finfo->datas.end()) {
      return card;
    }
    CNObjsVec objs = cnstream::any_cast<CNObjsVec>(finfo->datas[CNObjsVecKey]);
    for (size_t idx = 0; idx < objs.size(); ++idx) {
      auto& obj = objs[idx];
      if (obj_filter_) {
        if (!obj_filter_->Filter(finfo, obj)) continue;
      }
      timeout_helper_.LockOperator();
      InferTaskSptr task = obj_batching_stage_->Batching(finfo, obj);
      tp_->SubmitTask(task);
      batched_finfos_.push_back(std::make_pair(finfo, auto_set_done));
      batched_objs_.push_back(obj);

      if (batched_finfos_.size() == batchsize_) {
        BatchingDone();
        timeout_helper_.Reset(NULL);
      } else {
        timeout_helper_.Reset([this]() -> void { BatchingDone(); });
      }
      timeout_helper_.UnlockOperator();
    }
  } else {
    timeout_helper_.LockOperator();
    InferTaskSptr task = batching_stage_->Batching(finfo);
    tp_->SubmitTask(task);
    batched_finfos_.push_back(std::make_pair(finfo, auto_set_done));

    if (batched_finfos_.size() == batchsize_) {
      BatchingDone();
      timeout_helper_.Reset(NULL);
    } else {
      timeout_helper_.Reset([this]() -> void { BatchingDone(); });
    }
    timeout_helper_.UnlockOperator();
  }
  return card;
}

static bool CheckModel(const std::shared_ptr<edk::ModelLoader>& model) {
  auto shapes = model->InputShapes();
  if (shapes.size() != 1) {
    LOG(ERROR) << "Unsupport model with " << shapes.size() << " input.";
    return false;
  }

  if (shapes[0].c != 4) {
    LOG(ERROR) << "Use mlu to do preprocessing, only support model with c = 4, but c = " << shapes[0].c;
    return false;
  }
  return true;
}

void InferEngine::StageAssemble() {
  bool cpu_preprocessing = (!batching_by_obj_ && preprocessor_.get()) || (batching_by_obj_ && obj_preprocessor_.get());
  if (cpu_preprocessing) {
    // 1. cpu preprocessing
    if (batching_by_obj_) {
      obj_batching_stage_ =
          std::make_shared<CpuPreprocessingObjBatchingStage>(model_, batchsize_, obj_preprocessor_, cpu_input_res_);
    } else {
      batching_stage_ =
          std::make_shared<CpuPreprocessingBatchingStage>(model_, batchsize_, preprocessor_, cpu_input_res_);
    }
    std::shared_ptr<BatchingDoneStage> h2d_stage =
        std::make_shared<H2DBatchingDoneStage>(model_, batchsize_, dev_id_, cpu_input_res_, mlu_input_res_);
    batching_done_stages_.push_back(h2d_stage);
  } else {
    // 2. mlu preprocessing
    CHECK_EQ(true, CheckModel(model_));
    // rgb0 input.
    if (use_scaler_) {
      // use scaler (MLU220 only)
      if (batching_by_obj_) {
        obj_batching_stage_ = std::make_shared<ScalerObjBatchingStage>(model_, batchsize_, mlu_input_res_);
      } else {
        batching_stage_ = std::make_shared<ScalerBatchingStage>(model_, batchsize_, mlu_input_res_);
      }
    } else {
      // use resize convert
      if (batching_by_obj_) {
        obj_batching_stage_ = std::make_shared<ResizeConvertObjBatchingStage>(model_, batchsize_, dev_id_, rcop_res_);
      } else {
        batching_stage_ = std::make_shared<ResizeConvertBatchingStage>(model_, batchsize_, dev_id_, rcop_res_);
      }
      std::shared_ptr<BatchingDoneStage> rc_done_stage =
          std::make_shared<ResizeConvertBatchingDoneStage>(model_, batchsize_, dev_id_, rcop_res_, mlu_input_res_);
      rc_done_stage->SetPerfContext(infer_perf_manager_, infer_thread_id_);
      batching_done_stages_.push_back(rc_done_stage);
    }
  }
  std::shared_ptr<BatchingDoneStage> infer_stage =
      std::make_shared<InferBatchingDoneStage>(model_, batchsize_, dev_id_, mlu_input_res_, mlu_output_res_);
  auto mlu_queue = dynamic_cast<InferBatchingDoneStage*>(infer_stage.get())->SharedMluQueue();
  if (rcop_res_.get()) rcop_res_->SetMluQueue(mlu_queue);  // multiplexing cnrtQueue from EasyInfer.
  std::shared_ptr<BatchingDoneStage> d2h_stage =
      std::make_shared<D2HBatchingDoneStage>(model_, batchsize_, dev_id_, mlu_output_res_, cpu_output_res_);
  infer_stage->SetPerfContext(infer_perf_manager_, infer_thread_id_);
  infer_stage->SetDumpResizedImageDir(dump_resized_image_dir_);
  batching_done_stages_.push_back(infer_stage);
  batching_done_stages_.push_back(d2h_stage);

  if (batching_by_obj_) {
    obj_postproc_stage_ = std::make_shared<ObjPostprocessingBatchingDoneStage>(model_, batchsize_, dev_id_,
                                                                               obj_postprocessor_, cpu_output_res_);
  } else {
    std::shared_ptr<BatchingDoneStage> postproc_stage =
        std::make_shared<PostprocessingBatchingDoneStage>(model_, batchsize_, dev_id_, postprocessor_, cpu_output_res_);
    batching_done_stages_.push_back(postproc_stage);
  }
}

void InferEngine::BatchingDone() {
  if (batching_by_obj_) {
    obj_batching_stage_->Reset();
  } else {
    batching_stage_->Reset();
  }
  if (!batched_finfos_.empty()) {
    for (auto& it : batching_done_stages_) {
      auto tasks = it->BatchingDone(batched_finfos_);
      tp_->SubmitTask(tasks);
    }
    if (batching_by_obj_) {
      auto tasks = obj_postproc_stage_->ObjBatchingDone(batched_finfos_, batched_objs_);
      tp_->SubmitTask(tasks);
      batched_objs_.clear();
    }
    batched_finfos_.clear();
  }
}

}  // namespace cnstream
