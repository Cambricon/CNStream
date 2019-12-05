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
  TrackerContext& operator=(const TrackerContext&) = delete;
};

static thread_local TrackerContext g_tl_ctx;

Tracker::Tracker(const std::string &name) : Module(name) {}

Tracker::~Tracker() { Close(); }

inline TrackerContext *Tracker::GetTrackerContext() {
  if (!g_tl_ctx.processer_) {
    if ("KCF" == track_name_) {
      assert(nullptr != pKCFloader_);
      g_tl_ctx.processer_.reset(new edk::KcfTrack);
      if (!g_tl_ctx.processer_) return nullptr;

      dynamic_cast<edk::KcfTrack*>(g_tl_ctx.processer_.get())->SetModel(pKCFloader_);
    } else {  // "FeatureMatch by default"
      g_tl_ctx.processer_.reset(new edk::FeatureMatchTrack);
      if (!g_tl_ctx.processer_) return nullptr;

      g_tl_ctx.feature_extractor_ = std::unique_ptr<FeatureExtractor>(new FeatureExtractor);
      #ifdef CNS_MLU100
      if (!g_tl_ctx.feature_extractor_->Init(model_path_, func_name_)) {
        LOG(ERROR) << "FeatureMatchTrack Feature Extractor initial failed.";
        return nullptr;
      }
      #endif
    }
  }
  return &g_tl_ctx;
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

void Tracker::Close() {}

int Tracker::Process(std::shared_ptr<CNFrameInfo> data) {
  TrackerContext *ctx = GetTrackerContext();
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

    for (auto& obj : in) {
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

}  // namespace cnstream
