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

#include "track.hpp"

#include <opencv2/opencv.hpp>

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "feature_extractor.h"

namespace cnstream {

/**************************************************************************
 * @brief Tracker thread context
 *************************************************************************/
struct TrackerContext {
  std::unique_ptr<edk::EasyTrack> processer_ = nullptr;
  std::unique_ptr<FeatureExtractor> feature_extractor_ = nullptr;
  TrackerContext() = default;
  ~TrackerContext() = default;
  TrackerContext(const TrackerContext &) = delete;
  TrackerContext &operator=(const TrackerContext &) = delete;
};

Tracker::Tracker(const std::string &name) : Module(name) {
  param_register_.SetModuleDesc("Tracker is a module for realtime tracking.");
  param_register_.Register("model_path", "The offline model path.");
  param_register_.Register("func_name", "The offline model func name.");
  param_register_.Register("track_name", "Track type, must be FeatureMatch or KCF.");
}

Tracker::~Tracker() { Close(); }

inline TrackerContext *Tracker::GetTrackerContext(CNFrameInfoPtr data) {
  std::unique_lock<std::mutex> lock(tracker_mutex_);
  TrackerContext *ctx = nullptr;
  auto it = tracker_ctxs_.find(data->frame.stream_id);
  if (it != tracker_ctxs_.end()) {
    ctx = it->second;
  } else {
    ctx = new TrackerContext;
    tracker_ctxs_[data->frame.stream_id] = ctx;
    if (!ctx->processer_) {
      if ("KCF" == track_name_) {
        assert(nullptr != pKCFloader_);
        auto pKcfTrack = new edk::KcfTrack;
        pKcfTrack->SetModel(pKCFloader_);
        ctx->processer_.reset(new edk::KcfTrack);
        if (!ctx->processer_) return nullptr;
      } else {  // "FeatureMatch by default"
        ctx->processer_.reset(new edk::FeatureMatchTrack);
        if (!ctx->processer_) return nullptr;

        ctx->feature_extractor_.reset(new FeatureExtractor);
#ifdef CNS_MLU100
        if (!ctx->feature_extractor_->Init(model_path_, func_name_)) {
          LOG(ERROR) << "FeatureMatchTrack Feature Extractor initial failed.";
          return nullptr;
        }
#endif
      }
    }
  }
  return ctx;
}

bool Tracker::Open(cnstream::ModuleParamSet paramSet) {
  if (paramSet.find("model_path") != paramSet.end() && paramSet.find("func_name") != paramSet.end()) {
    model_path_ = paramSet["model_path"];
    model_path_ = GetPathRelativeToTheJSONFile(model_path_, paramSet);
    func_name_ = paramSet.find("func_name")->second;
  } else {
    model_path_ = "";
    func_name_ = "";
  }

  if (paramSet.find("track_name") != paramSet.end()) {
    track_name_ = paramSet.find("track_name")->second;
    if (track_name_ != "FeatureMatch" && track_name_ != "KCF") {
      LOG(ERROR) << "Unsupported tracker type " << track_name_;
      return false;
    }
    if (track_name_ == "KCF") {
      try {
        pKCFloader_ = std::make_shared<edk::ModelLoader>(model_path_, func_name_);
      } catch (edk::Exception &e) {
        LOG(ERROR) << e.what();
        return false;
      }
    } else {
      track_name_ = "FeatureMatch";
    }
  } else {
    track_name_ = "FeatureMatch";
  }

  return true;
}

void Tracker::Close() {
  std::unique_lock<std::mutex> lock(tracker_mutex_);
  for (auto &pair : tracker_ctxs_) {
    delete pair.second;
  }
  tracker_ctxs_.clear();
}

int Tracker::Process(std::shared_ptr<CNFrameInfo> data) {
  TrackerContext *ctx = GetTrackerContext(data);
  if (nullptr == ctx || nullptr == ctx->processer_) {
    throw TrackerError("Get Tracker Context Failed.");
    return -1;
  }

  if (track_name_ == "FeatureMatch") {
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

#ifdef HAVE_OPENCV
    cv::Mat img = *data->frame.ImageBGR();

    edk::TrackFrame tframe;
    tframe.data = img.data;
    tframe.width = img.cols;
    tframe.height = img.rows;
    tframe.format = edk::TrackFrame::ColorSpace::RGB24;
    tframe.dev_type = edk::TrackFrame::DevType::CPU;

    for (auto &obj : in) {
      obj.feature = ctx->feature_extractor_->ExtractFeature(tframe, obj);
    }

    ctx->processer_->UpdateFrame(tframe, in, &out);
#else
#error OpenCV required
#endif

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
  } else if (track_name_ == "KCF") {
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
  }

  return 0;
}

bool Tracker::CheckParamSet(ModuleParamSet paramSet) {
  ParametersChecker checker;
  for (auto &it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOG(WARNING) << "[Tracker] Unknown param: " << it.first;
    }
  }

  if (paramSet.find("model_path") == paramSet.end() || paramSet.find("func_name") == paramSet.end()) {
    LOG(ERROR) << "[Tracker] must specify [model_path], [func_name].";
    return false;
  }

  if (!checker.CheckPath(paramSet["model_path"], paramSet)) {
    LOG(ERROR) << "[Tracker] [model_path] : " << paramSet["model_path"] << " non-existence.";
    return false;
  }

  if (paramSet.find("track_name") != paramSet.end()) {
    std::string track_name = paramSet["track_name"];
    if (track_name != "FeatureMatch" && track_name != "KCF") {
      LOG(ERROR) << "[Tracker] [track_name] Unsupported tracker type " << track_name;
      return false;
    }
  }
  return true;
}

}  // namespace cnstream
