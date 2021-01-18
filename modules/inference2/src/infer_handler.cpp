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

#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#include <typeinfo>
#include <condition_variable>

#include "device/mlu_context.h"
#include "infer_handler.hpp"
#include "postproc.hpp"

namespace cnstream {

/**
 * @brief for InferDataObserver, which identify inference data callback from infer_server.
 */
class InferDataObserver : public InferEngineDataObserver {
 public:
  explicit InferDataObserver(InferHandlerImpl* infer_handler, std::shared_ptr<VideoPostproc> postprocessor,
                             std::shared_ptr<infer_server::ModelInfo> model_info, bool with_objs)
      : infer_handler_(infer_handler), postprocessor_(postprocessor), model_info_(model_info), with_objs_(with_objs) {}

  void Notify(InferStatus status, InferPackagePtr result, InferUserData user_data) noexcept override {
    CNFrameInfoPtr data = infer_server::any_cast<CNFrameInfoPtr>(user_data);
    cnstream::CNInferObjsPtr objs_holder = cnstream::GetCNInferObjsPtr(data);
    if (with_objs_) {
      for (auto i = 0u; i < objs_holder->objs_.size(); ++i) {
        postprocessor_->ExecuteInObserverNotify(result->data[i], model_info_, data, objs_holder->objs_[i]);
      }
    } else {
      postprocessor_->ExecuteInObserverNotify(result->data[0], model_info_, data);
    }
    infer_handler_->TransmitData(data);
  }

 private:
  InferHandlerImpl* infer_handler_ = nullptr;
  std::shared_ptr<VideoPostproc> postprocessor_ = nullptr;
  std::shared_ptr<infer_server::ModelInfo> model_info_ = nullptr;
  bool with_objs_ = false;
};

static InferVideoPixelFmt VPixelFmtCast(CNDataFormat fmt) {
  switch (fmt) {
    case CN_PIXEL_FORMAT_YUV420_NV12:
      return InferVideoPixelFmt::NV12;
    case CN_PIXEL_FORMAT_YUV420_NV21:
      return InferVideoPixelFmt::NV21;
    case CN_PIXEL_FORMAT_ARGB32:
      return InferVideoPixelFmt::ARGB;
    case CN_PIXEL_FORMAT_ABGR32:
      return InferVideoPixelFmt::ABGR;
    case CN_PIXEL_FORMAT_RGBA32:
      return InferVideoPixelFmt::RGBA;
    case CN_PIXEL_FORMAT_BGRA32:
      return InferVideoPixelFmt::BGRA;
    default:
      LOGE(INFERENCER2) << "Unsupported video pixel format: " << fmt;
      return InferVideoPixelFmt::NV12;
  }
}

InferHandlerImpl::~InferHandlerImpl() {
  edk::MluContext mlu_ctx;
  mlu_ctx.SetDeviceId(params_.device_id);
  mlu_ctx.BindDevice();
  if (data_observer_) data_observer_.reset();
}

bool InferHandlerImpl::Open() {
  // init cnrt environment
  edk::MluContext mlu_ctx;
  mlu_ctx.SetDeviceId(params_.device_id);
  mlu_ctx.BindDevice();

  return LinkInferServer();
}

void InferHandlerImpl::Close() {
  if (session_) {
    infer_server_->DestroySession(session_);
  }
}

bool InferHandlerImpl::LinkInferServer() {
  // create inference2 engine, load model
  infer_server_.reset(new InferEngine(params_.device_id));
  try {
    model_info_ = infer_server_->LoadModel(params_.model_path, params_.func_name);
  } catch (...) {
    LOGE(INFERENCER2) << "[" << module_->GetName() << "] init offline model failed. model_path: [" << params_.model_path
                     << "].";
    return false;
  }

  // create session between infer_engine
  InferSessionDesc desc;
  desc.name = module_->GetName();
  desc.strategy = params_.batch_strategy;
  desc.batch_timeout = params_.batch_timeout;
  desc.priority = params_.priority;
  desc.model = model_info_;
  desc.show_perf = params_.show_stats;
  desc.engine_num = params_.engine_num;
  desc.host_input_layout = {InferDataType::UINT8, InferDimOrder::NHWC};
  desc.host_output_layout = {InferDataType::FLOAT32, params_.data_order};
  InferVideoPixelFmt dst_format = VPixelFmtCast(params_.model_input_pixel_format);
  // describe preprocessor
  if (params_.preproc_name == "RCOP") {
    scale_platform_ = InferPreprocessType::RESIZE_CONVERT;
    desc.preproc = std::make_shared<InferMluPreprocess>();
    desc.preproc->SetParams("src_format", InferVideoPixelFmt::NV12, "dst_format", dst_format, "preprocess_type",
                            InferPreprocessType::RESIZE_CONVERT, "keep_aspect_ratio", params_.keep_aspect_ratio);
  } else if (params_.preproc_name == "Scaler") {
    scale_platform_ = InferPreprocessType::SCALER;
    desc.preproc = std::make_shared<InferMluPreprocess>();
    desc.preproc->SetParams("src_format", InferVideoPixelFmt::NV12, "dst_format", dst_format, "preprocess_type",
                            InferPreprocessType::SCALER);
  } else {
    scale_platform_ = InferPreprocessType::UNKNOWN;
    desc.preproc = std::make_shared<InferCpuPreprocess>();
    preprocessor_->SetModelInfo(model_info_);
    auto preproc_func = std::bind(&Preproc::ExecuteInferServer, preprocessor_, std::placeholders::_1,
                                  std::placeholders::_2, std::placeholders::_3);
    desc.preproc->SetParams<InferCpuPreprocess::ProcessFunction>("process_function", preproc_func);
  }

  // describe postprocessor
  desc.postproc = std::make_shared<InferPostprocess>();
  auto postproc_func = std::bind(&VideoPostproc::ExecuteByInferServer, postprocessor_, std::placeholders::_1,
                                 std::placeholders::_2, std::placeholders::_3);
  desc.postproc->SetParams<InferPostprocess::ProcessFunction>("process_function", postproc_func);
  data_observer_ = std::make_shared<InferDataObserver>(this, postprocessor_, model_info_, params_.object_infer);
  session_ = infer_server_->CreateSession(desc, data_observer_);

  return session_ != nullptr;
}

int InferHandlerImpl::Process(CNFrameInfoPtr data, bool with_objs) {
  if (nullptr == data || data->IsEos()) return -1;

  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
  InferVideoFrame vframe;
  vframe.plane_num = frame->GetPlanes();
  vframe.format = VPixelFmtCast(frame->fmt);
  vframe.width = frame->width;
  vframe.height = frame->height;

  if (InferPreprocessType::RESIZE_CONVERT == scale_platform_ || InferPreprocessType::SCALER == scale_platform_) {
    for (size_t plane_idx = 0; plane_idx < vframe.plane_num; ++plane_idx) {
      vframe.stride[plane_idx] = frame->stride[plane_idx];
      InferBuffer mlu_buffer(frame->data[plane_idx]->GetMutableMluData(), frame->GetPlaneBytes(plane_idx), nullptr,
                             params_.device_id);
      vframe.plane[plane_idx] = mlu_buffer;
    }
  } else if (InferPreprocessType::UNKNOWN == scale_platform_) {
    for (size_t plane_idx = 0; plane_idx < vframe.plane_num; ++plane_idx) {
      vframe.stride[plane_idx] = frame->stride[plane_idx];
      InferBuffer cpu_buffer(frame->data[plane_idx]->GetMutableCpuData(), frame->GetPlaneBytes(plane_idx), nullptr);
      vframe.plane[plane_idx] = cpu_buffer;
    }
  } else {
    LOGE(INFERENCER2) << "Unsupported scale platform type: " << static_cast<int>(scale_platform_);
    return -1;
  }

  if (!with_objs) {  // for first infer
    infer_server_->Request(session_, std::move(vframe), data->stream_id, data);
    return 0;
  } else {  // for secondary
    auto objs = GetCNInferObjsPtr(data);
    std::vector<infer_server::video::BoundingBox> BoundingBoxVec;
    for (decltype(objs->objs_.size()) i = 0; i < objs->objs_.size(); ++i) {
      infer_server::video::BoundingBox box;
      box.x = objs->objs_[i]->bbox.x;
      box.y = objs->objs_[i]->bbox.y;
      box.w = objs->objs_[i]->bbox.w;
      box.h = objs->objs_[i]->bbox.h;
      BoundingBoxVec.push_back(box);
    }
    infer_server_->Request(session_, std::move(vframe), BoundingBoxVec, data->stream_id, data);
    return 0;
  }

  return -1;
}

void InferHandlerImpl::WaitTaskDone(const std::string& stream_id) {
  if (infer_server_) infer_server_->WaitTaskDone(session_, stream_id);
}

}  // namespace cnstream
