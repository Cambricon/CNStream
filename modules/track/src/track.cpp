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

#include "easyinfer/mlu_context.h"
#include "feature_extractor.hpp"
#include "track.hpp"

namespace cnstream {

struct TrackerContext {
  std::unique_ptr<edk::EasyTrack> processer_ = nullptr;
  std::unique_ptr<FeatureExtractor> feature_extractor_ = nullptr;
  TrackerContext() = default;
  ~TrackerContext() = default;
  TrackerContext(const TrackerContext &) = delete;
  TrackerContext &operator=(const TrackerContext &) = delete;
};

static thread_local TrackerContext g_tl_ctx;

Tracker::Tracker(const std::string &name) : Module(name) {}

Tracker::~Tracker() { Close(); }

inline TrackerContext *Tracker::GetContext() {
  if (!g_tl_ctx.processer_) {
    edk::MluContext env;
    env.SetDeviceId(device_id_);
    env.ConfigureForThisThread();

    if ("KCF" == track_name_) {
#ifdef ENABLE_KCF
      auto pKcfTrack = new (std::nothrow) edk::KcfTrack;
      LOG_IF(FATAL, nullptr == pKcfTrack) << "Tracker::GetTrackerContext() new edk::KcfTrack failed";
      g_tl_ctx.processer_.reset(pKcfTrack);
      dynamic_cast<edk::KcfTrack *>(g_tl_ctx.processer_.get())->SetModel(model_loader_, device_id_);
#endif
    } else {  // "FeatureMatch by default"
      g_tl_ctx.processer_.reset(new edk::FeatureMatchTrack);
      g_tl_ctx.feature_extractor_.reset(new FeatureExtractor(model_loader_, batch_size_, device_id_));
    }
  }
  return &g_tl_ctx;
}

bool Tracker::Open(ModuleParamSet paramSet) {
  batch_size_ = 1;
  if (paramSet.find("model_path") != paramSet.end()) {
    model_path_ = paramSet["model_path"];
    model_path_ = GetPathRelativeToTheJSONFile(model_path_, paramSet);
  }
  std::string func_name_ = "subnet0";
  if (paramSet.find("func_name") != paramSet.end()) {
    func_name_ = paramSet["func_name"];
  }

  if (paramSet.find("device_id") != paramSet.end()) {
    device_id_ = std::stoi(paramSet["device_id"]);
  }

  if (paramSet.find("track_name") != paramSet.end()) {
    track_name_ = paramSet["track_name"];
    if (track_name_ != "FeatureMatch" && track_name_ != "KCF") {
      LOG(ERROR) << "Unsupported tracker type " << track_name_;
      return false;
    }
  } else {
    track_name_ = "FeatureMatch";
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
    model_loader_ = nullptr;
  }
  if (g_tl_ctx.feature_extractor_) {
    g_tl_ctx.feature_extractor_.reset();
    g_tl_ctx.feature_extractor_ = nullptr;
  }
  if (g_tl_ctx.processer_) {
    g_tl_ctx.processer_.reset();
    g_tl_ctx.processer_ = nullptr;
  }
}

int Tracker::Process(std::shared_ptr<CNFrameInfo> data) {
  if (data->frame.width <= 0 || data->frame.height <= 0) return -1;
  if (data->frame.width >= 4096 || data->frame.height >= 2160) return -1;
  for (auto idx = data->objs.begin(); idx != data->objs.end(); idx++) {
    cnstream::CNInferBoundingBox &bbox = (*idx)->bbox;
    bbox.x = CLIP(bbox.x);
    bbox.w = CLIP(bbox.w);
    bbox.y = CLIP(bbox.y);
    bbox.h = CLIP(bbox.h);
    bbox.w = (bbox.x + bbox.w > 1.0) ? (1.0 - bbox.x) : bbox.w;
    bbox.h = (bbox.y + bbox.h > 1.0) ? (1.0 - bbox.y) : bbox.h;
  }
  TrackerContext *ctx = GetContext();
  if (nullptr == ctx || nullptr == ctx->processer_) {
    throw TrackerError("Get Tracker Context Failed.");
  }

  if (track_name_ == "FeatureMatch") {
    std::vector<std::vector<float>> features;
    ctx->feature_extractor_->ExtractFeature(data, data->objs, &features);

    std::vector<edk::DetectObject> in, out;
    for (size_t i = 0; i < data->objs.size(); i++) {
      edk::DetectObject obj;
      obj.label = std::stoi(data->objs[i]->id);
      obj.score = data->objs[i]->score;
      obj.bbox.x = CLIP(data->objs[i]->bbox.x);
      obj.bbox.y = CLIP(data->objs[i]->bbox.y);
      obj.bbox.width = CLIP(data->objs[i]->bbox.w);
      obj.bbox.height = CLIP(data->objs[i]->bbox.h);
      obj.feature = features[i];
      in.push_back(obj);
    }

    edk::TrackFrame tframe;
    ctx->processer_->UpdateFrame(tframe, in, &out);

    data->objs.clear();
    for (size_t i = 0; i < out.size(); i++) {
      CNInferObjectPtr obj = std::make_shared<CNInferObject>();
      obj->id = std::to_string(out[i].label);
      obj->track_id = std::to_string(out[i].track_id);
      obj->score = out[i].score;
      obj->bbox.x = CLIP(out[i].bbox.x);
      obj->bbox.y = CLIP(out[i].bbox.y);
      obj->bbox.w = CLIP(out[i].bbox.width);
      obj->bbox.h = CLIP(out[i].bbox.height);
      obj->bbox.w = (obj->bbox.w + obj->bbox.x) > 1 ? 1 - obj->bbox.x : obj->bbox.w;
      obj->bbox.h = (obj->bbox.h + obj->bbox.y) > 1 ? 1 - obj->bbox.y : obj->bbox.h;
      data->objs.push_back(obj);
    }
  } else if (track_name_ == "KCF") {
#ifdef ENABLE_KCF
    if (data->frame.fmt != CN_PIXEL_FORMAT_YUV420_NV21)
      throw "KCF Only support frame in CN_PIXEL_FORMAT_YUV420_NV21 format";
    std::vector<edk::DetectObject> in, out;
    for (size_t i = 0; i < data->objs.size(); i++) {
      edk::DetectObject obj;
      obj.label = std::stoi(data->objs[i]->id);
      obj.score = data->objs[i]->score;
      obj.bbox.x = data->objs[i]->bbox.x;
      obj.bbox.y = data->objs[i]->bbox.y;
      obj.bbox.width = data->objs[i]->bbox.w;
      obj.bbox.height = data->objs[i]->bbox.h;
      in.push_back(obj);
    }

    edk::TrackFrame tframe;
    tframe.data = data->frame.data[0]->GetMutableMluData();
    tframe.width = data->frame.width;
    tframe.height = data->frame.height;
    tframe.format = edk::TrackFrame::ColorSpace::NV21;
    tframe.frame_id = data->frame.frame_id;
    tframe.dev_type = edk::TrackFrame::DevType::MLU;
    tframe.device_id = data->frame.ctx.dev_id;
    ctx->processer_->UpdateFrame(tframe, in, &out);

    data->objs.clear();
    for (size_t i = 0; i < out.size(); i++) {
      std::shared_ptr<CNInferObject> obj = std::make_shared<CNInferObject>();
      obj->id = std::to_string(out[i].label);
      obj->track_id = std::to_string(out[i].track_id);
      obj->score = out[i].score;
      obj->bbox.x = out[i].bbox.x;
      obj->bbox.y = out[i].bbox.y;
      obj->bbox.w = out[i].bbox.width;
      obj->bbox.h = out[i].bbox.height;
      data->objs.push_back(obj);
    }
#endif
  }
  return 0;
}

}  // namespace cnstream

