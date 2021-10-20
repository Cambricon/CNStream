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
#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace cns_openpose {
static const std::map<unsigned int, std::string> kBody25PartStrs {
  {0,  "Nose"},
  {1,  "Neck"},
  {2,  "RShoulder"},
  {3,  "RElbow"},
  {4,  "RWrist"},
  {5,  "LShoulder"},
  {6,  "LElbow"},
  {7,  "LWrist"},
  {8,  "MidHip"},
  {9,  "RHip"},
  {10, "RKnee"},
  {11, "RAnkle"},
  {12, "LHip"},
  {13, "LKnee"},
  {14, "LAnkle"},
  {15, "REye"},
  {16, "LEye"},
  {17, "REar"},
  {18, "LEar"},
  {19, "LBigToe"},
  {20, "LSmallToe"},
  {21, "LHeel"},
  {22, "RBigToe"},
  {23, "RSmallToe"},
  {24, "RHeel"},
  {25, "Background"}
};

static const std::map<unsigned int, std::string> kBodyCOCOPartStrs {
    {0,  "Nose"},
    {1,  "Neck"},
    {2,  "RShoulder"},
    {3,  "RElbow"},
    {4,  "RWrist"},
    {5,  "LShoulder"},
    {6,  "LElbow"},
    {7,  "LWrist"},
    {8,  "RHip"},
    {9,  "RKnee"},
    {10, "RAnkle"},
    {11, "LHip"},
    {12, "LKnee"},
    {13, "LAnkle"},
    {14, "REye"},
    {15, "LEye"},
    {16, "REar"},
    {17, "LEar"},
    {18, "Background"}
};

using Keypoints = std::vector<std::vector<cv::Point>>;
using Limbs = std::vector<std::vector<std::pair<cv::Point, cv::Point>>>;
static constexpr char kPoseKeypointsTag[] = "POSE_KEYPOINTS";
static constexpr char kPoseLimbsTag[] = "POSE_LIMBS";
}  // namespace cns_openpose

