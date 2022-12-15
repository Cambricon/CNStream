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

#include <memory>
#include <string>
#include <vector>

#include "cnis/processor.h"
#include "cnstream_frame_va.hpp"
#include "easytrack/include/easy_track.h"
#include "feature_extractor.hpp"
#include "profiler/module_profiler.hpp"
#include "track.hpp"

#include "private/cnstream_param.hpp"

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

namespace cnstream {

int tracker_priority_ = -1;

struct TrackerContext {
  std::unique_ptr<EasyTrack> processer_ = nullptr;
  TrackerContext() = default;
  ~TrackerContext() = default;
  TrackerContext(const TrackerContext &) = delete;
  TrackerContext &operator=(const TrackerContext &) = delete;
};

thread_local std::unique_ptr<FeatureExtractor> g_feature_extractor;

Tracker::Tracker(const std::string &name) : ModuleEx(name) {
  param_register_.SetModuleDesc(
      "Tracker is a module for realtime tracking.");
  param_helper_.reset(new (std::nothrow) ModuleParamsHelper<TrackParams>(name));

  auto model_input_pixel_format_parser = [](const ModuleParamSet& param_set, const std::string& param_name,
                                            const std::string& value, void* result) -> bool {
    if ("RGB24" == value) {
      *(static_cast<InferVideoPixelFmt*>(result)) = infer_server::NetworkInputFormat::RGB;
    } else if ("BGR24" == value) {
      *(static_cast<InferVideoPixelFmt*>(result)) = infer_server::NetworkInputFormat::BGR;
    } else if ("GRAY" == value) {
      *(static_cast<InferVideoPixelFmt*>(result)) = infer_server::NetworkInputFormat::GRAY;
    } else if ("TENSOR" == value) {
      *(static_cast<InferVideoPixelFmt*>(result)) = infer_server::NetworkInputFormat::TENSOR;
    } else {
      LOGE(Inferencer) << "[ModuleParamParser] [" << param_name << "]:" << value << " failed";
      return false;
    }
    return true;
  };

  static const std::vector<ModuleParamDesc> register_param = {
    {"device_id", "0", "Device ordinal number.", PARAM_OPTIONAL, OFFSET(TrackParams, device_id),
      ModuleParamParser<uint32_t>::Parser, "uint32_t"},

    {"model_input_pixel_format", "RGB24", "Optional. The pixel format of the model input image. "
       "RGB24/BGR24/TENSOR are supported. ",
       PARAM_OPTIONAL, OFFSET(TrackParams, input_format), model_input_pixel_format_parser, "InferVideoPixelFmt"},

    {"priority", "0", "Optional. The priority of this infer task in infer server.", PARAM_OPTIONAL,
      OFFSET(TrackParams, priority), ModuleParamParser<uint32_t>::Parser, "uint32_t"},

    {"engine_num", "1",
      "Optional. infer server engine number. Increase the engine number to improve performance. "
      "However, more MLU resources will be used. It is important to choose a proper number. "
      "Usually, it could be set to the core number of the device / the core number of the model.",
      PARAM_OPTIONAL, OFFSET(TrackParams, engine_num), ModuleParamParser<uint32_t>::Parser, "uint32_t"},

    {"batch_timeout", "300", "The batching timeout. unit[ms].", PARAM_OPTIONAL, OFFSET(TrackParams, batch_timeout),
      ModuleParamParser<uint32_t>::Parser, "uint32_t"},

    {"show_stats", "false",
      "Optional. Whether show performance statistics. "
      "1/true/TRUE/True/0/false/FALSE/False these values are accepted.",
      PARAM_OPTIONAL, OFFSET(TrackParams, show_stats), ModuleParamParser<bool>::Parser, "bool"},

    {"model_path", "", "The path of the offline model.", PARAM_OPTIONAL, OFFSET(TrackParams, model_path),
      ModuleParamParser<std::string>::Parser, "string"},

    {"track_name", "FeatureMatch", "Track algorithm name. Choose from FeatureMatch and IoUMatch.",
      PARAM_OPTIONAL, OFFSET(TrackParams, track_name),
      ModuleParamParser<std::string>::Parser, "string"},

    {"max_cosine_distance", "0.2", "Threshold of cosine distance.",
      PARAM_OPTIONAL, OFFSET(TrackParams, max_cosine_distance),
      ModuleParamParser<float>::Parser, "float"}
  };

  param_helper_->Register(register_param, &param_register_);
}

Tracker::~Tracker() { Close(); }

bool Tracker::InitFeatureExtractor(const CNFrameInfoPtr &data) {
  if (!g_feature_extractor) {
    if (!model_) {
      LOGI(TRACK) << "[Track] FeatureExtract model not set, extract feature on CPU";
      g_feature_extractor.reset(new FeatureExtractor(match_func_));
    } else {
      auto params = param_helper_->GetParams();
      if (!infer_server::SetCurrentDevice(params.device_id)) return false;
      g_feature_extractor.reset(new FeatureExtractor(model_, match_func_, params.device_id));
      if (!g_feature_extractor->Init(params.input_format, params.engine_num, params.batch_timeout, params.priority)) {
        LOGE(TRACK) << "[Track] Extract feature on MLU. Init extractor failed.";
        g_feature_extractor.reset();
        return false;
      }
    }
  }
  return true;
}

TrackerContext *Tracker::GetContext(const CNFrameInfoPtr &data) {
  TrackerContext *ctx = nullptr;
  std::unique_lock<std::mutex> guard(mutex_);
  auto search = contexts_.find(data->GetStreamIndex());
  if (search != contexts_.end()) {
    // context exists
    ctx = search->second;
  } else {
    ctx = new TrackerContext;
    auto params = param_helper_->GetParams();
    FeatureMatchTrack *track = new FeatureMatchTrack;
    track->SetParams(params.max_cosine_distance, 100, 0.7, 30, 3);
    ctx->processer_.reset(track);
    contexts_[data->GetStreamIndex()] = ctx;
  }
  return ctx;
}

bool Tracker::Open(ModuleParamSet param_set) {
  if (false == CheckParamSet(param_set)) {
    return false;
  }

  auto params = param_helper_->GetParams();

  if (!params.model_path.empty()) {
    params.model_path = GetPathRelativeToTheJSONFile(params.model_path, param_set);

    if (!params.model_path.empty()) {
      uint32_t dev_cnt = 0;
      if (cnrtGetDeviceCount(&dev_cnt) != cnrtSuccess || params.device_id < 0 ||
          static_cast<uint32_t>(params.device_id) >= dev_cnt) {
        LOGE(TRACK) << "[" << GetName() << "] device " << params.device_id << " does not exist.";
        return false;
      }
      cnrtSetDevice(params.device_id);
      model_ = infer_server::InferServer::LoadModel(params.model_path);
    }
  }

  need_feature_ = (params.track_name == "FeatureMatch");

  match_func_ = [this](const CNFrameInfoPtr data, bool valid) {
    if (!valid) {
      PostEvent(EventType::EVENT_ERROR, "Extract feature failed");
      return;
    }
    CNInferObjsPtr objs_holder = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);

    std::vector<DetectObject> in, out;
    std::unique_lock<std::mutex> guard(objs_holder->mutex_);
    in.reserve(objs_holder->objs_.size());

    // CNDataFramePtr dataframe = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
    for (size_t i = 0; i < objs_holder->objs_.size(); i++) {
      DetectObject obj;
      obj.label = std::stoi(objs_holder->objs_[i]->id);
      obj.score = objs_holder->objs_[i]->score;
      obj.bbox.x = objs_holder->objs_[i]->bbox.x;
      obj.bbox.y = objs_holder->objs_[i]->bbox.y;
      obj.bbox.width = objs_holder->objs_[i]->bbox.w;
      obj.bbox.height = objs_holder->objs_[i]->bbox.h;
      obj.feature = objs_holder->objs_[i]->GetFeature("track");
      in.emplace_back(obj);
    }
    GetContext(data)->processer_->UpdateFrame(in, &out);
    for (size_t i = 0; i < out.size(); i++) {
      objs_holder->objs_[out[i].detect_id]->track_id = std::to_string(out[i].track_id);
    }

    guard.unlock();
    TransmitData(data);
  };

  tracker_priority_ = this->GetPriority();
  return true;
}

void Tracker::Close() {
  for (auto &pair : contexts_) {
    delete pair.second;
  }
  contexts_.clear();
  g_feature_extractor.reset();
}

int Tracker::Process(std::shared_ptr<CNFrameInfo> data) {
  if (!data) {
    LOGE(TRACK) << "Process input data is nulltpr!";
    return -1;
  }
  if (data->IsEos()) {
    if (need_feature_) {
      g_feature_extractor->WaitTaskDone(data->stream_id);
    }
    TransmitData(data);
    return 0;
  }

  if (data->IsRemoved()) {
    return 0;
  }

  if (data->GetStreamIndex() >= GetMaxStreamNumber()) {
    return -1;
  }
  if (need_feature_ && !InitFeatureExtractor(data)) {
    LOGE(TRACK) << "Init Feature Extractor Failed.";
    return -1;
  }

  CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
  bool have_obj = data->collection.HasValue(kCNInferObjsTag);
  if (have_obj) {
    CNInferObjsPtr objs_holder = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
    std::unique_lock<std::mutex> guard(objs_holder->mutex_);
    for (size_t idx = 0; idx < objs_holder->objs_.size(); ++idx) {
      auto &obj = objs_holder->objs_[idx];
      infer_server::CNInferBoundingBox &bbox = obj->bbox;
      bbox.x = CLIP(bbox.x);
      bbox.w = CLIP(bbox.w);
      bbox.y = CLIP(bbox.y);
      bbox.h = CLIP(bbox.h);
      bbox.w = (bbox.x + bbox.w > 1.0) ? (1.0 - bbox.x) : bbox.w;
      bbox.h = (bbox.y + bbox.h > 1.0) ? (1.0 - bbox.y) : bbox.h;
    }
  }

  if (need_feature_) {
    // async extract feature
    if (!g_feature_extractor->ExtractFeature(data)) {
      LOGE(TRACK) << "Extract Feature failed";
      return -1;
    }
  } else {
    match_func_(data, true);
  }

  return 0;
}

bool Tracker::CheckParamSet(const ModuleParamSet& param_set) const {
  if (!param_helper_->ParseParams(param_set)) {
    LOGE(TRACK) << "[" << GetName() << "] parse parameters failed.";
    return false;
  }

  bool ret = true;

  auto params = param_helper_->GetParams();

  ParametersChecker checker;

  if (!checker.CheckPath(params.model_path, param_set)) {
    LOGE(TRACK) << "[Tracker] [model_path] : " << params.model_path << " non-existence.";
    ret = false;
  }

  if (params.track_name != "FeatureMatch" && params.track_name != "IoUMatch") {
    LOGE(TRACK) << "Unsupported track type: " << params.track_name;
    ret = false;
  }

  return ret;
}

}  // namespace cnstream
