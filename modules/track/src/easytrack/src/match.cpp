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
#include "match.h"

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "matrix.h"

namespace cnstream {

static float CosineDistance(const std::vector<Feature>& track_feats, const Feature& det) {
  float cos_simi, x_y, y_mold, x_mold;
  float max_simi = 0.f;

  x_mold = det.mold < 0 ? L2Norm(det.vec) : det.mold;
  det.mold = det.mold < 0 ? x_mold : det.mold;
  for (const auto& track : track_feats) {
    track.mold = track.mold < 0 ? L2Norm(track.vec) : track.mold;
    y_mold = track.mold;
    x_y = InnerProduct(track.vec, det.vec);
    if (x_mold == 0.f || y_mold == 0.f) {
      cos_simi = -1;
    } else {
      cos_simi = x_y / (x_mold * y_mold);
    }
    max_simi = std::max(cos_simi, max_simi);
  }

  if (max_simi > 1) max_simi = 1;
  return 1 - max_simi;
}

thread_local detail::HungarianWorkspace MatchAlgorithm::workspace_;

MatchAlgorithm* MatchAlgorithm::Instance(const std::string& func) {
  static std::map<std::string, MatchAlgorithm> algos{{"Cosine", MatchAlgorithm(CosineDistance)}};
  return &(algos.at(func));
}

inline float MatchAlgorithm::IoU(const Rect& a, const Rect& b) {
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

Matrix MatchAlgorithm::IoUCost(const std::vector<Rect>& det_rects, const std::vector<Rect>& tra_rects) {
  Matrix res(det_rects.size(), tra_rects.size());
  for (uint32_t det_idx = 0; det_idx < res.Rows(); ++det_idx) {
    for (uint32_t tra_idx = 0; tra_idx < res.Cols(); ++tra_idx) {
      res(det_idx, tra_idx) = 1.0 - IoU(tra_rects[tra_idx], det_rects[det_idx]);
    }
  }
  return res;
}

}  // namespace cnstream
