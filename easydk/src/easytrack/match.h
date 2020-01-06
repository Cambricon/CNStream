#ifndef EASYTRACK_MATCH_H_
#define EASYTRACK_MATCH_H_

#include <functional>
#include <map>
#include <vector>

#include "easytrack/easy_track.h"
#include "hungarian.h"
#include "track_data_type.h"

namespace edk {

using CostMatrix = std::vector<std::vector<float>>;
using DistanceFunc = std::function<float(const std::vector<std::vector<float>> &, const std::vector<float> &)>;

class MatchAlgorithm {
 public:
  static MatchAlgorithm *Instance();

  CostMatrix IoUCost(const std::vector<Rect> &det_rects, const std::vector<Rect> &tra_rects);

  void HungarianMatch(const CostMatrix &cost_matrix, std::vector<int> *assignment) {
    hungarian_.Solve(cost_matrix, assignment);
  }

  template <class... Args>
  float Distance(const std::string &dist_func, Args &&... args) {
    return distance_algo_[dist_func](std::forward<Args>(args)...);
  }

 private:
  MatchAlgorithm();
  float IoU(const Rect &a, const Rect &b);
  HungarianAlgorithm hungarian_;
  static std::map<std::string, DistanceFunc> distance_algo_;

};  // class MatchAlgorithm

}  // namespace edk

#endif  // EASYTRACK_MATCH_H_
