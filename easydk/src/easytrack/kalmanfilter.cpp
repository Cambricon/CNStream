#include "kalmanfilter.h"
#include <cmath>
#include <vector>

namespace edk {

KalmanFilter::KalmanFilter() {
  mean_ = Matrix(1, 8);
  covariance_ = Matrix(8, 8);
  // state transition matrix A
  motion_mat_ = {{1, 0, 0, 0, 1, 0, 0, 0}, {0, 1, 0, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 0, 1, 0},
                 {0, 0, 0, 1, 0, 0, 0, 1}, {0, 0, 0, 0, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 1, 0, 0},
                 {0, 0, 0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 0, 0, 1}};
  // measurement matrix H
  update_mat_ = {
      {1, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 0, 0, 0, 0}};

  this->std_weight_position_ = 1. / 20;
  this->std_weight_velocity_ = 1. / 160;
}

void KalmanFilter::Initiate(const BoundingBox &measurement) {
  // initial state X(k-1|k-1)
  mean_[0][0] = measurement.x;
  mean_[0][1] = measurement.y;
  mean_[0][2] = measurement.width;
  mean_[0][3] = measurement.height;
  for (int i = 4; i < 8; ++i) {
    mean_[0][i] = 0;
  }

  vector<float> std(8, 0);
  std[2] = 1e-2;
  std[0] = std[1] = std[3] = 2 * std_weight_position_ * measurement.height;

  std[6] = 1e-5;
  std[4] = std[5] = std[7] = 10 * std_weight_velocity_ * measurement.height;

  // init MMSE P(k-1|k-1)
  for (int i = 0; i < 8; ++i) covariance_[i][i] = std[i] * std[i];
}

void KalmanFilter::Predict() {
  vector<float> std(8, 0);
  Matrix motion_cov(8, 8);

  // process noise covariance Q

  std[2] = 1e-2;
  std[0] = std[1] = std[3] = std_weight_position_ * mean_[0][3];
  std[6] = 1e-5;
  std[4] = std[5] = std[7] = std_weight_velocity_ * mean_[0][3];

  for (int i = 0; i < 8; ++i) {
    motion_cov[i][i] = std[i] * std[i];
  }

  Matrix mean1(8, 1);
  Matrix covariance1(8, 8);
  // formula 1：x(k|k-1)=A*x(k-1|k-1)
  mean1 = motion_mat_ * mean_.Trans();
  // formula 2：P(k|k-1)=A*P(k-1|k-1)A^T +Q
  covariance1 = motion_mat_ * covariance_ * motion_mat_.Trans();
  covariance1 += motion_cov;

  mean_ = mean1.Trans();
  covariance_ = covariance1;
}

KalHData KalmanFilter::Project(const Matrix &mean, const Matrix &covariance) {
  vector<float> std(4);

  std[2] = 1e-1;
  std[0] = std[1] = std[3] = std_weight_position_ * mean[0][3];

  // measurement noise R
  Matrix innovation_cov(4, 4);

  for (int i = 0; i < 4; ++i) {
    innovation_cov[i][i] = std[i] * std[i];
  }

  Matrix mean1(1, 4);
  Matrix covariance1(4, 4);
  mean1 = (update_mat_ * mean_.Trans()).Trans();

  // part of formula 3：(H*P(k|k-1)*H^T + R)
  covariance1 = update_mat_ * covariance * update_mat_.Trans();
  covariance1 += innovation_cov;

  return std::make_pair(mean1, covariance1);
}

void KalmanFilter::Update(const BoundingBox &bbox) {
  KalHData pa = Project(mean_, covariance_);
  Matrix &projected_mean = pa.first;
  Matrix &projected_cov = pa.second;

  Matrix kalman_gain(8, 4);
  Matrix measurement(1, 4);
  measurement[0][0] = bbox.x;
  measurement[0][1] = bbox.y;
  measurement[0][2] = bbox.width;
  measurement[0][3] = bbox.height;

  // formula 3: Kg = P(k|k-1) * H^T * (H*P(k|k-1)*H^T + R)^(-1)
  kalman_gain = covariance_ * update_mat_.Trans() * projected_cov.Inv();
  // formula 4: x(k|k) = x(k|k-1) + Kg * (m - H * x(k|k-1))
  mean_ += (measurement - projected_mean) * kalman_gain.Trans();
  // formula 5: P(k|k) = P(k|k-1) - Kg * H * P(k|k-1)
  covariance_ = covariance_ - kalman_gain * update_mat_ * covariance_;
  // covariance_ = covariance_ - kalman_gain * projected_cov * kalman_gain.Trans();
}

Matrix KalmanFilter::GatingDistance(const std::vector<BoundingBox> &measurements) {
  KalHData pa = Project(mean_, covariance_);
  Matrix mean1 = pa.first;
  Matrix covariance1_inv = pa.second.Inv();

  int num = measurements.size();
  Matrix d(1, 4);
  Matrix square_maha(1, num);
  for (int i = 0; i < num; i++) {
    d[0][0] = measurements[i].x - mean1[0][0];
    d[0][1] = measurements[i].y - mean1[0][1];
    d[0][2] = measurements[i].width - mean1[0][2];
    d[0][3] = measurements[i].height - mean1[0][3];

    square_maha[0][i] = (d * covariance1_inv * d.Trans())[0][0];
  }
  return square_maha;
}

}  // namespace edk
