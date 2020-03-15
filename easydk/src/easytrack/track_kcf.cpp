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

#ifdef ENABLE_KCF

#include <algorithm>
#include <cstring>
#include <memory>
#include <set>
#include <utility>

#include "../easyinfer/mlu_task_queue.h"
#include "cxxutil/logger.h"
#include "easyinfer/easy_infer.h"
#include "easyinfer/mlu_context.h"
#include "easyinfer/mlu_memory_op.h"
#include "easytrack/easy_track.h"
#include "kcf/kcf.h"
#include "match.h"
#include "track_data_type.h"

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

namespace edk {

#define DETECT_OUT_SIZE 224
#define MAX_KCF_OBJ_NUM 10

struct KcfTrackObject {
  int track_id;
  int class_id;
  float confidence;
  Rect rect;
  TrackState state;
  int kcf_out_idx;
};

class KcfTrackPrivate {
 public:
  ~KcfTrackPrivate();

 private:
  explicit KcfTrackPrivate(KcfTrack *kcf) {
    kcf_ = kcf;
    rois_ = new __Rect[4 * MAX_KCF_OBJ_NUM];
    match_ = MatchAlgorithm::Instance();
  }
  void KcfUpdate(void *mlu_gray, uint32_t frame_index, uint32_t frame_width, uint32_t frame_height,
                 const Objects &detects, Objects *tracks);

  void ProcessTrack(const std::vector<DetectObject> &det_objs, int frame_index);

  int device_id_ = 0;
  uint32_t batch_size_ = 0;
  KcfTrack *kcf_ = nullptr;
  std::shared_ptr<ModelLoader> model_loader_ = nullptr;
  edk::EasyInfer yuv2gray_;
  edk::MluMemoryOp mem_op_;
  void **yuv2gray_outputs_ = nullptr;
  float *detect_float_output_ = nullptr;
  half *detect_half_output_ = nullptr;
  void *detect_output_ = nullptr;
  void **yuv2gray_input_ = nullptr;

  KCFHandle handle_;
  __Rect *rois_ = nullptr;
  int track_num_ = -1;

  std::vector<KcfTrackObject> track_objs_;
  MatchAlgorithm *match_ = nullptr;
  int next_idx_ = 1;
  friend class KcfTrack;
};  // KcfTrackPrivate

KcfTrack::KcfTrack() { kcf_p_ = new KcfTrackPrivate(this); }

KcfTrack::~KcfTrack() { delete kcf_p_; }

void KcfTrack::SetModel(std::shared_ptr<ModelLoader> model, int dev_id, uint32_t batch_size) {
  if (!model) {
    throw EasyTrackError("Model is nullptr");
  }
  model->InitLayout();
  kcf_p_->model_loader_ = model;
  kcf_p_->device_id_ = dev_id;
  kcf_p_->batch_size_ = batch_size;

  edk::MluContext context;
  context.SetDeviceId(kcf_p_->device_id_);
  context.ConfigureForThisThread();

  kcf_p_->yuv2gray_.Init(kcf_p_->model_loader_, kcf_p_->batch_size_, dev_id);
  kcf_p_->mem_op_.SetLoader(kcf_p_->model_loader_);
  kcf_p_->yuv2gray_outputs_ = kcf_p_->mem_op_.AllocMluOutput(kcf_p_->batch_size_);
  kcf_p_->detect_float_output_ = new float[6 * DETECT_OUT_SIZE];
  kcf_p_->detect_half_output_ = new half[6 * DETECT_OUT_SIZE];
  kcf_p_->detect_output_ = kcf_p_->mem_op_.AllocMlu(6 * DETECT_OUT_SIZE * sizeof(half), kcf_p_->batch_size_);
  kcf_p_->yuv2gray_input_ = kcf_p_->mem_op_.AllocMluInput(kcf_p_->batch_size_);

  kcf_init(&(kcf_p_->handle_), kcf_p_->yuv2gray_.GetMluQueue()->queue, 0.5);
}

void KcfTrack::SetParams(float max_iou_distance) { max_iou_distance_ = max_iou_distance; }

void KcfTrack::UpdateFrame(const TrackFrame &frame, const Objects &detects, Objects *tracks) {
  if (frame.dev_type == TrackFrame::DevType::CPU) {
    throw EasyTrackError("CPU frame tracking has not been supported now");
  }
  tracks->clear();

  // 1. got normalized gray image
  cnrtMemcpy(kcf_p_->yuv2gray_input_[0], frame.data, frame.width * frame.height, CNRT_MEM_TRANS_DIR_DEV2DEV);
  kcf_p_->yuv2gray_.Run(kcf_p_->yuv2gray_input_, kcf_p_->yuv2gray_outputs_);

  // 2. process kcf track
  kcf_p_->KcfUpdate(kcf_p_->yuv2gray_outputs_[0], frame.frame_id % 4, frame.width, frame.height, detects, tracks);
}

KcfTrackPrivate::~KcfTrackPrivate() {
  edk::MluContext context;
  context.SetDeviceId(device_id_);
  context.ConfigureForThisThread();
  if (yuv2gray_outputs_ != nullptr) mem_op_.FreeArrayMlu(yuv2gray_outputs_, model_loader_->OutputNum());
  if (detect_float_output_ != nullptr) delete[] detect_float_output_;
  if (detect_half_output_ != nullptr) delete[] detect_half_output_;
  if (detect_output_ != nullptr) mem_op_.FreeMlu(detect_output_);
  if (yuv2gray_input_ != nullptr) mem_op_.FreeArrayMlu(yuv2gray_input_, model_loader_->InputNum());
  delete[] rois_;

  if (model_loader_) kcf_destroy(&handle_);
}

void KcfTrackPrivate::KcfUpdate(void *mlu_gray, uint32_t frame_index, uint32_t frame_width, uint32_t frame_height,
                                const Objects &detects, Objects *tracks) {
  uint32_t detect_size = detects.size();

  if (frame_index % 4 == 0) {
    // restructure detect results
    if (detect_size > 0) {
      float *pmem = detect_float_output_;
      for (size_t i = 0; i < detect_size; i++) {
        const DetectObject &obj = detects[i];
        pmem[i + 0 * DETECT_OUT_SIZE] = obj.bbox.x;
        pmem[i + 1 * DETECT_OUT_SIZE] = obj.bbox.y;
        pmem[i + 2 * DETECT_OUT_SIZE] = obj.bbox.x + obj.bbox.width;
        pmem[i + 3 * DETECT_OUT_SIZE] = obj.bbox.y + obj.bbox.height;
        pmem[i + 4 * DETECT_OUT_SIZE] = obj.score;
        pmem[i + 5 * DETECT_OUT_SIZE] = obj.label;
        // printf("[%ld] {%d %0.2f (%0.2f,%0.2f,%0.2f,%0.2f)}\n", i,
        //    obj.label, obj.score, obj.x, obj.y, obj.w, obj.h);
      }
    } else {
      LOG(INFO, "@@@@@@ no detect result");
      memset(detect_float_output_, 0, 6 * DETECT_OUT_SIZE * sizeof(float));
    }

    for (size_t i = 0; i < 6 * DETECT_OUT_SIZE; i++) {
      cnrtConvertFloatToHalf(detect_half_output_ + i, detect_float_output_[i]);
    }

    mem_op_.MemcpyH2D(detect_output_, detect_half_output_, 6 * DETECT_OUT_SIZE * sizeof(half), batch_size_);
    kcf_initKernel(&handle_, reinterpret_cast<half *>(mlu_gray), reinterpret_cast<half *>(detect_output_), rois_,
                   &track_num_);
  } else {
    kcf_updateKernel(&handle_, reinterpret_cast<half *>(mlu_gray), rois_ + frame_index * track_num_, track_num_);
  }

  std::vector<DetectObject> objs;
  for (int oi = 0; oi < track_num_ && oi < MAX_KCF_OBJ_NUM; ++oi) {
    DetectObject obj;
    const __Rect &roi = rois_[frame_index * track_num_ + oi];
    obj.label = roi.label;
    obj.score = roi.score / 1000;
    obj.bbox.x = 1.0 * roi.x / frame_width;
    obj.bbox.y = 1.0 * roi.y / frame_height;
    obj.bbox.width = 1.0 * roi.width / frame_width;
    obj.bbox.height = 1.0 * roi.height / frame_height;
    objs.push_back(obj);
  }

  ProcessTrack(objs, frame_index);

  for (size_t i = 0; i < track_objs_.size(); i++) {
    KcfTrackObject &track_obj = track_objs_[i];
    if (track_obj.class_id < 0 || track_obj.track_id < 0 || track_obj.rect.xmin < 0 || track_obj.rect.xmin > 1 ||
        track_obj.rect.ymin < 0 || track_obj.rect.ymin > 1 || track_obj.rect.xmax < 0 || track_obj.rect.xmax > 1 ||
        track_obj.rect.ymax < 0 || track_obj.rect.ymax > 1)
      continue;
    DetectObject obj;
    obj.label = track_obj.class_id;
    obj.track_id = track_obj.track_id;
    obj.score = track_obj.confidence;
    obj.bbox = Rect2BoundingBox(track_obj.rect);
    tracks->push_back(obj);
    printf("[%lu] {%d %d %0.2f (%0.2f,%0.2f,%0.2f,%0.2f)}\n", i, obj.label, obj.track_id, obj.score, obj.bbox.x,
           obj.bbox.y, obj.bbox.width, obj.bbox.height);
  }
}

void KcfTrackPrivate::ProcessTrack(const std::vector<DetectObject> &det_objs, int frame_index) {
  if (det_objs.empty()) {
    track_objs_.clear();
    return;
  }
  if (track_objs_.empty()) {
    for (uint32_t i = 0; i < det_objs.size(); i++) {
      const DetectObject &det_obj = det_objs[i];
      KcfTrackObject tra_obj;
      tra_obj.track_id = next_idx_;
      tra_obj.state = TrackState::CONFIRMED;
      tra_obj.class_id = det_obj.label;
      tra_obj.confidence = det_obj.score;
      tra_obj.rect = BoundingBox2Rect(det_obj.bbox);
      tra_obj.kcf_out_idx = i;
      track_objs_.push_back(tra_obj);
      next_idx_++;
    }
  } else if (frame_index % 4 != 0) {
    for (auto &tra_obj : track_objs_) {
      const DetectObject &det = det_objs[tra_obj.kcf_out_idx];
      tra_obj.class_id = det.label;
      tra_obj.confidence = det.score;
      tra_obj.rect = BoundingBox2Rect(det.bbox);
    }
  } else {
    MatchResult res;
    std::set<int> remained_detections;
    std::vector<Rect> det_rects, track_rects;
    std::vector<int> assignments;
    det_rects.resize(det_objs.size());
    track_rects.reserve(track_objs_.size());
    for (size_t i = 0; i < det_objs.size(); ++i) {
      det_rects[i] = BoundingBox2Rect(det_objs[i].bbox);
      res.unmatched_detections.push_back(i);
    }
    for (auto &obj : track_objs_) {
      track_rects.push_back(obj.rect);
    }
    CostMatrix dist_cost = match_->IoUCost(det_rects, track_rects);
    match_->HungarianMatch(dist_cost, &assignments);

    remained_detections.insert(res.unmatched_detections.begin(), res.unmatched_detections.end());
    for (size_t i = 0; i < assignments.size(); ++i) {
      if (assignments[i] < 0 || dist_cost[i][assignments[i]] > kcf_->max_iou_distance_) {
        res.unmatched_tracks.push_back(i);
      } else {
        res.matches.push_back(std::make_pair(res.unmatched_detections[assignments[i]], i));
        remained_detections.erase(res.unmatched_detections[assignments[i]]);
      }
    }
    res.unmatched_detections.clear();
    res.unmatched_detections.insert(res.unmatched_detections.end(), remained_detections.begin(),
                                    remained_detections.end());

    // matched
    for (auto &match : res.matches) {
      const DetectObject &det = det_objs[match.first];
      KcfTrackObject &tra = track_objs_[match.second];
      tra.kcf_out_idx = match.first;
      tra.confidence = det.score;
      tra.rect = BoundingBox2Rect(det.bbox);
    }

    for (auto unmatch_track : res.unmatched_tracks) {
      track_objs_[unmatch_track].state = TrackState::DELETED;
    }

    for (auto it = track_objs_.begin(); it != track_objs_.end();) {
      if (it->state == TrackState::DELETED)
        it = track_objs_.erase(it);
      else
        ++it;
    }

    for (auto unmatch_det : res.unmatched_detections) {
      const DetectObject &det_obj = det_objs[unmatch_det];
      KcfTrackObject tra_obj;
      tra_obj.track_id = next_idx_;
      tra_obj.kcf_out_idx = unmatch_det;
      tra_obj.state = TrackState::CONFIRMED;
      tra_obj.class_id = det_obj.label;
      tra_obj.confidence = det_obj.score;
      tra_obj.rect = BoundingBox2Rect(det_obj.bbox);

      track_objs_.push_back(tra_obj);
      next_idx_++;
    }
  }
}

}  // namespace edk

#endif  // ENABLE_KCF

