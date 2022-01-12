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

  // fix paths
  if (!infer_params_.model_path.empty())
    infer_params_.model_path = GetPathRelativeToTheJSONFile(infer_params_.model_path, raw_params);

  // check preprocess
  if (!infer_server::SetCurrentDevice(params.device_id)) return false;

  if (params.preproc_name.empty()) {
    LOGE(INFERENCER2) << "Preproc name can't be empty string. Please set preproc_name.";
    return false;
  }
  std::shared_ptr<VideoPreproc> pre_processor = nullptr;
  if (params.preproc_name != "RCOP" && params.preproc_name != "SCALER" && params.preproc_name != "CNCV") {
    pre_processor.reset(VideoPreproc::Create(params.preproc_name));
    if (!pre_processor) {
      LOGE(INFERENCER2) << "Can not find VideoPreproc implemention by name: " << params.preproc_name;
      return false;
    }
    if (!pre_processor->Init(params.custom_preproc_params)) {
      LOGE(INFERENCER2) << "VideoPreprocessor init failed.";
      return false;
    }
    pre_processor->SetModelInputPixelFormat(params.model_input_pixel_format);
  }

  std::shared_ptr<VideoPostproc> post_processor = nullptr;
  if (params.postproc_name.empty()) {
    LOGE(INFERENCER2) << "Postproc name can't be empty string. Please set postproc_name.";
    return false;
  }
  post_processor.reset(VideoPostproc::Create(params.postproc_name));
  if (!post_processor) {
    LOGE(INFERENCER2) << "Can not find VideoPostproc implemention by name: " << params.postproc_name;
    return false;
  }
  if (!post_processor->Init(params.custom_postproc_params)) {
    LOGE(INFERENCER2) << "Postprocessor init failed.";
    return false;
  }
  post_processor->SetThreshold(params.threshold);

  std::shared_ptr<FrameFilter> frame_filter = nullptr;
  if (!params.frame_filter_name.empty()) {
    frame_filter.reset(FrameFilter::Create(params.frame_filter_name));
    if (!frame_filter) {
      LOGE(INFERENCER2) << "Can not find FrameFilter implemention by name: " << params.frame_filter_name;
      return false;
    }
  }
  std::shared_ptr<ObjFilter> obj_filter = nullptr;
  if (!params.obj_filter_name.empty()) {
    obj_filter.reset(ObjFilter::Create(params.obj_filter_name));
    if (!obj_filter) {
      LOGE(INFERENCER2) << "Can not find ObjFilter implemention by name: " << params.obj_filter_name;
      return false;
    }
  }

  infer_handler_ =
      std::make_shared<InferHandlerImpl>(this, infer_params_, post_processor, pre_processor, frame_filter, obj_filter);
  return infer_handler_->Open();
}

void Inferencer2::Close() { infer_handler_.reset(); }

int Inferencer2::Process(std::shared_ptr<CNFrameInfo> data) {
  if (!data) {
    LOGE(INFERENCE2) << "Process inputdata is nulltpr!";
    return -1;
  }

  if (!data->IsEos()) {
    CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
    if (frame->dst_device_id < 0) {
      /* CNSyncedMemory data is on CPU */
      for (int i = 0; i < frame->GetPlanes(); i++) {
        frame->data[i]->SetMluDevContext(infer_params_.device_id);
      }
      frame->dst_device_id = infer_params_.device_id;
    } else if (static_cast<uint32_t>(frame->dst_device_id) != infer_params_.device_id &&
               frame->ctx.dev_type == DevContext::DevType::MLU) {
      /* CNSyncedMemory data is on different MLU from the data this module needed, and SOURCE data is on MLU*/
      frame->CopyToSyncMemOnDevice(infer_params_.device_id);
      frame->dst_device_id = infer_params_.device_id;
    } else if (static_cast<uint32_t>(frame->dst_device_id) != infer_params_.device_id &&
               frame->ctx.dev_type == DevContext::DevType::CPU) {
      /* CNSyncedMemory data is on different MLU from the data this module needed, and SOURCE data is on CPU*/
      void *dst = frame->cpu_data.get();
      for (int i = 0; i < frame->GetPlanes(); i++) {
        size_t plane_size = frame->GetPlaneBytes(i);
        frame->data[i].reset(new CNSyncedMemory(plane_size));
        frame->data[i]->SetCpuData(dst);
        dst = reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(dst) + plane_size);
        frame->data[i]->SetMluDevContext(infer_params_.device_id);
      }
      frame->dst_device_id = infer_params_.device_id;  // set dst_device_id to param_.device_id
    }
    if (infer_handler_->Process(data, infer_params_.object_infer) != 0) {
      return -1;
    }
  } else {
    infer_handler_->WaitTaskDone(data->stream_id);
    TransmitData(data);
  }

  return 0;
}

Inferencer2::~Inferencer2() {
  Close();
}

bool Inferencer2::CheckParamSet(const ModuleParamSet& param_set) const {
  Infer2Param params;
  return param_manager_->ParseBy(param_set, &params);
}

}  // namespace cnstream
