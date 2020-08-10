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
#include <unordered_map>
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
#include "perf_calculator.hpp"
#include "perf_manager.hpp"

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

  std::shared_ptr<PerfManager> infer_perf_manager_ = nullptr;
  std::unordered_map<std::string, std::shared_ptr<PerfCalculator>> perf_calculator_;
  std::thread cal_perf_th_;
  std::atomic<bool> perf_th_running_{false};

  std::map<std::thread::id, InferContextSptr> ctxs_;
  std::mutex ctx_mtx_;

  void InferEngineErrorHnadleFunc(const std::string& err_msg) {
    LOG(FATAL) << err_msg;
    q_ptr_->PostEvent(EVENT_ERROR, err_msg);
  }

  bool InitByParams(const InferParams &params, const ModuleParamSet &param_set) {
    params_ = params;
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(params.device_id);
    mlu_ctx.ConfigureForThisThread();

    std::string model_path = GetPathRelativeToTheJSONFile(params.model_path, param_set);
    try {
      auto model_loader = std::make_shared<edk::ModelLoader>(model_path, params.func_name);

      for (uint32_t index = 0; index < model_loader->OutputNum(); ++index) {
        edk::DataLayout layout;
        layout.dtype = edk::DataType::FLOAT32;
        layout.order = params.data_order;
        model_loader->SetCpuOutputLayout(layout, index);
      }

      model_loader->InitLayout();
      bsize_ = model_loader->InputShapes()[0].n;
      model_loader_ = model_loader;
    } catch (edk::Exception &e) {
      LOG(ERROR) << "[" << q_ptr_->GetName() << "] init offline model failed. model_path: ["
                 << model_path << "]. error message: [" << e.what() << "]";
      return false;
    }

    if (params.object_infer) {
      LOG(INFO) << "[" << q_ptr_->GetName() << "] inference mode: inference with objects.";
      if (!params.obj_filter_name.empty()) {
        obj_filter_ = std::shared_ptr<ObjFilter>(ObjFilter::Create(params.obj_filter_name));
        if (obj_filter_) {
          LOG(INFO) << "[" << q_ptr_->GetName() << "] Object filter set:" << params.obj_filter_name;
        } else {
          LOG(ERROR) << "Can not find ObjFilter implemention by name: "
                     << params.obj_filter_name;
          return false;
        }
      }
    }

    if (!params.preproc_name.empty()) {
      if (params.object_infer) {
        obj_preproc_ = std::shared_ptr<ObjPreproc>(ObjPreproc::Create(params.preproc_name));
        if (!obj_preproc_) {
          LOG(ERROR) << "Can not find ObjPreproc implemention by name: " << params.preproc_name;
          return false;
        }
      } else {
        preproc_ = std::shared_ptr<Preproc>(Preproc::Create(params.preproc_name));
        if (!preproc_) {
          LOG(ERROR) << "Can not find Preproc implemention by name: " << params.preproc_name;
          return false;
        }
      }
    }

    if (!params.postproc_name.empty()) {
      if (params.object_infer) {
        obj_postproc_ = std::shared_ptr<ObjPostproc>(ObjPostproc::Create(params.postproc_name));
        if (!obj_postproc_) {
          LOG(ERROR) << "Can not find ObjPostproc implemention by name: " << params.postproc_name;
          return false;
        }
        obj_postproc_->SetThreshold(params.threshold);
      } else {
        postproc_ = std::shared_ptr<Postproc>(Postproc::Create(params.postproc_name));
        if (!postproc_) {
          LOG(ERROR) << "Can not find Postproc implemention by name: " << params.postproc_name;
          return false;
        }
        postproc_->SetThreshold(params.threshold);
      }
    }

    if (!params.dump_resized_image_dir.empty()) {
      dump_resized_image_dir_ = GetPathRelativeToTheJSONFile(params.dump_resized_image_dir, param_set);
    }

    if (params.show_stats) {
      infer_perf_manager_ = std::make_shared<PerfManager>();

      std::string stats_db_path = "perf_database/" + q_ptr_->GetName() + ".db";
      if (!params.stats_db_name.empty()) {
        stats_db_path = GetPathRelativeToTheJSONFile(params.stats_db_name, param_set);
      }
      LOG(INFO) << "[" << q_ptr_->GetName() << "] save performance info database file to : " << stats_db_path;
      if (!infer_perf_manager_->Init(stats_db_path)) {
        LOG(ERROR) << "[" << q_ptr_->GetName() << "] Init infer perf manager failed.";
        return false;
      }

      infer_perf_manager_->SqlBeginTrans();
      std::shared_ptr<PerfUtils> perf_utils = std::make_shared<PerfUtils>();
      perf_utils->AddSql(q_ptr_->GetName(), infer_perf_manager_->GetSql());

      perf_calculator_["rsz_cvt"] = std::make_shared<PerfCalculatorForInfer>();
      perf_calculator_["infer"] = std::make_shared<PerfCalculatorForInfer>();
      perf_calculator_["rsz_cvt_batch"] = std::make_shared<PerfCalculatorForInfer>();

      for (auto it : perf_calculator_) {
        if (!it.second->SetPerfUtils(perf_utils)) {
          LOG(ERROR) << "Set perf utils failed.";
          return false;
        }
      }

      perf_th_running_.store(true);
      cal_perf_th_ = std::thread(&InferencerPrivate::CalcPerf, this);
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
          params_.device_id,
          model_loader_,
          preproc_,
          postproc_,
          bsize_,
          params_.batching_timeout,
          params_.use_scaler,
          infer_perf_manager_,
          tid_str,
          std::bind(&InferencerPrivate::InferEngineErrorHnadleFunc, this, std::placeholders::_1),
          params_.keep_aspect_ratio,
          params_.object_infer,
          obj_preproc_,
          obj_postproc_,
          obj_filter_,
          dump_resized_image_dir_,
          params_.model_input_pixel_format,
          params_.mem_on_mlu_for_postproc);
      ctx->trans_data_helper = std::make_shared<InferTransDataHelper>(q_ptr_, bsize_);
      ctxs_[tid] = ctx;
      if (infer_perf_manager_) {
        infer_perf_manager_->RegisterPerfType(
            tid_str, PerfManager::GetPrimaryKey(),
            {"resize_start_time", "resize_end_time", "resize_cnt", "infer_start_time", "infer_end_time", "infer_cnt"});
      }
    }
    return ctx;
  }

  void CalcPerf();
  void PrintPerf();
  void PrintPerf(std::string name, std::vector<std::string> keys);

 private:
  DECLARE_PUBLIC(q_ptr_, Inferencer);
};  // class InferencerPrivate

void InferencerPrivate::PrintPerf(std::string name, std::vector<std::string> keys) {
  if (perf_calculator_.find(name) == perf_calculator_.end()) {
    LOG(ERROR) << "[Inferencer] [" << q_ptr_->GetName() << "] Can not find perf calculator " << name << std::endl;
    return;
  }
  std::shared_ptr<PerfCalculator> calculator = perf_calculator_[name];
  std::shared_ptr<PerfUtils> perf_utils = calculator->GetPerfUtils();
  std::vector<std::string> thread_ids = perf_utils->GetTableNames(q_ptr_->GetName());

  std::vector<std::pair<std::string, PerfStats>> latest_fps, entire_fps, latency_vec;
  std::vector<uint32_t> latest_frame_cnt_digit, entire_frame_cnt_digit, latency_frame_cnt_digit;
  for (auto thread_id : thread_ids) {
    PerfStats fps_stats = calculator->CalcThroughput(q_ptr_->GetName(), thread_id, keys);
    PerfStats latency_stats = calculator->CalcLatency(q_ptr_->GetName(), thread_id, keys);
    PerfStats avg_fps = calculator->GetAvgThroughput(q_ptr_->GetName(), thread_id);

    if (name == "rsz_cvt_batch") {
      latency_stats.frame_cnt *= bsize_;
      fps_stats.frame_cnt *= bsize_;
      fps_stats.fps *= bsize_;
      avg_fps.fps *= bsize_;
      avg_fps.frame_cnt *= bsize_;
    }

    latency_vec.push_back(std::make_pair(thread_id, latency_stats));
    latest_fps.push_back(std::make_pair(thread_id, fps_stats));
    entire_fps.push_back(std::make_pair(thread_id, avg_fps));

    latency_frame_cnt_digit.push_back(std::to_string(latency_stats.frame_cnt).length());
    latest_frame_cnt_digit.push_back(std::to_string(fps_stats.frame_cnt).length());
    entire_frame_cnt_digit.push_back(std::to_string(avg_fps.frame_cnt).length());
  }

  PerfStats total_stats = calculator->CalcThroughput(q_ptr_->GetName(), "", keys);
  PerfStats total_avg_fps = calculator->GetAvgThroughput(q_ptr_->GetName(), "");

  if (name == "rsz_cvt_batch") {
    total_stats.fps *= bsize_;
    total_stats.frame_cnt *= bsize_;
    total_avg_fps.fps *= bsize_;
    total_avg_fps.frame_cnt *= bsize_;
  }

  uint32_t max_digit = PerfUtils::Max(latency_frame_cnt_digit);
  for (auto &it : latency_vec) {
    PrintStr(it.first);
    PrintLatency(it.second, max_digit);
  }
  PrintTitleForLatestThroughput();
  max_digit = PerfUtils::Max(latest_frame_cnt_digit);
  for (auto &it : latest_fps) {
    PrintStr(it.first);
    PrintThroughput(it.second, max_digit);
  }
  PrintTitleForTotal();
  PrintThroughput(total_stats);

  PrintTitleForAverageThroughput();
  max_digit = PerfUtils::Max(entire_frame_cnt_digit);
  for (auto &it : entire_fps) {
    PrintStr(it.first);
    PrintThroughput(it.second, max_digit);
  }
  PrintTitleForTotal();
  PrintThroughput(total_avg_fps);
}

void InferencerPrivate::PrintPerf() {
  std::cout << "\033[1;35m" << "\n\n#################################################"
            << "#################################################\n" << "\033[0m" << std::endl;
  PrintStr("Inferencer performance.    Module name : " + q_ptr_->GetName());
  PrintTitle("resize and convert (theoretical)");
  PrintPerf("rsz_cvt_batch", {"resize_start_time", "resize_end_time"});
  PrintTitle("resize and convert (realistic)");
  PrintPerf("rsz_cvt", {"resize_start_time", "resize_end_time", "resize_cnt"});
  PrintTitle("run inference");
  PrintPerf("infer", {"infer_start_time", "infer_end_time", "infer_cnt"});
}

void InferencerPrivate::CalcPerf() {
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  while (perf_th_running_) {
    PrintPerf();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    infer_perf_manager_->SqlCommitTrans();
    infer_perf_manager_->SqlBeginTrans();
  }
  infer_perf_manager_->SqlCommitTrans();
  PrintPerf();
}

Inferencer::Inferencer(const std::string& name) : Module(name) {
  d_ptr_ = nullptr;
  hasTransmit_.store(1);  // transmit data by module itself
  param_register_.SetModuleDesc(
      "Inferencer is a module for running offline model inference,"
      " as well as preprocedding and postprocessing.");
  param_manager_ = new (std::nothrow) InferParamManager();
  LOG_IF(FATAL, !param_manager_) << "Inferencer::Inferencer(const std::string& name) new InferParams failed.";
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
    LOG(ERROR) << "Inferencer::Open() new InferencerPrivate failed";
    return false;
  }

  InferParams params;
  if (!param_manager_->ParseBy(raw_params, &params)) {
    LOG(ERROR) << "[" << GetName() << "] parse parameters failed.";
    return false;
  }

  if (!d_ptr_->InitByParams(params, raw_params)) {
    LOG(ERROR) << "[" << GetName() << "] init resources failed.";
    return false;
  }

  if (container_ == nullptr) {
    LOG(INFO) << name_ << " has not been added into pipeline.";
  } else {
  }

  return true;
}

void Inferencer::Close() {
  if (nullptr == d_ptr_) return;

  d_ptr_->perf_th_running_.store(false);
  if (d_ptr_->cal_perf_th_.joinable()) d_ptr_->cal_perf_th_.join();

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
    CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
    if (static_cast<uint32_t>(frame->ctx.dev_id) != d_ptr_->params_.device_id &&
        frame->ctx.dev_type == DevContext::MLU) {
      frame->CopyToSyncMemOnDevice(d_ptr_->params_.device_id);
    } else if (frame->ctx.dev_type == DevContext::CPU) {
      for (int i = 0; i < frame->GetPlanes(); i++) {
        frame->data[i]->SetMluDevContext(d_ptr_->params_.device_id, 0);
      }
    }
  }

  if (eos || drop_data) {
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
