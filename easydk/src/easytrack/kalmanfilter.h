#ifndef EASYTRACK_KALMANFILTER_H
#define EASYTRACK_KALMANFILTER_H

#include "cxxutil/matrix.h"
#include "easytrack/easy_track.h"

namespace edk {

using KalHData = std::pair<Matrix, Matrix>;

/**
 * @brief Implementation of Kalman filter
 */
class KalmanFilter {
 public:
  /**
   * @brief Initialize the state transition matrix and measurement matrix
   */
  KalmanFilter();

  /**
   * @brief Initialize the initial state X(k-1|k-1) and MMSE P(k-1|k-1)
   */
  void Initiate(const BoundingBox& measurement);

  /**
   * @brief Predict the x(k|k-1) and P(k|k-1)
   */
  void Predict();

  /**
   * @brief Calculate measurement noise R
   */
  KalHData Project(const Matrix& mean, const Matrix& covariance);

  /**
   * @brief Calculate the Kalman gain and update the state and MMSE
   */
  void Update(const BoundingBox& measurement);

  /**
   * @brief Calculate the mahalanobis distance
   */
  Matrix GatingDistance(const std::vector<BoundingBox>& measurements);

 private:
  Matrix motion_mat_;
  Matrix update_mat_;
  Matrix mean_;
  Matrix covariance_;

  float std_weight_position_;
  float std_weight_velocity_;
};  // class KalmanFilter

}  // namespace edk

#endif  // EASYTRACK_KALMANFILTER_H
