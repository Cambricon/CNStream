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

#ifndef DEMO_POSE_ESTIMATE_POSE_PARAMS_HPP_
#define DEMO_POSE_ESTIMATE_POSE_PARAMS_HPP_

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace openpose {
// pose body parts
const std::map<unsigned int, std::string> POSE_COCO_BODY_PARTS{
    {0, "Nose"},   {1, "Neck"},      {2, "RShoulder"},  {3, "RElbow"},
    {4, "RWrist"}, {5, "LShoulder"}, {6, "LElbow"},     {7, "LWrist"},
    {8, "RHip"},   {9, "RKnee"},     {10, "RAnkle"},    {11, "LHip"},
    {12, "LKnee"}, {13, "LAnkle"},   {14, "REye"},      {15, "LEye"},
    {16, "REar"},  {17, "LEar"},     {18, "Background"}};

// pose body part pairs
const std::vector<unsigned int> POSE_COCO_BODY_PART_PAIRS{
    1,  2, 1,  5,  2,  3,  3,  4, 5, 6, 6,  7,  1,  8, 8,  9,  9,
    10, 1, 11, 11, 12, 12, 13, 1, 0, 0, 14, 14, 16, 0, 15, 15, 17};

const std::vector<unsigned int> POSE_MAP_INDEX{
    12, 13, 20, 21, 14, 15, 16, 17, 22, 23, 24, 25, 0,  1,  2,  3,  4,  5,  6,
    7,  8,  9,  10, 11, 28, 29, 30, 31, 34, 35, 32, 33, 36, 37, 18, 19, 26, 27};

#define RENDER_COLORS                                                          \
  255.f, 0.f, 85.f, 255.f, 0.f, 0.f, 255.f, 85.f, 0.f, 255.f, 170.f, 0.f,      \
      255.f, 255.f, 0.f, 170.f, 255.f, 0.f, 85.f, 255.f, 0.f, 0.f, 255.f, 0.f, \
      0.f, 255.f, 85.f, 0.f, 255.f, 170.f, 0.f, 255.f, 255.f, 0.f, 170.f,      \
      255.f, 0.f, 85.f, 255.f, 0.f, 0.f, 255.f, 255.f, 0.f, 170.f, 170.f, 0.f, \
      255.f, 255.f, 0.f, 255.f, 85.f, 0.f, 255.f

// pose coco map index
const std::vector<float> POSE_COCO_COLORS_RENDER{RENDER_COLORS};
const unsigned int POSE_MAX_PEOPLE = 20;

struct BlobData {
  int count;
  float *list;
  int num;
  int channels;
  int height;
  int width;
  int capacity_count;
};

template <typename T> inline int intRound(const T a) {
  return static_cast<int>(a + 0.5f);
}

// Max/min functions
template <typename T> inline T fastMax(const T a, const T b) {
  return (a > b ? a : b);
}

template <typename T> inline T fastMin(const T a, const T b) {
  return (a < b ? a : b);
}

// some default value
inline const std::vector<unsigned int> &getPosePartPairs() {
  return POSE_COCO_BODY_PART_PAIRS;
}

inline float getDefaultNmsThreshold(const bool maximizePositives) {
  return (maximizePositives ? 0.02f : 0.05f);
}

inline float
getDefaultConnectInterMinAboveThreshold(const bool maximizePositives) {
  return (maximizePositives ? 0.75f : 0.95f);
}

inline float getDefaultConnectInterThreshold(const bool maximizePositives) {
  return (maximizePositives ? 0.01f : 0.05f);
}

inline unsigned int getDefaultMinSubsetCnt(const bool maximizePositives) {
  return (maximizePositives ? 5u : 6u);
}

inline float getDefaultConnectMinSubsetScore(const bool maximizePositives) {
  return (maximizePositives ? 0.05f : 0.4f);
}

inline float getDefaultRenderThreshold() { return 0.05f; }

inline unsigned int getNumberBodyParts() {
  return POSE_COCO_BODY_PARTS.size() - 1;
}

float getScaleFactor(cv::Size srcSize, cv::Size scaledSize);
cv::Mat getScaledImg(const cv::Mat &im, cv::Size baseSize);

BlobData *createBlobData(const int num, const int channels, const int height,
                         const int width);
void releaseBlobData(BlobData **blob);

//
void nms(BlobData *input_blob, BlobData *output_blob,
         const float nms_threshold);

float getScoreAB(const int i, const int j, const float *const candidateAPtr,
                 const float *const candidateBPtr, const float *const mapX,
                 const float *const mapY, const cv::Size &heatMapSize,
                 const float interThreshold,
                 const float interMinAboveThreshold);

std::vector<std::pair<std::vector<int>, float>> createPeopleVector(
    const float *const heatMapPtr, const float *const peaksPtr,
    const cv::Size &heatMapSize, const int maxPeaks, const float interThreshold,
    const float interMinAboveThreshold,
    const std::vector<unsigned int> &bodyPartPairs,
    const unsigned int numberBodyParts, const unsigned int numberBodyPartPairs);

void removePeopleBelowThresholds(
    std::vector<int> *validSubsetIndexes, int *numberPeople,
    const std::vector<std::pair<std::vector<int>, float>> &peopleVector,
    const unsigned int numberBodyParts, const int minSubsetCnt,
    const float minSubsetScore, const int maxPeaks,
    const bool maximizePositives);

void getPoseKeyPoints(
    std::vector<float> *poseKeypoints_ptr, const float scaleFactor,
    const std::vector<std::pair<std::vector<int>, float>> &peopleVector,
    const std::vector<int> &validSubsetIndexes, const float *const peaksPtr,
    const int numberPeople, const unsigned int numberBodyParts,
    const unsigned int numberBodyPartPairs);

void connectBodyParts(std::vector<float> *poseKeypoints_ptr,
                      const float *const heatMapPtr,
                      const float *const peaksPtr, const cv::Size &heatMapSize,
                      const int maxPeaks, const float interMinAboveThreshold,
                      const float interThreshold, const int minSubsetCnt,
                      const float minSubsetScore, const float scaleFactor,
                      const bool maximizePositives,
                      std::vector<int> *keypointShape);

void renderKeypoints(cv::Mat *frame, const std::vector<float> &keypoints,
                     const std::vector<int> keyshape,
                     const std::vector<unsigned int> &pairs,
                     const std::vector<float> colors,
                     const float thicknessCircleRatio,
                     const float thicknessLineRatioWRTCircle,
                     const float render_threshold, float scale);

void renderPoseKeypoints(cv::Mat *frame,
                         const std::vector<float> &poseKeypoints,
                         const std::vector<int> keyshape,
                         const float renderThreshold, float scale,
                         const bool blendOriginalFrame = true);
}  // namespace openpose

#endif  // DEMO_POSE_ESTIMATE_POSE_PARAMS_HPP_
