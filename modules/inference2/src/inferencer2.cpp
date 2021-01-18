/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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
#include "inferencer2.hpp"

#include <iostream>
#include <memory>
#include <string>

#include "device/mlu_context.h"
#include "infer_handler.hpp"
#include "infer_params.hpp"
#include "postproc.hpp"

namespace cnstream {

void InferHandler::TransmitData(const CNFrameInfoPtr& data) {
  if (module_) module_->TransmitData(data);
}

Inferencer2::Inferencer2(const std::string& name) : Module(name) {
  hasTransmit_.store(true);
  param_register_.SetModuleDesc("Inferencer2 is a module for running offline model inference, preprocessing and "
                                "postprocessing based on infer_server.");
  param_manager_ = std::make_shared<Infer2ParamManager>();
  LOGF_IF(INFERENCER2, !param_manager_) << "Inferencer::Inferencer(const std::string& name) new InferParams failed.";
  param_manager_->RegisterAll(&param_register_);
}

bool Inferencer2::Open(ModuleParamSet raw_params) {
  Infer2Params params;
  if (!param_manager_->ParseBy(raw_params, &params)) {
    LOGE(INFERENCER2) << "[" << GetName() << "] parse parameters failed.";
    return false;
  }

  // InferParamerters infer_params;
  infer_params_.model_path = cnstream::GetPathRelativeToTheJSONFile(params.model_path, raw_params);
  infer_params_.device_id = params.device_id;
  infer_params_.show_stats = params.show_stats;
  infer_params_.priority = 0;
  infer_params_.batch_timeout = params.batching_timeout;
  infer_params_.keep_aspect_ratio = params.keep_aspect_ratio;
  infer_params_.model_input_pixel_format = params.model_input_pixel_format;
  infer_params_.func_name = params.func_name;
  infer_params_.preproc_name = params.preproc_name;
  infer_params_.engine_num = params.engine_num;
  infer_params_.object_infer = params.object_infer;
  if (params.data_order == edk::DimOrder::NCHW) {
    infer_params_.data_order = InferDimOrder::NCHW;
  } else {
    infer_params_.data_order = InferDimOrder::NHWC;
  }

  if (params.batch_strategy == "dynamic") {
    infer_params_.batch_strategy = InferBatchStrategy::DYNAMIC;
  } else if (params.batch_strategy == "static") {
    infer_params_.batch_strategy = InferBatchStrategy::STATIC;
  }

  // check preprocess
  std::shared_ptr<Preproc> pre_processor = nullptr;
  edk::MluContext mlu_ctx;
  mlu_ctx.SetDeviceId(params.device_id);
  mlu_ctx.BindDevice();
  if (params.preproc_name.empty()) {
    if (edk::CoreVersion::MLU220 == mlu_ctx.GetCoreVersion()) {
      infer_params_.preproc_name = "SCALER";
    } else if (edk::CoreVersion::MLU270 == mlu_ctx.GetCoreVersion()) {
      infer_params_.preproc_name = "RCOP";
    }
  } else if (params.preproc_name != "RCOP" && params.preproc_name != "SCALER") {
    pre_processor = std::shared_ptr<Preproc>(Preproc::Create(params.preproc_name));
    if (!pre_processor) {
      LOGE(INFERENCER2) << "Can not find Preproc implemention by name: " << params.preproc_name;
      return false;
    }
  }

  std::shared_ptr<VideoPostproc> post_processor = nullptr;
  if (!params.postproc_name.empty()) {
    post_processor = std::shared_ptr<VideoPostproc>(VideoPostproc::Create(params.postproc_name));
    if (!post_processor) {
      LOGE(INFERENCER2) << "Can not find Postproc implemention by name: " << params.postproc_name;
      return false;
    }
    post_processor->SetThreshold(params.threshold);
  } else {
    LOGE(INFERENCER2) << "Postproc name can't be nullptr, check it!";
    return false;
  }

  infer_handler_ = std::make_shared<InferHandlerImpl>(this, infer_params_, post_processor, pre_processor);
  return infer_handler_->Open();
}

void Inferencer2::Close() {
  if (infer_handler_) infer_handler_->Close();
}

int Inferencer2::Process(std::shared_ptr<CNFrameInfo> data) {
  if (!data) return -1;

  if (!data->IsEos()) {
    CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
    if (static_cast<uint32_t>(frame->data[0]->GetMluDevId()) != infer_params_.device_id &&
        frame->data[0]->GetHead() == CNSyncedMemory::SyncedHead::HEAD_AT_MLU) {
      frame->CopyToSyncMemOnDevice(infer_params_.device_id);
    } else if (frame->data[0]->GetHead() == CNSyncedMemory::SyncedHead::HEAD_AT_CPU) {
      for (int i = 0; i < frame->GetPlanes(); i++) {
        frame->data[i]->SetMluDevContext(infer_params_.device_id);
      }
    }
    infer_handler_->Process(data, infer_params_.object_infer);
  } else {
    infer_handler_->WaitTaskDone(data->stream_id);
    TransmitData(data);
  }

  return 0;
}

Inferencer2::~Inferencer2() {
  edk::MluContext mlu_ctx;
  mlu_ctx.SetDeviceId(infer_params_.device_id);
  mlu_ctx.BindDevice();
  param_manager_.reset();
  infer_handler_.reset();
}

bool Inferencer2::CheckParamSet(const ModuleParamSet& param_set) const {
  Infer2Params params;
  return param_manager_->ParseBy(param_set, &params);
}

}  // namespace cnstream
