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
  Infer2Param params;
  if (!param_manager_->ParseBy(raw_params, &params)) {
    LOGE(INFERENCER2) << "[" << GetName() << "] parse parameters failed.";
    return false;
  }

  infer_params_ = params;

  // check preprocess
  std::shared_ptr<VideoPreproc> pre_processor = nullptr;
  edk::MluContext mlu_ctx;
  mlu_ctx.SetDeviceId(params.device_id);
  mlu_ctx.BindDevice();

  std::string model_path = GetPathRelativeToTheJSONFile(params.model_path, raw_params);
  if (FILE* file = fopen(model_path.c_str(), "r")) {
    fclose(file);
  } else {
    LOGE(INFERENCER2) << "Model path is wrong. Wrong path is [ " << model_path << " ], please check it.";
    return false;
  }

  if (params.preproc_name.empty()) {
    LOGE(INFERENCER2) << "Preproc name can't be empty string. Please set preproc_name.";
    return false;
  }
  if (params.preproc_name != "RCOP" && params.preproc_name != "SCALER") {
    pre_processor = std::shared_ptr<VideoPreproc>(VideoPreproc::Create(params.preproc_name));
    if (!pre_processor) {
      LOGE(INFERENCER2) << "Can not find VideoPreproc implemention by name: " << params.preproc_name;
      return false;
    }
    pre_processor->SetModelInputPixelFormat(params.model_input_pixel_format);
  }

  std::shared_ptr<VideoPostproc> post_processor = nullptr;
  if (params.postproc_name.empty()) {
    LOGE(INFERENCER2) << "Postproc name can't be empty string. Please set postproc_name.";
    return false;
  }
  post_processor = std::shared_ptr<VideoPostproc>(VideoPostproc::Create(params.postproc_name));
  if (!post_processor) {
    LOGE(INFERENCER2) << "Can not find Postproc implemention by name: " << params.postproc_name;
    return false;
  }
  post_processor->SetThreshold(params.threshold);

  infer_handler_ = std::make_shared<InferHandlerImpl>(this, infer_params_, post_processor, pre_processor);
  return infer_handler_->Open();
}

void Inferencer2::Close() { infer_handler_.reset(); }

int Inferencer2::Process(std::shared_ptr<CNFrameInfo> data) {
  if (!data) return -1;

  if (!data->IsEos()) {
    CNDataFramePtr frame = GetCNDataFramePtr(data);
    if (frame->dst_device_id < 0) {  /* origin data is on Cpu */
      for (int i = 0; i < frame->GetPlanes(); i++) {
        frame->data[i]->SetMluDevContext(infer_params_.device_id);
      }
      frame->dst_device_id = infer_params_.device_id;
    } else if ((unsigned)frame->dst_device_id != infer_params_.device_id) {  /* origin data is on another device */
      frame->CopyToSyncMemOnDevice(infer_params_.device_id);
      frame->dst_device_id = infer_params_.device_id;
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
  Close();
}

bool Inferencer2::CheckParamSet(const ModuleParamSet& param_set) const {
  Infer2Param params;
  return param_manager_->ParseBy(param_set, &params);
}

}  // namespace cnstream
