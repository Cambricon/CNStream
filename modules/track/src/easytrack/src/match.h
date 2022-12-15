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
#ifndef EASYTRACK_MATCH_H_
#define EASYTRACK_MATCH_H_

#include <cmath>
#include <map>
#include <string>
#include <vector>
#include <utility>

#include "../include/easy_track.h"
#include "cnstream_logging.hpp"
#include "hungarian.h"
#include "matrix.h"
#include "track_data_type.h"

namespace cnstream {

typedef float (*DistanceFunc)(const std::vector<Feature> &track_feature_set, const Feature &detect_feature);

namespace detail {
struct HungarianWorkspace {
  void* ptr{nullptr};
  size_t len{0};
  void Refresh(size_t new_len) {
    if (new_len == 0) return;
    if (len < new_len) {
      if (ptr) free(ptr);
      ptr = malloc(new_len);
      if (!ptr) {
        len = 0;
        throw std::bad_alloc();
      }
      len = new_len;
    }
  }
  ~HungarianWorkspace() {
    if (ptr) {
      free(ptr);
      len = 0;
    }
  }
};
}  // namespace detail

static inline float InnerProduct(const std::vector<float>& lhs, const std::vector<float>& rhs) {
  size_t cnt = lhs.size();
  if (cnt != rhs.size()) LOGF(TRACK) << "inner product need two vector of equal size";
  float sum{0.f};
  for (size_t idx = 0; idx < cnt; ++idx) {
    sum += lhs[idx] * rhs[idx];
  }
  return sum;
}

static inline float L2Norm(const std::vector<float>& feature) {
  return std::sqrt(InnerProduct(feature, feature));
}

class MatchAlgorithm {
 public:
  static MatchAlgorithm *Instance(const std::string &dist_func = "Cosine");

  Matrix IoUCost(const std::vector<Rect> &det_rects, const std::vector<Rect> &tra_rects);

  void HungarianMatch(const Matrix &cost_matrix, std::vector<int> *assignment) {
    workspace_.Refresh(hungarian_.GetWorkspaceSize(cost_matrix.Rows(), cost_matrix.Cols()));
    hungarian_.Solve(cost_matrix, assignment, workspace_.ptr);
  }

  template <class... Args>
  float Distance(Args &&... args) {
    return dist_func_(std::forward<Args>(args)...);
  }

 private:
  explicit MatchAlgorithm(DistanceFunc func) : dist_func_(func) {}
  float IoU(const Rect &a, const Rect &b);
  static thread_local detail::HungarianWorkspace workspace_;
  HungarianAlgorithm hungarian_;
  DistanceFunc dist_func_;
};  // class MatchAlgorithm

}  // namespace cnstream

#endif  // EASYTRACK_MATCH_H_
