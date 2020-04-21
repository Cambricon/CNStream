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
#include <mutex>
#include <numeric>
#include <set>
#include <vector>
#include "easytrack/easy_track.h"

#include "cxxutil/logger.h"
#include "cxxutil/matrix.h"
#include "kalmanfilter.h"
#include "match.h"
#include "track_data_type.h"

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

// chi2inv95 at 4 degree of freedom
constexpr const float gating_threshold = 9.4877;

static edk::BoundingBox to_xyah(const edk::BoundingBox &bbox) {
  edk::BoundingBox xyah;
  xyah.x = bbox.x + bbox.width / 2;
  xyah.y = bbox.y + bbox.height / 2;
  xyah.width = bbox.width / bbox.height;
  xyah.height = bbox.height;
  return xyah;
}

namespace edk {

struct FeatureMatchTrackObject {
  Rect pos;
  int class_id;
  int track_id = -1;
  float score;
  TrackState state;
  int age = 1;
  int time_since_last_update = 0;
  std::vector<std::vector<float>> features;
  bool has_feature;
  bool feature_unmatched = false;
  KalmanFilter *kf;
};

class FeatureMatchPrivate {
 private:
  explicit FeatureMatchPrivate(FeatureMatchTrack *fm) {
    fm_ = fm;
    match_algo_ = MatchAlgorithm::Instance();
  }
  MatchResult &MatchCascade();
  MatchResult &MatchIou(std::vector<int> detect_matrices, std::vector<int> track_matrices);
  void InitNewTrack(const DetectObject &obj);
  void MarkMiss(FeatureMatchTrackObject *track);

  FeatureMatchTrack *fm_;

  MatchAlgorithm *match_algo_;
  std::vector<FeatureMatchTrackObject> tracks_;
  std::vector<int> unconfirmed_track_;
  std::vector<int> confirmed_track_;
  std::vector<int> assignments_;
  MatchResult res_feature_;
  MatchResult res_iou_;
  const Objects *detects_ = nullptr;
  std::mutex update_mutex_;

  uint64_t next_id_ = 0;
  friend class FeatureMatchTrack;
};  // class FeatureMatchPrivate

FeatureMatchTrack::FeatureMatchTrack() { fm_p_ = new FeatureMatchPrivate(this); }

FeatureMatchTrack::~FeatureMatchTrack() {
  for (auto& obj : fm_p_->tracks_) {
    delete obj.kf;
  }
  delete fm_p_;
}

void FeatureMatchTrack::SetParams(float max_cosine_distance, int nn_budget, float max_iou_distance, int max_age,
                                  int n_init) {
  LOG(INFO, "FeatureMatchTrack Params -----");
  LOG(INFO, "   max cosine distance: %f", max_cosine_distance);
  LOG(INFO, "   max IoU distance: %f", max_iou_distance);
  LOG(INFO, "   max age: %d", max_age);
  LOG(INFO, "   nn budget: %d", nn_budget);
  LOG(INFO, "   n_init: %d", n_init);
  max_cosine_distance_ = max_cosine_distance;
  max_iou_distance_ = max_iou_distance;
  nn_budget_ = nn_budget;
  max_age_ = max_age;
  n_init_ = n_init;
}

MatchResult &FeatureMatchPrivate::MatchCascade() {
  const Objects &det_objs = *detects_;
  CostMatrix cost_matrix;
  std::vector<int> track_indices;
  MatchResult &res = res_feature_;
  res.matches.clear();
  res.unmatched_detections.clear();
  res.unmatched_tracks.clear();
  res.unmatched_detections.resize(det_objs.size());
  std::iota(res.unmatched_detections.begin(), res.unmatched_detections.end(), 0);

  std::set<int> remained_detections;
  remained_detections.insert(res.unmatched_detections.begin(), res.unmatched_detections.end());
  LOG(TRACE, "MatchCascade) Match scale, detects %u, tracks %u", det_objs.size(), confirmed_track_.size());
  for (int age = 0; age < fm_->max_age_; ++age) {
#ifdef TRACE_RESULT
    LOG(TRACE, "Cascade: Number of remained detections ----- %u", remained_detections.size());
#endif
    // no remained detections or no confirmed tracks, end match
    if (remained_detections.empty() || confirmed_track_.empty()) break;

    // get all confirmed tracks with same age
    for (size_t t = 0; t < confirmed_track_.size(); ++t) {
      if (tracks_[confirmed_track_[t]].time_since_last_update == age + 1) {
        track_indices.push_back(confirmed_track_[t]);
      }
    }
    if (track_indices.empty()) {
      LOG(TRACE, "Cascade: No tracks for age %d round, continue", age);
      continue;
    }
    size_t det_num = res.unmatched_detections.size();
    size_t tra_num = track_indices.size();
    cost_matrix.assign(tra_num, std::vector<float>(det_num, 0));

    // calculate cost matrix
    std::vector<BoundingBox> measurements;
    measurements.reserve(det_num);
    for (size_t i = 0; i < det_num; ++i) {
      measurements.push_back(to_xyah(det_objs[res.unmatched_detections[i]].bbox));
    }
    for (size_t i = 0; i < tra_num; ++i) {
      Matrix gating_dist = tracks_[track_indices[i]].kf->GatingDistance(measurements);
      for (size_t j = 0; j < det_num; ++j) {
        cost_matrix[i][j] = match_algo_->Distance("Cosine", tracks_[track_indices[i]].features,
                                                  det_objs[res.unmatched_detections[j]].feature);
        // LOG(TRACE, "object %d - %d mdist: %.5f", i, j, gating_dist[0][j]);
        // LOG(TRACE, "object %d - %d cosdist: %.5f", i, j, cost_matrix[i][j]);
        if (cost_matrix[i][j] > fm_->max_cosine_distance_ || gating_dist[0][j] > gating_threshold) {
          LOG(TRACE, "object %d - %d feature distance is larger than max_cosine_distance", i, j);
          cost_matrix[i][j] = fm_->max_cosine_distance_ + 1e-5;
        }
      }
    }

    // min cost match
    match_algo_->HungarianMatch(cost_matrix, &assignments_);

    // arrange match result
    for (size_t i = 0; i < assignments_.size(); ++i) {
      if (assignments_[i] < 0 || cost_matrix[i][assignments_[i]] > fm_->max_cosine_distance_) {
        res.unmatched_tracks.push_back(track_indices[i]);
      } else {
        res.matches.push_back(std::make_pair(res.unmatched_detections[assignments_[i]], track_indices[i]));
        remained_detections.erase(res.unmatched_detections[assignments_[i]]);
      }
    }
    track_indices.clear();
    res.unmatched_detections.clear();
    res.unmatched_detections.insert(res.unmatched_detections.end(), remained_detections.begin(),
                                    remained_detections.end());
  }
  return res;
}

MatchResult &FeatureMatchPrivate::MatchIou(std::vector<int> detect_indices, std::vector<int> track_indices) {
  MatchResult &res = res_iou_;
  res.matches.clear();
  res.unmatched_detections.clear();
  res.unmatched_tracks.clear();
  if (detect_indices.empty()) {
    LOG(INFO, "No remained detections to process IoU match");
    res.unmatched_tracks.insert(res.unmatched_tracks.end(), track_indices.begin(), track_indices.end());
    return res;
  }
  uint32_t detect_num = detect_indices.size();
  uint32_t track_num = track_indices.size();
  LOG(TRACE, "MatchIoU) Match scale, detects %u, tracks %u", detect_num, track_num);
  std::vector<Rect> det_rects, tra_rects;
  const Objects &det_objs = *detects_;
  std::set<int> remained_detections;
  det_rects.reserve(detect_num);
  tra_rects.reserve(track_num);

  // calculate iou cost matrix
  for (auto &idx : detect_indices) {
    det_rects.push_back(BoundingBox2Rect(det_objs[idx].bbox));
    remained_detections.insert(idx);
    res.unmatched_detections.push_back(idx);
  }
  for (auto &idx : track_indices) {
    tra_rects.push_back(tracks_[idx].pos);
  }
  CostMatrix cost_matrix = match_algo_->IoUCost(tra_rects, det_rects);
  if (cost_matrix.empty()) return res;
  match_algo_->HungarianMatch(cost_matrix, &assignments_);

  for (size_t i = 0; i < assignments_.size(); ++i) {
    if (assignments_[i] < 0 || cost_matrix[i][assignments_[i]] > fm_->max_iou_distance_) {
      res.unmatched_tracks.push_back(track_indices[i]);
    } else {
      res.matches.push_back(std::make_pair(res.unmatched_detections[assignments_[i]], track_indices[i]));
      remained_detections.erase(res.unmatched_detections[assignments_[i]]);
    }
  }

  res.unmatched_detections.clear();
  res.unmatched_detections.insert(res.unmatched_detections.end(), remained_detections.begin(),
                                  remained_detections.end());
  return res;
}

void FeatureMatchPrivate::InitNewTrack(const DetectObject &det) {
  LOG(TRACE, "new track: %lu", next_id_);
  FeatureMatchTrackObject obj;
  obj.age = 1;
  obj.class_id = det.label;
  obj.pos = BoundingBox2Rect(det.bbox);
  obj.state = TrackState::TENTATIVE;
  obj.features.clear();
  obj.has_feature = false;
  if (!det.feature.empty()) {
    for (auto& val : det.feature) {
      if (val != 0) {
        obj.has_feature = true;
        obj.features.emplace_back(det.feature);
        break;
      }
    }
  }
  obj.track_id = next_id_++;
  obj.kf = new KalmanFilter;
  obj.kf->Initiate(to_xyah(det.bbox));
  tracks_.push_back(obj);
}

void FeatureMatchPrivate::MarkMiss(FeatureMatchTrackObject *track) {
  if (track->state == TrackState::TENTATIVE || track->time_since_last_update > fm_->max_age_) {
    track->state = TrackState::DELETED;
  }
}

void FeatureMatchTrack::UpdateFrame(const TrackFrame &frame, const Objects &detects, Objects *tracks) {
  if (tracks == nullptr) {
    throw EasyTrackError("parameter 'tracks' is nullptr");
  }

  // guard track state
  std::lock_guard<std::mutex> lk(fm_p_->update_mutex_);

  uint32_t detect_num = detects.size();
  uint32_t track_num = fm_p_->tracks_.size();
  LOG(TRACE, "FeatureMatch) Track scale, detects %u, tracks %u", detect_num, track_num);
  // no tracks, first enter
  if (fm_p_->tracks_.empty()) {
    fm_p_->tracks_.reserve(detect_num);
    for (auto &det : detects) {
      fm_p_->InitNewTrack(det);
      tracks->push_back(det);
      tracks->rbegin()->track_id = fm_p_->tracks_.rbegin()->track_id;
    }
  } else {
    fm_p_->detects_ = &detects;
    fm_p_->unconfirmed_track_.clear();
    fm_p_->confirmed_track_.clear();
    for (size_t i = 0; i < track_num; ++i) {
      // update track indices
      if (fm_p_->tracks_[i].state == TrackState::CONFIRMED && fm_p_->tracks_[i].has_feature) {
        fm_p_->confirmed_track_.push_back(i);
        fm_p_->tracks_[i].feature_unmatched = false;
      } else {
        fm_p_->unconfirmed_track_.push_back(i);
      }
      fm_p_->tracks_[i].time_since_last_update++;
      fm_p_->tracks_[i].kf->Predict();
    }

    // match with features
    MatchResult &res_f = fm_p_->MatchCascade();
#ifdef TRACE_RESULT
    LOG(TRACE, "FeatureMatch) Cascade result, matched %u, unmatched detects %u, unmatched tracks %u",
        res_f.matches.size(), res_f.unmatched_detections.size(), res_f.unmatched_tracks.size());
#endif

    // give first missed object a chance
    std::vector<int> match_iou_track;
    match_iou_track.clear();
    match_iou_track.insert(match_iou_track.end(), fm_p_->unconfirmed_track_.begin(), fm_p_->unconfirmed_track_.end());
    for (auto &idx : res_f.unmatched_tracks) {
      fm_p_->tracks_[idx].feature_unmatched = true;
      if (fm_p_->tracks_[idx].time_since_last_update == 1) {
        match_iou_track.push_back(idx);
      } else {
        LOG(TRACE, "Object %d missed", idx);
        fm_p_->MarkMiss(&(fm_p_->tracks_[idx]));
      }
    }

    // match with iou
    MatchResult &res_iou = fm_p_->MatchIou(res_f.unmatched_detections, match_iou_track);
#ifdef TRACE_RESULT
    LOG(TRACE, "FeatureMatch) IoU result, matched %u, unmatched detects %u, unmatched tracks %u",
        res_iou.matches.size(), res_iou.unmatched_detections.size(), res_iou.unmatched_tracks.size());
#endif

    // update matched
    DetectObject tmp_obj;
    FeatureMatchTrackObject *ptrack_obj;
    const DetectObject *pdetect_obj;
    res_f.matches.insert(res_f.matches.end(), res_iou.matches.begin(), res_iou.matches.end());

    tracks->reserve(detect_num);
    for (auto &pair : res_f.matches) {
      ptrack_obj = &(fm_p_->tracks_[pair.second]);
      pdetect_obj = &detects[pair.first];
      ptrack_obj->kf->Update(to_xyah(pdetect_obj->bbox));
      tracks->push_back(*pdetect_obj);
      tracks->rbegin()->track_id = ptrack_obj->track_id;
      if (!ptrack_obj->feature_unmatched) {
        ptrack_obj->features.emplace_back(pdetect_obj->feature);
        if (ptrack_obj->features.size() > nn_budget_) {
          ptrack_obj->features.erase(ptrack_obj->features.begin());
        }
      }
      ptrack_obj->time_since_last_update = 0;
      ptrack_obj->age++;
      if (ptrack_obj->state == TrackState::TENTATIVE && ptrack_obj->age > n_init_) {
        ptrack_obj->state = TrackState::CONFIRMED;
      }
    }

    // unmatched detections: init new track
    for (auto &idx : res_iou.unmatched_detections) {
      fm_p_->InitNewTrack(detects[idx]);
      tracks->push_back(detects[idx]);
      tracks->rbegin()->track_id = fm_p_->tracks_.rbegin()->track_id;
    }

    // unmatched tracks: mark missed
    for (auto idx : res_iou.unmatched_tracks) {
      LOG(TRACE, "Object %d missed", idx);
      fm_p_->MarkMiss(&(fm_p_->tracks_[idx]));
    }

    // erase dead track object
    for (auto iter = fm_p_->tracks_.begin(); iter != fm_p_->tracks_.end();) {
      if (iter->state == TrackState::DELETED || iter->time_since_last_update > max_age_) {
        LOG(TRACE, "delete track: %d", iter->track_id);
        delete iter->kf;
        iter = fm_p_->tracks_.erase(iter);
      } else {
        iter++;
      }
    }
  }
}

}  // namespace edk

