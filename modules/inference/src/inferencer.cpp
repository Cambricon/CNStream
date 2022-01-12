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

#include <device/mlu_context.h>
#include <easyinfer/model_loader.h>

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "infer_engine.hpp"
#include "infer_trans_data_helper.hpp"
#include "obj_filter.hpp"
#include "postproc.hpp"
#include "preproc.hpp"

#include "cnstream_frame_va.hpp"
#include "inferencer.hpp"
#include "infer_params.hpp"

#include "profiler/module_profiler.hpp"

namespace cnstream {

struct PerfStats;

struct InferContext {
  std::shared_ptr<InferEngine> engine;
  std::shared_ptr<InferTransDataHelper> trans_data_helper;
  int drop_count = 0;
};  // struct InferContext

using InferContextSptr = std::shared_ptr<InferContext>;

class InferencerPrivate {
 public:
  explicit InferencerPrivate(Inferencer* q) : q_ptr_(q) {}
  InferParams params_;
  std::shared_ptr<edk::ModelLoader> model_loader_;
  std::shared_ptr<Preproc> preproc_ = nullptr;
  std::shared_ptr<Postproc> postproc_ = nullptr;

  std::shared_ptr<ObjPreproc> obj_preproc_ = nullptr;
  std::shared_ptr<ObjPostproc> obj_postproc_ = nullptr;
  std::shared_ptr<ObjFilter> obj_filter_ = nullptr;
  int bsize_ = 0;
  std::string dump_resized_image_dir_ = "";
  std::string module_name_ = "";

  std::map<std::thread::id, InferContextSptr> ctxs_;
  std::mutex ctx_mtx_;

  void InferEngineErrorHnadleFunc(const std::string& err_msg) {
    LOGE(INFERENCER) << err_msg;
  }

  bool InitByParams(const InferParams &params, const ModuleParamSet &param_set) {
    params_ = params;
    module_name_ = q_ptr_->GetName();
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(params.device_id);
    mlu_ctx.BindDevice();

    std::string model_path = GetPathRelativeToTheJSONFile(params.model_path, param_set);
    try {
      auto model_loader = std::make_shared<edk::ModelLoader>(model_path, params.func_name);

      for (uint32_t index = 0; index < model_loader->OutputNum(); ++index) {
        edk::DataLayout layout;
        layout.dtype = edk::DataType::FLOAT32;
        layout.order = params.data_order;
        model_loader->SetCpuOutputLayout(layout, index);
      }

      bsize_ = model_loader->InputShape(0).N();
      model_loader_ = model_loader;
    } catch (edk::Exception &e) {
      LOGE(INFERENCER) << "[" << q_ptr_->GetName() << "] init offline model failed. model_path: ["
                 << model_path << "]. error message: [" << e.what() << "]";
      return false;
    }

    if (params.object_infer) {
      LOGI(INFERENCER) << "[" << q_ptr_->GetName() << "] inference mode: inference with objects.";
      if (!params.obj_filter_name.empty()) {
        obj_filter_ = std::shared_ptr<ObjFilter>(ObjFilter::Create(params.obj_filter_name));
        if (obj_filter_) {
          LOGI(INFERENCER) << "[" << q_ptr_->GetName() << "] Object filter set:" << params.obj_filter_name;
        } else {
          LOGE(INFERENCER) << "Can not find ObjFilter implemention by name: "
                     << params.obj_filter_name;
          return false;
        }
      }
    }

    if (!params.preproc_name.empty()) {
      if (params.object_infer) {
        obj_preproc_ = std::shared_ptr<ObjPreproc>(ObjPreproc::Create(params.preproc_name));
        if (!obj_preproc_) {
          LOGE(INFERENCER) << "Can not find ObjPreproc implemention by name: " << params.preproc_name;
          return false;
        }
        if (!obj_preproc_->Init(params.custom_preproc_params)) {
          LOGE(INFERENCER) << "Preprocessor init failed.";
          return false;
        }
      } else {
        preproc_ = std::shared_ptr<Preproc>(Preproc::Create(params.preproc_name));
        if (!preproc_) {
          LOGE(INFERENCER) << "Can not find Preproc implemention by name: " << params.preproc_name;
          return false;
        }
        if (!preproc_->Init(params.custom_preproc_params)) {
          LOGE(INFERENCER) << "Preprocessor init failed.";
          return false;
        }
      }
    }

    if (!params.postproc_name.empty()) {
      if (params.object_infer) {
        obj_postproc_ = std::shared_ptr<ObjPostproc>(ObjPostproc::Create(params.postproc_name));
        if (!obj_postproc_) {
          LOGE(INFERENCER) << "Can not find ObjPostproc implemention by name: " << params.postproc_name;
          return false;
        }
        if (!obj_postproc_->Init(params.custom_postproc_params)) {
          LOGE(INFERENCER) << "Postprocessor init failed.";
          return false;
        }
        obj_postproc_->SetThreshold(params.threshold);
      } else {
        postproc_ = std::shared_ptr<Postproc>(Postproc::Create(params.postproc_name));
        if (!postproc_) {
          LOGE(INFERENCER) << "Can not find Postproc implemention by name: " << params.postproc_name;
          return false;
        }
        if (!postproc_->Init(params.custom_postproc_params)) {
          LOGE(INFERENCER) << "Postprocessor init failed.";
          return false;
        }
        postproc_->SetThreshold(params.threshold);
      }
    }

    if (!params.dump_resized_image_dir.empty()) {
      dump_resized_image_dir_ = GetPathRelativeToTheJSONFile(params.dump_resized_image_dir, param_set);
    }

    return true;
  }

  InferContextSptr GetInferContext() {
    std::thread::id tid = std::this_thread::get_id();
    InferContextSptr ctx(nullptr);
    std::lock_guard<std::mutex> lk(ctx_mtx_);
    if (ctxs_.find(tid) != ctxs_.end()) {
      ctx = ctxs_[tid];
    } else {
      ctx = std::make_shared<InferContext>();
      std::stringstream ss;
      ss << tid;
      std::string thread_id_str = ss.str();
      thread_id_str.erase(0, thread_id_str.length() - 9);
      std::string tid_str = "th_" + thread_id_str;
      ctx->engine = std::make_shared<InferEngine>(
          params_.device_id, model_loader_, preproc_, postproc_, bsize_, params_.batching_timeout, params_.use_scaler,
          tid_str, std::bind(&InferencerPrivate::InferEngineErrorHnadleFunc, this, std::placeholders::_1),
          params_.keep_aspect_ratio, params_.object_infer, obj_preproc_, obj_postproc_, obj_filter_,
          dump_resized_image_dir_, params_.model_input_pixel_format, params_.mem_on_mlu_for_postproc,
          params_.saving_infer_input, module_name_, q_ptr_->GetProfiler(), params_.pad_method);
      ctx->trans_data_helper = std::make_shared<InferTransDataHelper>(q_ptr_, params_.infer_interval * bsize_ * 2);
      ctxs_[tid] = ctx;
    }
    return ctx;
  }

 private:
  DECLARE_PUBLIC(q_ptr_, Inferencer);
};  // class InferencerPrivate

Inferencer::Inferencer(const std::string& name) : Module(name) {
  d_ptr_ = nullptr;
  hasTransmit_.store(1);  // transmit data by module itself
  param_register_.SetModuleDesc(
      "Inferencer is a module for running offline model inference,"
      " as well as preprocessing and postprocessing.");
  param_manager_ = new (std::nothrow) InferParamManager();
  LOGF_IF(INFERENCER, !param_manager_) << "Inferencer::Inferencer(const std::string& name) new InferParams failed.";
  param_manager_->RegisterAll(&param_register_);
}

Inferencer::~Inferencer() {
  if (param_manager_) delete param_manager_;
}

bool Inferencer::Open(ModuleParamSet raw_params) {
  if (d_ptr_) {
    Close();
  }
  d_ptr_ = new (std::nothrow) InferencerPrivate(this);
  if (!d_ptr_) {
    LOGE(INFERENCER) << "Inferencer::Open() new InferencerPrivate failed";
    return false;
  }

  InferParams params;
  if (!param_manager_->ParseBy(raw_params, &params)) {
    LOGE(INFERENCER) << "[" << GetName() << "] parse parameters failed.";
    return false;
  }

  if (!d_ptr_->InitByParams(params, raw_params)) {
    LOGE(INFERENCER) << "[" << GetName() << "] init resources failed.";
    return false;
  }

  if (container_ == nullptr) {
    LOGI(INFERENCER) << name_ << " has not been added into pipeline.";
  } else {
    if (GetProfiler()) {
      if (!params.use_scaler && params.preproc_name.empty()) {
        GetProfiler()->RegisterProcessName("RESIZE CONVERT");
      }
      GetProfiler()->RegisterProcessName("RUN MODEL");
    }
  }

  return true;
}

void Inferencer::Close() {
  if (nullptr == d_ptr_) return;

  /*destroy infer contexts*/
  d_ptr_->ctx_mtx_.lock();
  d_ptr_->ctxs_.clear();
  d_ptr_->ctx_mtx_.unlock();

  delete d_ptr_;
  d_ptr_ = nullptr;
}

int Inferencer::Process(CNFrameInfoPtr data) {
  std::shared_ptr<InferContext> pctx = d_ptr_->GetInferContext();
  bool eos = data->IsEos();
  bool drop_data = d_ptr_->params_.infer_interval > 0 && pctx->drop_count++ % d_ptr_->params_.infer_interval != 0;

  if (!eos) {
    if (data->IsRemoved()) {
      // discard packets from removed-stream
      return 0;
    }
    CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
    if (frame->dst_device_id < 0) {
      /* CNSyncedMemory data is on CPU */
      for (int i = 0; i < frame->GetPlanes(); i++) {
        frame->data[i]->SetMluDevContext(d_ptr_->params_.device_id, 0);
      }
      frame->dst_device_id = d_ptr_->params_.device_id;
    } else if (static_cast<uint32_t>(frame->dst_device_id) != d_ptr_->params_.device_id &&
               frame->ctx.dev_type == DevContext::DevType::MLU) {
      /* CNSyncedMemory data is on different MLU from the data this module needed, and SOURCE data is on MLU*/
      frame->CopyToSyncMemOnDevice(d_ptr_->params_.device_id);
      frame->dst_device_id = d_ptr_->params_.device_id;
    } else if (static_cast<uint32_t>(frame->dst_device_id) != d_ptr_->params_.device_id &&
               frame->ctx.dev_type == DevContext::DevType::CPU) {
      /* CNSyncedMemory data is on different MLU from the data this module needed, and SOURCE data is on CPU*/
      void *dst = frame->cpu_data.get();
      for (int i = 0; i < frame->GetPlanes(); i++) {
        size_t plane_size = frame->GetPlaneBytes(i);
        frame->data[i].reset(new CNSyncedMemory(plane_size));
        frame->data[i]->SetCpuData(dst);
        dst = reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(dst) + plane_size);
        frame->data[i]->SetMluDevContext(d_ptr_->params_.device_id, 0);
      }
      frame->dst_device_id = d_ptr_->params_.device_id;  // set dst_device_id to param_.device_id
    }
  }

  if (eos || drop_data) {
    if (eos && IsStreamRemoved(data->stream_id)) {
      // minimize batch_timeout delay
      pctx->engine->ForceBatchingDone();
    }
    if (drop_data) pctx->drop_count %= d_ptr_->params_.infer_interval;
    std::shared_ptr<std::promise<void>> promise = std::make_shared<std::promise<void>>();
    promise->set_value();
    InferEngine::ResultWaitingCard card(promise);
    pctx->trans_data_helper->SubmitData(std::make_pair(data, card));
  } else {
    InferEngine::ResultWaitingCard card = pctx->engine->FeedData(data);
    pctx->trans_data_helper->SubmitData(std::make_pair(data, card));
  }

  return 1;
}

bool Inferencer::CheckParamSet(const ModuleParamSet &param_set) const {
  InferParams params;
  return param_manager_->ParseBy(param_set, &params);
}

}  // namespace cnstream
