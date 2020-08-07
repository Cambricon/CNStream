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

#include "cnstream_frame_va.hpp"
#include "device/mlu_context.h"
#include "feature_extractor.hpp"
#include "track.hpp"

namespace cnstream {

struct TrackerContext {
  std::unique_ptr<edk::EasyTrack> processer_ = nullptr;
  TrackerContext() = default;
  ~TrackerContext() = default;
  TrackerContext(const TrackerContext &) = delete;
  TrackerContext &operator=(const TrackerContext &) = delete;
};

static thread_local std::unique_ptr<edk::MluContext> g_tl_mlu_env;
static thread_local std::unique_ptr<FeatureExtractor> g_tl_feature_extractor;

Tracker::Tracker(const std::string &name) : Module(name) {
  param_register_.SetModuleDesc("Tracker is a module for realtime tracking.");
  param_register_.Register("model_path",
                           "The offline model path. Normally offline model is a file"
                           " with cambricon extension.");
  param_register_.Register("func_name", "The offline model function name, usually is 'subnet0'.");
  param_register_.Register("track_name", "Track algorithm name. Choose from FeatureMatch and KCF.");
  param_register_.Register("device_id", "Which device will be used. If there is only one device, it might be 0.");
  param_register_.Register("max_cosine_distance", "Threshold of cosine distance.");
}

Tracker::~Tracker() { Close(); }

TrackerContext *Tracker::GetContext(CNFrameInfoPtr data) {
  if (!g_tl_mlu_env) {
    g_tl_mlu_env.reset(new edk::MluContext);
    g_tl_mlu_env->SetDeviceId(device_id_);
    g_tl_mlu_env->ConfigureForThisThread();
  }
  if (!g_tl_feature_extractor) {
    if (!model_loader_) {
      LOG(INFO) << "[FeatureExtractor] model not set, extract feature on CPU";
      g_tl_feature_extractor.reset(new FeatureExtractor());
    } else {
      g_tl_feature_extractor.reset(new FeatureExtractor(model_loader_, device_id_));
    }
  }

  TrackerContext *ctx = nullptr;
  std::unique_lock<std::mutex> guard(mutex_);
  auto search = contexts_.find(data->GetStreamIndex());
  if (data->GetStreamIndex() >= GetMaxStreamNumber()) {
    return nullptr;
  }
  if (search != contexts_.end()) {
    // context exists
    ctx = search->second;
  } else {
    ctx = new TrackerContext;
    if ("KCF" == track_name_) {
#ifdef ENABLE_KCF
      ctx->processer_.reset(new edk::KcfTrack);
      dynamic_cast<edk::KcfTrack *>(ctx->processer_.get())->SetModel(model_loader_, device_id_);
      contexts_[data->GetStreamIndex()] = ctx;
#endif
    } else {  // "FeatureMatch by default"
      edk::FeatureMatchTrack *track = new edk::FeatureMatchTrack;
      track->SetParams(max_cosine_distance_, 100, 0.7, 30, 3);
      ctx->processer_.reset(track);
      contexts_[data->GetStreamIndex()] = ctx;
    }
  }
  return ctx;
}

bool Tracker::Open(ModuleParamSet paramSet) {
  if (paramSet.find("model_path") != paramSet.end()) {
    model_path_ = paramSet["model_path"];
    model_path_ = GetPathRelativeToTheJSONFile(model_path_, paramSet);
  }

  std::string func_name_ = "subnet0";
  if (paramSet.find("func_name") != paramSet.end()) {
    func_name_ = paramSet["func_name"];
  }

  if (paramSet.find("max_cosine_distance") != paramSet.end()) {
    max_cosine_distance_ = std::stof(paramSet["max_cosine_distance"]);
  }

  if (paramSet.find("device_id") != paramSet.end()) {
    device_id_ = std::stoi(paramSet["device_id"]);
  }

  track_name_ = "FeatureMatch";
  if (paramSet.find("track_name") != paramSet.end()) {
    track_name_ = paramSet["track_name"];
  }

  if (!model_path_.empty()) {
    try {
      model_loader_ = std::make_shared<edk::ModelLoader>(model_path_, func_name_);
    } catch (edk::Exception &e) {
      LOG(ERROR) << e.what();
      return false;
    }
  }
  return true;
}

void Tracker::Close() {
  if (model_loader_) {
    model_loader_.reset();
  }
  if (g_tl_feature_extractor) {
    g_tl_feature_extractor.reset();
  }
  if (g_tl_mlu_env) {
    g_tl_mlu_env.reset();
  }
  for (auto &pair : contexts_) {
    delete pair.second;
  }
  contexts_.clear();
}

int Tracker::Process(std::shared_ptr<CNFrameInfo> data) {
  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
  if (frame->width <= 0 || frame->height <= 0) {
    LOG(ERROR) << "Frame width and height can not be lower than 0.";
    return -1;
  }

  if (data->datas.find(CNObjsVecKey) == data->datas.end()) {
    return 0;
  }

  CNObjsVec objs = cnstream::any_cast<CNObjsVec>(data->datas[CNObjsVecKey]);
  for (size_t idx = 0; idx < objs.size(); ++idx) {
    auto& obj = objs[idx];
    cnstream::CNInferBoundingBox &bbox = obj->bbox;
    bbox.x = CLIP(bbox.x);
    bbox.w = CLIP(bbox.w);
    bbox.y = CLIP(bbox.y);
    bbox.h = CLIP(bbox.h);
    bbox.w = (bbox.x + bbox.w > 1.0) ? (1.0 - bbox.x) : bbox.w;
    bbox.h = (bbox.y + bbox.h > 1.0) ? (1.0 - bbox.y) : bbox.h;
  }
  TrackerContext *ctx = GetContext(data);
  if (nullptr == ctx || nullptr == ctx->processer_) {
    LOG(ERROR) << "Get Tracker Context Failed.";
    return -1;
  }

  if (track_name_ == "FeatureMatch") {
    std::vector<std::vector<float>> features;
    g_tl_feature_extractor->ExtractFeature(*frame->ImageBGR(), objs, &features);

    std::vector<edk::DetectObject> in, out;
    for (size_t i = 0; i < objs.size(); i++) {
      edk::DetectObject obj;
      obj.label = std::stoi(objs[i]->id);
      obj.score = objs[i]->score;
      obj.bbox.x = objs[i]->bbox.x;
      obj.bbox.y = objs[i]->bbox.y;
      obj.bbox.width = objs[i]->bbox.w;
      obj.bbox.height = objs[i]->bbox.h;
      obj.feature = features[i];
      in.push_back(obj);
    }

    edk::TrackFrame tframe;
    ctx->processer_->UpdateFrame(tframe, in, &out);

    for (size_t i = 0; i < out.size(); i++) {
      objs[out[i].detect_id]->track_id = std::to_string(out[i].track_id);
    }
  } else if (track_name_ == "KCF") {
#ifdef ENABLE_KCF
    if (frame->fmt != CN_PIXEL_FORMAT_YUV420_NV21) {
      LOG(ERROR) << "KCF Only support frame in CN_PIXEL_FORMAT_YUV420_NV21 format.";
      return -1;
    }
    std::vector<edk::DetectObject> in, out;
    for (size_t i = 0; i < objs.size(); i++) {
      edk::DetectObject obj;
      obj.label = std::stoi(objs[i]->id);
      obj.score = objs[i]->score;
      obj.bbox.x = objs[i]->bbox.x;
      obj.bbox.y = objs[i]->bbox.y;
      obj.bbox.width = objs[i]->bbox.w;
      obj.bbox.height = objs[i]->bbox.h;
      in.push_back(obj);
    }

    edk::TrackFrame tframe;
    tframe.data = frame->data[0]->GetMutableMluData();
    tframe.width = frame->width;
    tframe.height = frame->height;
    tframe.format = edk::TrackFrame::ColorSpace::NV21;
    tframe.frame_id = frame->frame_id;
    tframe.dev_type = edk::TrackFrame::DevType::MLU;
    tframe.device_id = frame->ctx.dev_id;
    ctx->processer_->UpdateFrame(tframe, in, &out);

    objs.clear();
    for (size_t i = 0; i < out.size(); i++) {
      std::shared_ptr<CNInferObject> obj = std::make_shared<CNInferObject>();
      obj->id = std::to_string(out[i].label);
      obj->track_id = std::to_string(out[i].track_id);
      obj->score = out[i].score;
      obj->bbox.x = out[i].bbox.x;
      obj->bbox.y = out[i].bbox.y;
      obj->bbox.w = out[i].bbox.width;
      obj->bbox.h = out[i].bbox.height;
      objs.push_back(obj);
    }
    data->datas[CNObjsVecKey] = objs;
#endif
  }
  return 0;
}

bool Tracker::CheckParamSet(const ModuleParamSet &paramSet) const {
  bool ret = true;
  ParametersChecker checker;
  for (auto &it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOG(WARNING) << "[Tracker] Unknown param: " << it.first;
    }
  }

  if (paramSet.find("model_path") != paramSet.end()) {
    if (!checker.CheckPath(paramSet.at("model_path"), paramSet)) {
      LOG(ERROR) << "[Tracker] [model_path] : " << paramSet.at("model_path") << " non-existence.";
      ret = false;
    }
  }

  if (paramSet.find("track_name") != paramSet.end()) {
    std::string track_name = paramSet.at("track_name");
    if (track_name != "FeatureMatch" && track_name != "KCF") {
      LOG(ERROR) << "[Tracker] [track_name] : Unsupported tracker type " << track_name;
      ret = false;
    }
  }

  std::string err_msg;
  if (paramSet.find("device_id") != paramSet.end()) {
    if (!checker.IsNum({"device_id"}, paramSet, err_msg)) {
      LOG(ERROR) << "[Tracker] " << err_msg;
      ret = false;
    }
  }

  if (paramSet.find("max_cosine_distance") != paramSet.end()) {
    if (!checker.IsNum({"max_cosine_distance"}, paramSet, err_msg)) {
      LOG(ERROR) << "[Tracker] " << err_msg;
      ret = false;
    }
  }
  return ret;
}

}  // namespace cnstream
