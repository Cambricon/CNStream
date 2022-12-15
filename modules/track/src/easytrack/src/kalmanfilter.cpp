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

#include "kalmanfilter.h"
#include <cmath>
#include <vector>

namespace cnstream {

const Matrix KalmanFilter::motion_mat_{
    std::vector<float>{1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1,
                       0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    8u, 8u};
const Matrix KalmanFilter::update_mat_{
    std::vector<float>{1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0},
    4u, 8u};

const Matrix KalmanFilter::update_mat_trans_{KalmanFilter::update_mat_.Trans()};
const Matrix KalmanFilter::motion_mat_trans_{KalmanFilter::motion_mat_.Trans()};

KalmanFilter::KalmanFilter()
    : mean_(1, 8), covariance_(8, 8), std_weight_position_(1. / 20), std_weight_velocity_(1. / 160) {}

void KalmanFilter::Initiate(const BoundingBox &measurement) {
  // initial state X(k-1|k-1)
  mean_(0, 0) = measurement.x;
  mean_(0, 1) = measurement.y;
  mean_(0, 2) = measurement.width;
  mean_(0, 3) = measurement.height;
  for (int i = 4; i < 8; ++i) {
    mean_(0, i) = 0;
  }

  std::vector<float> std(8, 0);
  std[2] = 1e-2;
  std[0] = std[1] = std[3] = 2 * std_weight_position_ * measurement.height;

  std[6] = 1e-5;
  std[4] = std[5] = std[7] = 10 * std_weight_velocity_ * measurement.height;

  // init MMSE P(k-1|k-1)
  for (int i = 0; i < 8; ++i) covariance_(i, i) = std[i] * std[i];
}

void KalmanFilter::Predict() {
  std::vector<float> std(8, 0);
  Matrix motion_cov(8, 8);

  // process noise covariance Q

  std[2] = 1e-2;
  std[0] = std[1] = std[3] = std_weight_position_ * mean_(0, 3);
  std[6] = 1e-5;
  std[4] = std[5] = std[7] = std_weight_velocity_ * mean_(0, 3);

  for (int i = 0; i < 8; ++i) {
    motion_cov(i, i) = std[i] * std[i];
  }

  // formula 1：x(k|k-1)=A*x(k-1|k-1)
  // mean_ = (motion_mat_ * mean_.Trans()).Trans();
  Matrix mean1 = mean_ * motion_mat_trans_;
  // formula 2：P(k|k-1)=A*P(k-1|k-1)A^T +Q
  Matrix covariance1 = motion_mat_ * covariance_ * motion_mat_trans_ + motion_cov;

  mean_ = std::move(mean1);
  covariance_ = std::move(covariance1);
  need_recalc_project_ = true;
}

void KalmanFilter::Project(const Matrix &mean, const Matrix &covariance) {
  if (!need_recalc_project_) return;
  float cov_val1 = 1e-1 * 1e-1;
  float cov_val2 = std_weight_position_ * mean(0, 3);
  cov_val2 *= cov_val2;

  // measurement noise R
  Matrix innovation_cov(4, 4);

  innovation_cov(0, 0) = cov_val2;
  innovation_cov(1, 1) = cov_val2;
  innovation_cov(3, 3) = cov_val2;

  innovation_cov(2, 2) = cov_val1;

  // project_mean_ = (update_mat_ * mean_.Trans()).Trans();
  project_mean_ = mean_ * update_mat_trans_;

  // part of formula 3：(H*P(k|k-1)*H^T + R)
  project_covariance_ = update_mat_ * covariance * update_mat_trans_ + innovation_cov;
  need_recalc_project_ = false;
}

void KalmanFilter::Update(const BoundingBox &bbox) {
  Project(mean_, covariance_);

  Matrix measurement(1, 4);
  measurement(0, 0) = bbox.x;
  measurement(0, 1) = bbox.y;
  measurement(0, 2) = bbox.width;
  measurement(0, 3) = bbox.height;

  // formula 3: Kg = P(k|k-1) * H^T * (H*P(k|k-1)*H^T + R)^(-1)
  Matrix kalman_gain = covariance_ * update_mat_trans_ * project_covariance_.Inv();
  // formula 4: x(k|k) = x(k|k-1) + Kg * (m - H * x(k|k-1))
  mean_ += (measurement - project_mean_) * kalman_gain.Trans();
  // formula 5: P(k|k) = P(k|k-1) - Kg * H * P(k|k-1)
  covariance_ = covariance_ - kalman_gain * update_mat_ * covariance_;
  // covariance_ = covariance_ - kalman_gain * projected_cov * kalman_gain.Trans();

  need_recalc_project_ = true;
}

Matrix KalmanFilter::GatingDistance(const std::vector<BoundingBox> &measurements) {
  Project(mean_, covariance_);
  Matrix const &mean1 = project_mean_;
  Matrix covariance1_inv = project_covariance_.Inv();

  int num = measurements.size();
  Matrix d(1, 4);
  Matrix square_maha(1, num);
  for (int i = 0; i < num; i++) {
    d(0, 0) = measurements[i].x - mean1(0, 0);
    d(0, 1) = measurements[i].y - mean1(0, 1);
    d(0, 2) = measurements[i].width - mean1(0, 2);
    d(0, 3) = measurements[i].height - mean1(0, 3);

    square_maha(0, i) = (d * covariance1_inv * d.Trans())(0, 0);
  }
  return square_maha;
}

BoundingBox KalmanFilter::GetCurPos() { return {mean_(0, 0), mean_(0, 1), mean_(0, 2), mean_(0, 3)}; }

}  // namespace cnstream
