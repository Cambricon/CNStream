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
#include "device/mlu_context.h"
#include "feature_extractor.hpp"
#include "profiler/module_profiler.hpp"
#include "track.hpp"

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

namespace cnstream {

struct TrackerContext {
  std::unique_ptr<edk::EasyTrack> processer_ = nullptr;
  TrackerContext() = default;
  ~TrackerContext() = default;
  TrackerContext(const TrackerContext &) = delete;
  TrackerContext &operator=(const TrackerContext &) = delete;
};

thread_local std::unique_ptr<FeatureExtractor> g_feature_extractor;

Tracker::Tracker(const std::string &name) : Module(name) {
  hasTransmit_.store(true);
  param_register_.SetModuleDesc("Tracker is a module for realtime tracking.");
  param_register_.Register("model_path",
                           "The offline model path. Normally offline model is a file"
                           " with cambricon or model extension.");
  param_register_.Register("func_name",
                           "The offline model function name, usually is 'subnet0'."
                           "Works only if backend is CNRT.");
  param_register_.Register("engine_num", "Infer server engine number.");
  param_register_.Register("track_name", "Track algorithm name. Choose from FeatureMatch and IoUMatch.");
  param_register_.Register("device_id", "Which device will be used. If there is only one device, it might be 0.");
  param_register_.Register("max_cosine_distance", "Threshold of cosine distance.");
}

Tracker::~Tracker() { Close(); }

bool Tracker::InitFeatureExtractor(const CNFrameInfoPtr &data) {
  if (!g_feature_extractor) {
    if (!model_) {
      LOGI(TRACK) << "[Track] FeatureExtract model not set, extract feature on CPU";
      g_feature_extractor.reset(new FeatureExtractor(match_func_));
    } else {
      if (!infer_server::SetCurrentDevice(device_id_)) return false;
      g_feature_extractor.reset(new FeatureExtractor(model_, match_func_, device_id_));
      if (!g_feature_extractor->Init(engine_num_)) {
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
    edk::FeatureMatchTrack *track = new edk::FeatureMatchTrack;
    track->SetParams(max_cosine_distance_, 100, 0.7, 30, 3);
    ctx->processer_.reset(track);
    contexts_[data->GetStreamIndex()] = ctx;
  }
  return ctx;
}

bool Tracker::Open(ModuleParamSet paramSet) {
#ifdef CNIS_USE_MAGICMIND
    if (paramSet.find("model_path") != paramSet.end()) {
      model_pattern1_ = paramSet["model_path"];
      model_pattern1_ = GetPathRelativeToTheJSONFile(model_pattern1_, paramSet);
    }
    if (!model_pattern1_.empty())
      model_ = infer_server::InferServer::LoadModel(model_pattern1_);
#else
    if (paramSet.find("model_path") != paramSet.end()) {
      model_pattern1_ = paramSet["model_path"];
      model_pattern1_ = GetPathRelativeToTheJSONFile(model_pattern1_, paramSet);
    }

    std::string model_pattern2_ = "subnet0";
    if (paramSet.find("func_name") != paramSet.end()) {
      model_pattern2_ = paramSet["func_name"];
    }
    if (!model_pattern1_.empty() && !model_pattern2_.empty())
      model_ = infer_server::InferServer::LoadModel(model_pattern1_, model_pattern2_);
#endif

  if (paramSet.find("max_cosine_distance") != paramSet.end()) {
    max_cosine_distance_ = std::stof(paramSet["max_cosine_distance"]);
  }

  if (paramSet.find("engine_num") != paramSet.end()) {
    engine_num_ = std::stoi(paramSet["engine_num"]);
  }

  if (paramSet.find("device_id") != paramSet.end()) {
    device_id_ = std::stoi(paramSet["device_id"]);
  }

  track_name_ = "FeatureMatch";
  if (paramSet.find("track_name") != paramSet.end()) {
    track_name_ = paramSet["track_name"];
  }

  if (track_name_ != "FeatureMatch" && track_name_ != "IoUMatch") {
    LOGE(TRACK) << "Unsupported track type: " << track_name_;
    return false;
  }
  need_feature_ = (track_name_ == "FeatureMatch");

  match_func_ = [this](const CNFrameInfoPtr data, bool valid) {
    if (!valid) {
      PostEvent(EventType::EVENT_ERROR, "Extract feature failed");
      return;
    }
    CNInferObjsPtr objs_holder = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);

    std::vector<edk::DetectObject> in, out;
    in.reserve(objs_holder->objs_.size());
    for (size_t i = 0; i < objs_holder->objs_.size(); i++) {
      edk::DetectObject obj;
      obj.label = std::stoi(objs_holder->objs_[i]->id);
      obj.score = objs_holder->objs_[i]->score;
      obj.bbox.x = objs_holder->objs_[i]->bbox.x;
      obj.bbox.y = objs_holder->objs_[i]->bbox.y;
      obj.bbox.width = objs_holder->objs_[i]->bbox.w;
      obj.bbox.height = objs_holder->objs_[i]->bbox.h;
      obj.feature = objs_holder->objs_[i]->GetFeature("track");
      in.emplace_back(obj);
    }

    GetContext(data)->processer_->UpdateFrame(edk::TrackFrame(), in, &out);

    for (size_t i = 0; i < out.size(); i++) {
      objs_holder->objs_[out[i].detect_id]->track_id = std::to_string(out[i].track_id);
    }
    TransmitData(data);
  };

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
  if (data->GetStreamIndex() >= GetMaxStreamNumber()) {
    return -1;
  }
  if (need_feature_ && !InitFeatureExtractor(data)) {
    LOGE(TRACK) << "Init Feature Extractor Failed.";
    return -1;
  }

  if (!data->IsEos()) {
    CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
    if (frame->width <= 0 || frame->height <= 0) {
      LOGE(TRACK) << "Frame width and height can not be lower than 0.";
      return -1;
    }
    bool have_obj = data->collection.HasValue(kCNInferObjsTag);
    if (have_obj) {
      CNInferObjsPtr objs_holder = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
      for (size_t idx = 0; idx < objs_holder->objs_.size(); ++idx) {
        auto &obj = objs_holder->objs_[idx];
        cnstream::CNInferBoundingBox &bbox = obj->bbox;
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
  } else {
    if (need_feature_) {
      g_feature_extractor->WaitTaskDone(data->stream_id);
    }
    TransmitData(data);
  }
  return 0;
}

bool Tracker::CheckParamSet(const ModuleParamSet &paramSet) const {
  bool ret = true;
  ParametersChecker checker;
  for (auto &it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOGW(TRACK) << "[Tracker] Unknown param: " << it.first;
    }
  }

  if (paramSet.find("model_path") != paramSet.end()) {
    if (!checker.CheckPath(paramSet.at("model_path"), paramSet)) {
      LOGE(TRACK) << "[Tracker] [model_path] : " << paramSet.at("model_path") << " non-existence.";
      ret = false;
    }
  }

  if (paramSet.find("track_name") != paramSet.end()) {
    std::string track_name = paramSet.at("track_name");
    if (track_name != "FeatureMatch" && track_name != "IoUMatch") {
      LOGE(TRACK) << "[Tracker] [track_name] : Unsupported tracker type " << track_name;
      ret = false;
    }
  }

  std::string err_msg;
  if (paramSet.find("device_id") != paramSet.end()) {
    if (!checker.IsNum({"device_id"}, paramSet, err_msg)) {
      LOGE(TRACK) << "[Tracker] " << err_msg;
      ret = false;
    }
  }

  if (paramSet.find("engine_num") != paramSet.end()) {
    if (!checker.IsNum({"engine_num"}, paramSet, err_msg)) {
      LOGE(TRACK) << "[Tracker] " << err_msg;
      ret = false;
    }
  }

  if (paramSet.find("max_cosine_distance") != paramSet.end()) {
    if (!checker.IsNum({"max_cosine_distance"}, paramSet, err_msg)) {
      LOGE(TRACK) << "[Tracker] " << err_msg;
      ret = false;
    }
  }
  return ret;
}

}  // namespace cnstream
