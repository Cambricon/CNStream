/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "cns_openpose.hpp"
#include "postproc.hpp"

void RemapKeypoints(cns_openpose::Keypoints* keypoints, int src_w, int src_h, int dst_w, int dst_h) {
  float scaling_factor = std::min(1.0f * src_w / dst_w, 1.0f * src_h / dst_h);
  const int scaled_w = scaling_factor * dst_w;
  const int scaled_h = scaling_factor * dst_h;
  for (auto& points : *keypoints) {
    for (auto& point : points) {
      point.x -= (src_w - scaled_w) / 2.0f;
      point.y -= (src_h - scaled_h) / 2.0f;
      point.x = std::floor(1.0f * point.x / scaled_w * dst_w);
      point.y = std::floor(1.0f * point.y / scaled_w * dst_w);
    }
  }
}

/**
 * @brief Post process for body pose
 */
template <int knKeypoints, int knLimbs>
class PostprocPose : public cnstream::Postproc {
  static constexpr int knHeatmaps = knKeypoints + knLimbs * 2;
 public:
  virtual ~PostprocPose() {}
  using Heatmaps = std::array<cv::Mat, knHeatmaps>;
  int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& package) override;

 protected:
  virtual const std::array<std::pair<int, int>, knLimbs>& GetHeatmapIndexs() = 0;
  virtual const std::array<std::pair<int, int>, knLimbs>& GetLimbEndpointPairs() = 0;

 private:
  Heatmaps GetHeatmaps(float* net_output, const std::shared_ptr<edk::ModelLoader>& model);
  cns_openpose::Keypoints GetKeypoints(const Heatmaps& heatmaps);
  cns_openpose::Limbs GetLimbs(const Heatmaps& heatmaps, const cns_openpose::Keypoints& keypoints);
};  // class PostprocPose

template<int knKeypoints, int knLimbs>
int PostprocPose<knKeypoints, knLimbs>::Execute(const std::vector<float*>& net_outputs,
                                                const std::shared_ptr<edk::ModelLoader>& model,
                                                const cnstream::CNFrameInfoPtr& package) {
  // model output in NCHW order. see parameter named data_order in Inferencer module.
  if (model->OutputShape(0).C() != knHeatmaps)
    LOGF(POSTPROC_POSE) << "The number of heatmaps in model mismatched.";
  auto frame = package->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag);
  auto heatmaps = GetHeatmaps(net_outputs[0], model);
  auto keypoints = GetKeypoints(heatmaps);
  auto total_limbs = GetLimbs(heatmaps, keypoints);
  RemapKeypoints(&keypoints, model->InputShape(0).W(), model->InputShape(0).H(), frame->width, frame->height);
  package->collection.Add(cns_openpose::kPoseKeypointsTag, keypoints);
  package->collection.Add(cns_openpose::kPoseLimbsTag, total_limbs);
  return 0;
}

template <int knKeypoints, int knLimbs>
typename PostprocPose<knKeypoints, knLimbs>::Heatmaps
PostprocPose<knKeypoints, knLimbs>::GetHeatmaps(float* net_output, const std::shared_ptr<edk::ModelLoader>& model) {
  Heatmaps heatmaps;
  const int src_w = model->OutputShape(0).W();
  const int src_h = model->OutputShape(0).H();
  const int src_heatmap_len = src_w * src_h;
  const cv::Size dst_size(model->InputShape(0).W(), model->InputShape(0).H());
  for (int i = 0; i < knHeatmaps; ++i) {
    cv::Mat src(src_h, src_w, CV_32FC1, net_output + i * src_heatmap_len);
    cv::Mat dst;
    cv::resize(src, dst, dst_size, cv::INTER_CUBIC);
    heatmaps[i] = std::move(dst);
  }
  return heatmaps;
}

template <int knKeypoints, int knLimbs>
cns_openpose::Keypoints
PostprocPose<knKeypoints, knLimbs>::GetKeypoints(const Heatmaps& heatmaps) {
  cns_openpose::Keypoints keypoints(knKeypoints - 1);  // ignore background
  for (int i = 0; i < knKeypoints - 1; ++i) {
    static constexpr double kThreshold = 0.1;
    cv::Mat confidence_map = heatmaps[i];
    // image binaryzation
    cv::Mat smooth;
    cv::GaussianBlur(confidence_map, smooth, cv::Size(3, 3), 0, 0);
    cv::Mat binary;
    cv::threshold(smooth, binary, kThreshold, 255, cv::THRESH_BINARY);
    binary.convertTo(binary, CV_8UC1);

    // find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    // local maximum
    std::vector<cv::Point> points;
    for (const auto& contour : contours) {
      cv::Mat mask = cv::Mat::zeros(binary.rows, binary.cols, smooth.type());
      cv::fillConvexPoly(mask, contour, cv::Scalar(1));
      cv::Point max_loc;
      cv::minMaxLoc(smooth.mul(mask), NULL, NULL, NULL, &max_loc);
      points.push_back(max_loc);
    }
    keypoints[i] = std::move(points);
  }
  return keypoints;
}

static
std::vector<cv::Point> Sampling(cv::Point first_end, const cv::Point& second_end, int nsamples) {
  cv::Point distance = second_end - first_end;
  float x_step = 1.0 * distance.x / (nsamples - 1);
  float y_step = 1.0 * distance.y / (nsamples - 1);
  std::vector<cv::Point> samples;
  samples.push_back(first_end);
  for (int i = 1; i < nsamples - 1; ++i) {
    samples.emplace_back(cv::Point(std::round(first_end.x + x_step * i), std::round(first_end.y + y_step * i)));
  }
  samples.push_back(second_end);
  return samples;
}

template <int knKeypoints, int knLimbs>
cns_openpose::Limbs
PostprocPose<knKeypoints, knLimbs>::GetLimbs(const Heatmaps& heatmaps, const cns_openpose::Keypoints& keypoints) {
  static constexpr int knSamples = 10;  // number of samples between tow points
  static constexpr float kPafThreshold = 0.1;
  static constexpr float kSamplesMatchThreshold = 0.7;

  cns_openpose::Limbs total_limbs;
  for (int limb_idx = 0; limb_idx < knLimbs; ++limb_idx) {
    static constexpr int kPafHeatmapsOffset = knKeypoints;

    int first_ends_idx = GetLimbEndpointPairs()[limb_idx].first;
    int second_ends_idx = GetLimbEndpointPairs()[limb_idx].second;

    const cv::Mat& paf_first = heatmaps[kPafHeatmapsOffset + GetHeatmapIndexs()[limb_idx].first];
    const cv::Mat& paf_second = heatmaps[kPafHeatmapsOffset + GetHeatmapIndexs()[limb_idx].second];

    // both ends of the limb
    const std::vector<cv::Point>& first_ends = keypoints[first_ends_idx];
    const std::vector<cv::Point>& second_ends = keypoints[second_ends_idx];

    const int knFirstEnds = static_cast<int>(first_ends.size());
    const int knSecondEnds = static_cast<int>(second_ends.size());

    std::vector<std::pair<cv::Point, cv::Point>> limbs;
    // stores second endpoint status (selected or not), selected by which first endpoint and the score.
    // 0: selected or not,   1: index in limbs,  2: limb score
    std::vector<std::tuple<bool, size_t, float>>
      second_end_selected_status(knSecondEnds, std::make_tuple(false, -1, -1.0f));
    // find max score between first ends and second ends
    for (int first_end_idx = 0; first_end_idx < knFirstEnds; ++first_end_idx) {
      int selected_second_end_idx = -1;
      float max_score = -1;
      const cv::Point& first_end = first_ends[first_end_idx];
      for (int second_end_idx = 0; second_end_idx < knSecondEnds; ++second_end_idx) {
        const cv::Point& second_end = second_ends[second_end_idx];
        std::pair<float, float> distance = std::make_pair(second_end.x - first_end.x, second_end.y - first_end.y);
        float norm2 = std::sqrt(distance.first * distance.first +
                                distance.second * distance.second);
        distance.first /= norm2;
        distance.second /= norm2;

        // p(u)
        std::vector<cv::Point> sample_points = Sampling(first_end, second_end, knSamples);

        // L(p(u))
        float sum_of_sample_score = 0;
        int num_over_threshold = 0;
        for (int sample_idx = 0; sample_idx < knSamples; ++sample_idx) {
          const auto& sample_point = sample_points[sample_idx];
          float paf_first_value = paf_first.at<float>(sample_point);
          float paf_second_value = paf_second.at<float>(sample_point);
          float score = paf_first_value * distance.first + paf_second_value * distance.second;
          if (score > kPafThreshold) {
            ++num_over_threshold;
            sum_of_sample_score += score;
          }
        }  // for samples
        if (1.0f * num_over_threshold / knSamples > kSamplesMatchThreshold) {
          float avg_score = sum_of_sample_score / sample_points.size();
          if (avg_score > max_score) {
            const auto& selected_status = second_end_selected_status[second_end_idx];
            if (std::get<0>(selected_status) &&
                std::get<2>(selected_status) > avg_score) {
              // selected by other first endpoint and pre-score greater than current score
              continue;
            }
            selected_second_end_idx = second_end_idx;
            max_score = avg_score;
          }
        }  // if kSamplesMatchThreshold
      }  // for second ends
      if (-1 != selected_second_end_idx) {
        auto& selected_status = second_end_selected_status[selected_second_end_idx];
        // found best matchs second end, positions in keypoints vector
        limbs.emplace_back(std::make_pair(cv::Point(first_ends_idx, first_end_idx),
                                          cv::Point(second_ends_idx, selected_second_end_idx)));
        if (std::get<0>(selected_status)) {
          // selected by other first endpoint, but current score greater than pre-score. remove limb
          limbs.erase(limbs.begin() + std::get<1>(selected_status));
        }
        // save second endpoint selected status
        std::get<0>(selected_status) = true;
        std::get<1>(selected_status) = limbs.size() - 1;
        std::get<2>(selected_status) = max_score;
      }
    }  // for first ends
    total_limbs.push_back(std::move(limbs));
  }  // for keypoints

  return total_limbs;
}

static constexpr int knBody25Keypoints = 26;  // 25 keypoints + 1 background
static constexpr int knBody25Limbs = 26;

class PostprocBody25Pose : public PostprocPose<knBody25Keypoints, knBody25Limbs> {
 public:
  ~PostprocBody25Pose() = default;

 private:
  const std::array<std::pair<int, int>, knBody25Limbs>& GetHeatmapIndexs() override {
    static constexpr std::array<std::pair<int, int>, knBody25Limbs> kBody25HeatmapIndexs {
      std::make_pair(0, 1), {14, 15}, {22, 23}, {16, 17}, {18, 19}, {24, 25},
      {26, 27}, {6, 7}, {2, 3}, {4, 5}, {8, 9}, {10, 11}, {12, 13},
      {30, 31}, {32, 33}, {36, 37}, {34, 35}, {38, 39}, {20, 21},
      {28, 29}, {40, 41}, {42, 43}, {44, 45}, {46, 47}, {48, 49}, {50, 51}
    };
    return kBody25HeatmapIndexs;
  }
  const std::array<std::pair<int, int>, knBody25Limbs>& GetLimbEndpointPairs() override {
    static constexpr std::array<std::pair<int, int>, knBody25Limbs> kBody25LimbEndpointPairs {
      std::make_pair(1, 8), {1, 2}, {1, 5}, {2, 3}, {3, 4}, {5, 6}, {6, 7},
      {8, 9}, {9, 10}, {10, 11}, {8, 12}, {12, 13}, {13, 14},
      {1, 0}, {0, 15}, {15, 17}, {0, 16}, {16, 18}, {2, 17},
      {5, 18}, {14, 19}, {19, 20}, {14, 21}, {11, 22}, {22, 23}, {11, 24}
    };
    return kBody25LimbEndpointPairs;
  }

  DECLARE_REFLEX_OBJECT_EX(PostprocBody25Pose, cnstream::Postproc)
};  // class PostprocBody25Pose

IMPLEMENT_REFLEX_OBJECT_EX(PostprocBody25Pose, cnstream::Postproc)

static constexpr int knCOCOKeypoints = 19;  // 18 keypoints + 1 background
static constexpr int knCOCOLimbs = 19;

class PostprocCOCOPose : public PostprocPose<knCOCOKeypoints, knCOCOLimbs> {
 public:
  ~PostprocCOCOPose() = default;

 private:
  const std::array<std::pair<int, int>, knCOCOLimbs>& GetHeatmapIndexs() override {
    static constexpr std::array<std::pair<int, int>, knCOCOLimbs> kCOCOHeatmapIndexs {
      std::make_pair(12, 13), {20, 21}, {14, 15}, {16, 17}, {22, 23},
      {24, 25}, {0, 1}, {2, 3}, {4, 5}, {6, 7}, {8, 9}, {10, 11}, {28, 29},
      {30, 31}, {34, 35}, {32, 33}, {36, 37}, {18, 19}, {26, 27}
    };
    return kCOCOHeatmapIndexs;
  }
  const std::array<std::pair<int, int>, knCOCOLimbs>& GetLimbEndpointPairs() override {
    static constexpr std::array<std::pair<int, int>, knCOCOLimbs> kCOCOLimbEndpointPairs {
      std::make_pair(1, 2), {1, 5}, {2, 3}, {3, 4}, {5, 6},
      {6, 7}, {1, 8}, {8, 9}, {9, 10}, {1, 11}, {11, 12}, {12, 13},
      {1, 0}, {0, 14}, {14, 16}, {0, 15}, {15, 17}, {2, 16}, {5, 17}
    };
    return kCOCOLimbEndpointPairs;
  }

  DECLARE_REFLEX_OBJECT_EX(PostprocCOCOPose, cnstream::Postproc)
};  // class PostprocCOCOPose

IMPLEMENT_REFLEX_OBJECT_EX(PostprocCOCOPose, cnstream::Postproc)

