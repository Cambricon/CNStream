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

/**
 * @file easy_track.h
 * This file contains FeatureMatchTrack class and KcfTrack class.
 * Its purpose is to achieve object tracking.
 */

#ifndef EASYTRACK_EASY_TRACK_H_
#define EASYTRACK_EASY_TRACK_H_

#include <memory>
#include <vector>
#include <ostream>

namespace cnstream {

/**
 * @brief Struct of BoundingBox
 */
struct BoundingBox {
  float x;       ///< Topleft coordinates x
  float y;       ///< Topleft coordinates y
  float width;   ///< BoundingBox width
  float height;  ///< BoundingBox height
};

/**
 * @brief Struct of detection objects.
 */
struct DetectObject {
  /// Object detection label
  int label;
  /// Object detection confidence rate
  float score;
  /// Struct BoundingBox
  BoundingBox bbox;
  /// Object track identification
  int track_id;
  /// Object index in input vector
  int detect_id;
  /**
   * @brief Features of object extraction.
   * @attention The dimension of the feature vector is 128.
   */
  std::vector<float> feature;
  /// internal
  mutable float feat_mold;
};

/// Alias of vector stored DetectObject
using Objects = std::vector<DetectObject>;


/**
 * @brief EasyTrack class, help for tracking objects.
 */
class EasyTrack {
 public:
  /**
   * @brief Destroy the EasyTrack object.
   */
  virtual ~EasyTrack() {}

  /**
   * @brief Update object status and do track
   *
   * @param detects Detected objects
   * @param tracks Tracked objects
   */
  virtual void UpdateFrame(const Objects &detects, Objects *tracks) noexcept(false) = 0;
};  // class EasyTrack

class FeatureMatchPrivate;

/**
 * @brief Track objects based on match feature.
 *
 * @note Match tentative and featureless objects using IOU,
 *       and cascade-match confirmed objects using feature cosine distance
 */
class FeatureMatchTrack : public EasyTrack {
 public:
  /**
   * @brief Constructor of the FeatureMatchTrack class.
   */
  FeatureMatchTrack();

  /**
   * @brief Destroy the FeatureMatchTrack object.
   */
  ~FeatureMatchTrack();

  /**
   * @brief Set params related to Tracking algorithm.
   *
   * @param max_cosine_distance Threshold of cosine distance
   * @param nn_budget Tracker only saves the latest [nn_budget] samples of feature for each object
   * @param max_iou_distance Threshold of iou distance
   * @param max_age Object stay alive for [max_age] after disappeared
   * @param n_init After matched [n_init] times in a row, object is turned from TENTATIVE to CONFIRMED
   */
  void SetParams(float max_cosine_distance, int nn_budget, float max_iou_distance, int max_age, int n_init);

  /**
   * @brief Update object status and do tracking using cascade matching and IOU matching.
   *
   * @param detects Detected objects
   * @param tracks Tracked objects
   */
  void UpdateFrame(const Objects &detects, Objects *tracks) override;

 private:
  FeatureMatchPrivate *fm_p_;
  friend class FeatureMatchPrivate;
  float max_cosine_distance_ = 0.2;
  float max_iou_distance_ = 0.7;
  int max_age_ = 30;
  int n_init_ = 3;
  uint32_t nn_budget_ = 100;
};  // class FeatureMatchTrack

/**
 * @brief Insert DetectObject into the ostream
 *
 * @param os output stream to insert data to
 * @param obj reference to an DetectObjec to insert
 *
 * @return reference to output stream
 */
inline std::ostream &operator<<(std::ostream &os, const DetectObject &obj) {
  os << "[Object] label: " << obj.label << " score: " << obj.score << " track_id: " << obj.track_id << '\t'
     << "bbox: " << obj.bbox.x << "  " << obj.bbox.y << "  " << obj.bbox.width << "  " << obj.bbox.height;
  return os;
}

}  // namespace cnstream

#endif  // EASYTRACK_EASY_TRACK_H_
