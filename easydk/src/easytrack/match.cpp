#include "match.h"

#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <string>

#define AVERAGE_DISTANCE false

namespace edk {

static float CosineDistance(const std::vector<std::vector<float>>& track_feature, const std::vector<float>& feature) {
  float cos_simi, x_y, y_squa, x_squa;
  float max_simi = 0.;
  size_t feat_size = feature.size();
#if AVERAGE_DISTANCE
  size_t feat_num = track_feature.size();
  x_squa = y_squa = x_y = 0.;
  for (size_t i = 0; i < feat_size; ++i) {
    float tra_feat = 0.;
    for (size_t j = 0; j < feat_num; ++j) {
      tra_feat += track_feature[j][i];
    }
    tra_feat /= feat_num;
    x_squa += feature[i] * feature[i];
    y_squa += tra_feat * tra_feat;
    x_y += tra_feat * feature[i];
  }
  cos_simi = x_y / (std::sqrt(x_squa) * std::sqrt(y_squa));
  max_simi = std::max(cos_simi, max_simi);

#else
  for (const auto& feat : track_feature) {
    x_squa = y_squa = x_y = 0;
    for (size_t i = 0; i < feat_size; i++) {
      x_squa += feature[i] * feature[i];
      y_squa += feat[i] * feat[i];
      x_y += feature[i] * feat[i];
    }
    if (x_squa * y_squa == 0) {
      cos_simi = -1;
    } else {
      cos_simi = x_y / (std::sqrt(x_squa) * std::sqrt(y_squa));
    }
    max_simi = std::max(cos_simi, max_simi);
  }
#endif

  if (max_simi > 1) max_simi = 1;
  return 1 - max_simi;
}

#ifdef ENABLE_ECULIDEAN_DISTANCE
static inline float L2Norm(const std::vector<float>& feature) {
  return std::sqrt(std::inner_product(feature.begin(), feature.end(), feature.begin(), 0));
}

static float EculideanDistance(const std::vector<std::vector<float>>& track_feature,
                               const std::vector<float>& feature) {
  float dist, feat_norm, track_norm, ele_minus;
  float min_dist = std::numeric_limits<float>::max();
  feat_norm = L2Norm(feature);
  size_t feat_size = feature.size();
  for (const auto& feat : track_feature) {
    dist = 0;
    track_norm = L2Norm(feat);
    for (size_t i = 0; i < feat_size; i++) {
      ele_minus = (feature[i] / feat_norm - feat[i] / track_norm);
      dist += ele_minus * ele_minus;
    }
    min_dist = std::min(min_dist, std::sqrt(dist));
  }
  return min_dist;
}
#endif

std::map<std::string, DistanceFunc> MatchAlgorithm::distance_algo_;

MatchAlgorithm::MatchAlgorithm() {
  distance_algo_["Cosine"] = CosineDistance;
#ifdef ENABLE_ECULIDEAN_DISTANCE
  distance_algo_["Eculidean"] = EculideanDistance;
#endif
}

MatchAlgorithm* MatchAlgorithm::Instance() {
  static MatchAlgorithm instance;
  return &instance;
}

float MatchAlgorithm::IoU(const Rect& a, const Rect& b) {
  float tl_x = std::max(a.xmin, b.xmin);
  float tl_y = std::max(a.ymin, b.ymin);
  float br_x = std::min(a.xmax, b.xmax);
  float br_y = std::min(a.ymax, b.ymax);

  float w = br_x - tl_x;
  float h = br_y - tl_y;
  if (w <= 0 || h <= 0) return 0.;
  float area_intersection = w * h;

  float area_a = (a.xmax - a.xmin) * (a.ymax - a.ymin);
  float area_b = (b.xmax - b.xmin) * (b.ymax - b.ymin);

  return area_intersection / (area_a + area_b - area_intersection);
}

CostMatrix MatchAlgorithm::IoUCost(const std::vector<Rect>& det_rects, const std::vector<Rect>& tra_rects) {
  CostMatrix res;
  for (auto& det : det_rects) {
    std::vector<float> arr;

    for (auto& tra : tra_rects) {
      float iou_cost_value = 1.0 - IoU(tra, det);
      arr.push_back(iou_cost_value);
    }

    res.push_back(arr);
  }

  return res;
}

}  // namespace edk
