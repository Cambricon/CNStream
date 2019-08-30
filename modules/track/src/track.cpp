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

#include "cninfer/model_loader.h"
#include "opencv2/opencv.hpp"

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

float calculateFeatureOfNthRow(cv::Mat img, int n) {
  float result = 0;
  for (int i = 0; i < img.cols; i++) {
    int grey = img.ptr<uchar>(n)[i];
    result += grey > 127 ? (float)grey / 255 : -(float)grey / 255;
  }
  return result;
}

std::vector<float> getFeatures(cv::Mat img) {
#if (CV_MAJOR_VERSION == 2)
  cv::Ptr<cv::ORB> processer = new cv::ORB(128);
#elif (CV_MAJOR_VERSION == 3)
  cv::Ptr<cv::ORB> processer = cv::ORB::create(128);
#endif
  std::vector<cv::KeyPoint> keypoints;
  processer->detect(img, keypoints);
  cv::Mat desc;
  processer->compute(img, keypoints, desc);

  std::vector<float> features;
  for (int i = 0; i < 128; i++) {
    float feature = i < img.rows ? calculateFeatureOfNthRow(desc, i) : 0;
    features.push_back(feature);
  }
  return features;
}

namespace cnstream {

Tracker::Tracker(const std::string &name) : Module(name) {}

Tracker::~Tracker() { Close(); }

TrackerContext *Tracker::GetTrackerContext(CNFrameInfoPtr data) {
  TrackerContext *ctx = nullptr;
  auto it = tracker_ctxs_.find(data->channel_idx);
  if (it != tracker_ctxs_.end()) {
    ctx = it->second;
  } else {
    ctx = new TrackerContext;
    tracker_ctxs_[data->channel_idx] = ctx;
  }
  return ctx;
}

bool Tracker::Open(cnstream::ModuleParamSet paramSet) {
  if (paramSet.find("model_path") != paramSet.end() && paramSet.find("func_name") != paramSet.end()) {
    std::string model_path = paramSet.find("model_path")->second;
    std::string func_name = paramSet.find("func_name")->second;
    ploader_ = std::make_shared<libstream::ModelLoader>(model_path, func_name);
    ploader_->InitLayout();
  }
  return true;
}

void Tracker::Close() {
  for (auto &pair : tracker_ctxs_) {
    if (pair.second->mlu_processer_) {
      delete pair.second->mlu_processer_;
      pair.second->mlu_processer_ = nullptr;
    }
    if (pair.second->cpu_tracker_) {
      DS_Delete(pair.second->cpu_tracker_);
      pair.second->cpu_tracker_ = nullptr;
    }
    delete pair.second;
  }
  tracker_ctxs_.clear();
}

int Tracker::Process(std::shared_ptr<CNFrameInfo> data) {
  TrackerContext *ctx = GetTrackerContext(data);
  if (!ctx->initialized_) {
    if (ploader_) {
      ctx->mlu_processer_ = static_cast<libstream::DeepSortTrack *>(libstream::CnTrack::Create("DeepSortTrack"));
      ctx->mlu_processer_->SetModel(ploader_);
    } else {
      ctx->cpu_tracker_ = DS_Create();
    }
    ctx->initialized_ = true;
  }

  uint8_t *img_data = new uint8_t[data->frame.GetBytes()];
  uint8_t *p = img_data;
  for (int i = 0; i < data->frame.GetPlanes(); i++) {
    memcpy(p, data->frame.data[i]->GetCpuData(), data->frame.GetPlaneBytes(i));
    p += data->frame.GetPlaneBytes(i);
  }

  int width = data->frame.width;
  int height = data->frame.height;
  CNDataFormat format = data->frame.fmt;
  cv::Mat img;
  switch (format) {
    case CNDataFormat::CN_PIXEL_FORMAT_BGR24:
      img = cv::Mat(height, width, CV_8UC3, img_data).clone();
      break;
    case CNDataFormat::CN_PIXEL_FORMAT_RGB24:
      img = cv::Mat(height, width, CV_8UC3, img_data).clone();
      cv::cvtColor(img, img, cv::COLOR_RGB2BGR);
      break;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12: {
      img = cv::Mat(height * 3 / 2, width, CV_8UC1, img_data);
      cv::Mat bgr(height, width, CV_8UC3);
      cv::cvtColor(img, bgr, cv::COLOR_YUV2BGR_NV12);
      img = bgr;
      data->frame.ReallocMemory(CNDataFormat::CN_PIXEL_FORMAT_BGR24);
    } break;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21: {
      img = cv::Mat(height * 3 / 2, width, CV_8UC1, img_data);
      cv::Mat bgr(height, width, CV_8UC3);
      cv::cvtColor(img, bgr, cv::COLOR_YUV2BGR_NV21);
      img = bgr;
      data->frame.ReallocMemory(CNDataFormat::CN_PIXEL_FORMAT_BGR24);
    } break;
    default:
      LOG(WARNING) << "[Track] Unsupport pixel format.";
      delete[] img_data;
      return -1;
  }
  delete[] img_data;

  if (ctx->mlu_processer_) {
    std::vector<CnDetectObject> in, out;
    for (size_t i = 0; i < data->objs.size(); i++) {
      CnDetectObject obj;
      obj.label = std::stoi(data->objs[i]->id);
      obj.score = data->objs[i]->score;
      obj.x = data->objs[i]->bbox.x;
      obj.y = data->objs[i]->bbox.y;
      obj.w = data->objs[i]->bbox.w;
      obj.h = data->objs[i]->bbox.h;
      in.push_back(obj);
    }

    ctx->mlu_processer_->UpdateCpuFrame(img, in, &out);

    data->objs.clear();
    for (size_t i = 0; i < out.size(); i++) {
      std::shared_ptr<CNInferObject> obj = std::make_shared<CNInferObject>();
      obj->id = std::to_string(out[i].label);
      obj->track_id = std::to_string(out[i].track_id);
      obj->score = out[i].score;
      obj->bbox.x = out[i].x;
      obj->bbox.y = out[i].y;
      obj->bbox.w = out[i].w;
      obj->bbox.h = out[i].h;
      data->objs.push_back(obj);
    }

  } else {
    DS_DetectObjects detect_objs;
    for (size_t i = 0; i < data->objs.size(); i++) {
      DS_DetectObject obj;
      obj.class_id = std::stoi(data->objs[i]->id);
      obj.confidence = data->objs[i]->score;
      obj.rect.x = data->objs[i]->bbox.x * width;
      obj.rect.y = data->objs[i]->bbox.y * height;
      obj.rect.width = data->objs[i]->bbox.w * width;
      obj.rect.height = data->objs[i]->bbox.h * height;
      cv::Mat obj_mat(img, cv::Rect(obj.rect.x, obj.rect.y, obj.rect.width, obj.rect.height));
      obj.feature = getFeatures(obj_mat);
      detect_objs.push_back(obj);
    }

    DS_TrackObjects track_objs;
    DS_Update(ctx->cpu_tracker_, detect_objs, track_objs);

    data->objs.clear();
    for (size_t i = 0; i < track_objs.size(); i++) {
      std::shared_ptr<CNInferObject> obj = std::make_shared<CNInferObject>();
      obj->id = std::to_string(track_objs[i].class_id);
      obj->track_id = std::to_string(track_objs[i].track_id);
      obj->score = track_objs[i].confidence;
      obj->bbox.x = CLIP((float)track_objs[i].rect.x / width);
      obj->bbox.y = CLIP((float)track_objs[i].rect.y / height);
      obj->bbox.w = CLIP((float)track_objs[i].rect.width / width);
      obj->bbox.h = CLIP((float)track_objs[i].rect.height / height);
      data->objs.push_back(obj);
    }
  }
  cv::Mat frame(height, width, CV_8UC3, data->frame.data[0]->GetMutableCpuData());
  img.copyTo(frame);
  return 0;
}

}  // namespace cnstream
