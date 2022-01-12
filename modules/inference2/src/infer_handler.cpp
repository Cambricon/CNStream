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
#include <algorithm>

#include "device/mlu_context.h"
#include "infer_handler.hpp"
#include "postproc.hpp"

namespace cnstream {

/**
 * @brief for InferDataObserver, which identify inference data callback from infer_server.
 */
class InferDataObserver : public InferEngineDataObserver {
 public:
  explicit InferDataObserver(InferHandlerImpl* infer_handler)
      : infer_handler_(infer_handler) {}

  void Response(InferStatus status, InferPackagePtr result, InferUserData user_data) noexcept override {
    if (status != InferStatus::SUCCESS) {
      infer_handler_->PostEvent(EventType::EVENT_ERROR, "Process inference failed");
    }
    CNFrameInfoPtr data = infer_server::any_cast<CNFrameInfoPtr>(user_data);
    infer_handler_->TransmitData(data);
  }

 private:
  InferHandlerImpl* infer_handler_ = nullptr;
};

static InferVideoPixelFmt VPixelFmtCast(CNDataFormat fmt) {
  switch (fmt) {
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
      return InferVideoPixelFmt::NV12;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
      return InferVideoPixelFmt::NV21;
    case CNDataFormat::CN_PIXEL_FORMAT_ARGB32:
      return InferVideoPixelFmt::ARGB;
    case CNDataFormat::CN_PIXEL_FORMAT_ABGR32:
      return InferVideoPixelFmt::ABGR;
    case CNDataFormat::CN_PIXEL_FORMAT_RGBA32:
      return InferVideoPixelFmt::RGBA;
    case CNDataFormat::CN_PIXEL_FORMAT_BGRA32:
      return InferVideoPixelFmt::BGRA;
    default:
      LOGE(INFERENCER2) << "Unsupported video pixel format: " << static_cast<int>(fmt);
      return InferVideoPixelFmt::NV12;
  }
}

InferHandlerImpl::~InferHandlerImpl() {
  Close();
}

bool InferHandlerImpl::Open() {
  // init cnrt environment
  if (!infer_server::SetCurrentDevice(params_.device_id)) return false;
  return LinkInferServer();
}

void InferHandlerImpl::Close() {
  infer_server::SetCurrentDevice(params_.device_id);
  if (session_) {
    infer_server_->DestroySession(session_);
    session_ = nullptr;
  }
}

inline int GetChannelNum(InferVideoPixelFmt pix_fmt) {
  switch (pix_fmt) {
    case InferVideoPixelFmt::RGB24:
    case InferVideoPixelFmt::BGR24:
      return 3;
    case InferVideoPixelFmt::RGBA:
    case InferVideoPixelFmt::BGRA:
    case InferVideoPixelFmt::ARGB:
    case InferVideoPixelFmt::ABGR:
      return 4;
    default:
      return 0;
  }
}

bool InferHandlerImpl::LinkInferServer() {
  // create inference2 engine, load model
  infer_server_.reset(new InferEngine(params_.device_id));
  std::shared_ptr<infer_server::ModelInfo> model_info{nullptr};
  std::string backend = infer_server::Predictor::Backend();
  if (backend == "cnrt") {
    if (params_.model_path.empty()) {
      LOGE(INFERENCER2) << "[" << module_->GetName() << "] init offline model failed, "
                      << "no valid model path.";
      return false;
    }
    model_info = infer_server_->LoadModel(params_.model_path, params_.func_name);
  } else if (backend == "magicmind") {
    if (params_.model_path.empty()) {
      LOGE(INFERENCER2) << "[" << module_->GetName() << "] init offline model failed, "
                      << "no valid model path.";
      return false;
    }
    model_info = infer_server_->LoadModel(params_.model_path);
  } else {
    LOGF(INFERENCER2) << "[" << module_->GetName() << "] backend not supported" << backend;
  }
  if (!model_info) {
    LOGE(INFERENCER2) << "[" << module_->GetName() << "] init offline model failed,"
                      << "create model failed.";
    return false;
  }
  int c = 3;
  switch (model_info->InputLayout(0).order) {
    case InferDimOrder::NHWC:
      c = model_info->InputShape(0)[3];
      break;
    case InferDimOrder::NCHW:
      c = model_info->InputShape(0)[1];
      break;
    default:
      LOGE(INFERENCER2) << "[" << module_->GetName() << "] dim order not supported";
      return false;
  }
  if (GetChannelNum(params_.model_input_pixel_format) != c) {
      LOGE(INFERENCER2) << "[" << module_->GetName() << "] model_input_pixel_format is wrong! model input shape: "
                        << model_info->InputShape(0);
      return false;
  }

  InferSessionDesc desc;
  desc.name = module_->GetName();
  desc.strategy = params_.batch_strategy;
  desc.batch_timeout = params_.batching_timeout;
  desc.priority = params_.priority;
  desc.model = model_info;
  desc.show_perf = params_.show_stats;
  desc.engine_num = params_.engine_num;
  if (model_info->InputLayout(0).dtype == InferDataType::UINT8) {
    desc.host_input_layout = {InferDataType::UINT8, InferDimOrder::NHWC};
  } else {
    desc.host_input_layout = {InferDataType::FLOAT32, InferDimOrder::NHWC};
  }
  desc.host_output_layout = {InferDataType::FLOAT32, params_.data_order};
  InferVideoPixelFmt dst_format = params_.model_input_pixel_format;
  // preprocess
  if (params_.preproc_name == "RCOP") {
    scale_platform_ = InferPreprocessType::RESIZE_CONVERT;
    desc.preproc = std::make_shared<InferMluPreprocess>();
    desc.preproc->SetParams("dst_format", dst_format, "preprocess_type", InferPreprocessType::RESIZE_CONVERT,
                            "keep_aspect_ratio", params_.keep_aspect_ratio);
  } else if (params_.preproc_name == "SCALER") {
    scale_platform_ = InferPreprocessType::SCALER;
    desc.preproc = std::make_shared<InferMluPreprocess>();
    desc.preproc->SetParams("dst_format", dst_format, "preprocess_type", InferPreprocessType::SCALER,
                            "keep_aspect_ratio", params_.keep_aspect_ratio);
  } else if (params_.preproc_name == "CNCV") {
    scale_platform_ = InferPreprocessType::CNCV_PREPROC;
    desc.preproc = std::make_shared<InferMluPreprocess>();
    desc.preproc->SetParams("dst_format", dst_format, "preprocess_type", InferPreprocessType::CNCV_PREPROC,
                            "keep_aspect_ratio", params_.keep_aspect_ratio, "mean", params_.mean_,
                            "std", params_.std_, "normalize", params_.normalize);
  } else {
    scale_platform_ = InferPreprocessType::UNKNOWN;
    desc.preproc = std::make_shared<InferCpuPreprocess>();
    auto preproc_func = std::bind(&VideoPreproc::Execute, preprocessor_, std::placeholders::_1,
                                  std::placeholders::_2, std::placeholders::_3);
    desc.preproc->SetParams<InferCpuPreprocess::ProcessFunction>("process_function", preproc_func);
  }
  // postprocess
  desc.postproc = std::make_shared<InferPostprocess>();
  auto postproc_func = std::bind(&VideoPostproc::Execute, postprocessor_, std::placeholders::_1,
                                 std::placeholders::_2, std::placeholders::_3);
  desc.postproc->SetParams<InferPostprocess::ProcessFunction>("process_function", postproc_func);
  // observer
  data_observer_ = std::make_shared<InferDataObserver>(this);
  // create session
  session_ = infer_server_->CreateSession(desc, data_observer_);

  bool ret = session_ != nullptr;

  if (!ret) {
    LOGE(INFERENCER2) << "[" << module_->GetName() << "] [infer_server_] create session failed!";
  }

  return session_ != nullptr;
}

int InferHandlerImpl::Process(CNFrameInfoPtr data, bool with_objs) {
  if (nullptr == data || data->IsEos()) return -1;
  CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);

  thread_local uint32_t drop_cnt = params_.infer_interval - 1;
  bool drop_data = params_.infer_interval > 0 && drop_cnt++ != params_.infer_interval - 1;

  if (!drop_data && frame_filter_ && !frame_filter_->Filter(data)) {
    drop_data = true;
    drop_cnt = 0;
  }

  if (drop_data) {
    // to keep data in sequence, we pass empty package to infer_server, with CNFrameInfo as user data.
    // frame won't be inferred, and CNFrameInfo will be responsed in sequence
    infer_server::PackagePtr in = infer_server::Package::Create(0, data->stream_id);
    if (!infer_server_->Request(session_, std::move(in), data)) {
      LOGE(INFERENCER2) << "[" << module_->GetName() << "]"<< " Request sending data to infer server failed."
                        << " stream id: " << data->stream_id << " frame id: " << frame->frame_id;
      return -1;
    }
    return 0;
  }
  drop_cnt = 0;

  InferVideoFrame vframe;
  vframe.plane_num = frame->GetPlanes();
  vframe.format = VPixelFmtCast(frame->fmt);
  vframe.width = frame->width;
  vframe.height = frame->height;

  if (InferPreprocessType::RESIZE_CONVERT == scale_platform_ || InferPreprocessType::SCALER == scale_platform_ ||
      InferPreprocessType::CNCV_PREPROC == scale_platform_) {
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

  if (!with_objs) {
    infer_server::PackagePtr in = infer_server::Package::Create(1, data->stream_id);
    in->data[0]->Set(std::move(vframe));
    in->data[0]->SetUserData(data);
    if (!infer_server_->Request(session_, std::move(in), data)) {
      LOGE(INFERENCER2) << "[" << module_->GetName() << "]"<< " Request sending data to infer server failed."
                        << " stream id: " << data->stream_id << " frame id: " << frame->frame_id;
      return -1;
    }
  } else {  /* secondary inference */
    auto objs = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
    infer_server::PackagePtr in = infer_server::Package::Create(0, data->stream_id);
    in->data.reserve(objs->objs_.size());
    for (auto& obj : objs->objs_) {
      if (obj_filter_ && !obj_filter_->Filter(data, obj)) continue;
      InferVideoFrame tmp_frame = vframe;
      infer_server::video::BoundingBox& box = tmp_frame.roi;
      box.x = obj->bbox.x;
      box.y = obj->bbox.y;
      box.w = obj->bbox.w;
      box.h = obj->bbox.h;
      auto* tmp = new infer_server::InferData;
      tmp->Set(std::move(tmp_frame));
      tmp->SetUserData(obj);
      in->data.emplace_back(tmp);
    }
    if (!infer_server_->Request(session_, std::move(in), data)) {
      LOGE(INFERENCER2) << "[" << module_->GetName() << "]"<< " Request sending data to infer server failed."
                        << " stream id: " << data->stream_id << " frame id: " << frame->frame_id;
      return -1;
    }
  }
  return 0;
}

void InferHandlerImpl::WaitTaskDone(const std::string& stream_id) {
  if (infer_server_) infer_server_->WaitTaskDone(session_, stream_id);
}

}  // namespace cnstream
