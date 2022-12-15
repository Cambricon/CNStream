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

#ifndef EASYTRACK_KALMANFILTER_H
#define EASYTRACK_KALMANFILTER_H

#include <utility>
#include <vector>

#include "../include/easy_track.h"
#include "matrix.h"

namespace cnstream {

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
  void Project(const Matrix& mean, const Matrix& covariance);

  /**
   * @brief Calculate the Kalman gain and update the state and MMSE
   */
  void Update(const BoundingBox& measurement);

  /**
   * @brief Calculate the mahalanobis distance
   */
  Matrix GatingDistance(const std::vector<BoundingBox>& measurements);

  BoundingBox GetCurPos();

 private:
  static const Matrix motion_mat_;
  static const Matrix update_mat_;
  static const Matrix motion_mat_trans_;
  static const Matrix update_mat_trans_;
  Matrix mean_;
  Matrix covariance_;

  Matrix project_mean_;
  Matrix project_covariance_;

  float std_weight_position_;
  float std_weight_velocity_;

  bool need_recalc_project_{true};
};  // class KalmanFilter

}  // namespace cnstream

#endif  // EASYTRACK_KALMANFILTER_H
