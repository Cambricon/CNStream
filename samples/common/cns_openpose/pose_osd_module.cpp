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
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "cnstream_module.hpp"
#include "cns_openpose.hpp"

namespace cns_openpose {

static
std::vector<cv::Scalar> GenerateColors(size_t ncolors) {
  std::random_device rd;
  std::mt19937 num_gen(rd());
  std::uniform_int_distribution<> b(64, 255);
  std::uniform_int_distribution<> g(120, 255);
  std::uniform_int_distribution<> r(90, 200);
  std::vector<cv::Scalar> colors;
  for (size_t i = 0; i < ncolors; ++i) {
    colors.emplace_back(cv::Scalar(b(num_gen), g(num_gen), r(num_gen)));
  }
  return colors;
}

class PoseOsd : public cnstream::Module, public cnstream::ModuleCreator<PoseOsd> {
 public:
  explicit PoseOsd(const std::string& name) : cnstream::Module(name) {}
  bool Open(cnstream::ModuleParamSet param_set) override {
    if (param_set.find("nkeypoints") == param_set.end()) {
      LOGE(POSE_OSD) << "[nkeypoints] the number of keypoints must be set. For body25, the number of keypoints is 25";
      return false;
    }
    if (param_set.find("nlimbs") == param_set.end()) {
      LOGE(POSE_OSD) << "[nlimbs] the number of limbs must be set. For body25, the number of limbs is 26";
      return false;
    }

    try {
      nkeypoints_ = std::stoi(param_set["nkeypoints"]);
      nlimbs_ = std::stoi(param_set["nlimbs"]);
    } catch (std::exception& e) {
      LOGE(POSE_OSD) << "Parse [nkeypoints] or [nlimbs] failed, maybe there are not integers.";
      return false;
    }

    colors_ = GenerateColors(std::max(nkeypoints_, nlimbs_));
    return true;
  }
  void Close() override {}
  int Process(std::shared_ptr<cnstream::CNFrameInfo> package) override {
    auto frame = package->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag);
    const auto& keypoints = package->collection.Get<Keypoints>(kPoseKeypointsTag);
    const auto& total_limbs = package->collection.Get<Limbs>(kPoseLimbsTag);
    if (keypoints.size() != nkeypoints_) {
      LOGF(POSE_OSD) << "keypoints number mismatch!";
    }
    if (total_limbs.size() != nlimbs_) {
      LOGF(POSE_OSD) << "limbs number mismatch!";
    }
    cv::Mat origin_img = frame->ImageBGR();
    // draw limbs
    for (size_t i = 0; i < total_limbs.size(); ++i) {
      for (const auto& limb : total_limbs[i]) {
        cv::line(origin_img, keypoints[limb.first.x][limb.first.y],
                 keypoints[limb.second.x][limb.second.y],
                 colors_[i], 3);
      }
    }
    return 0;
  }

 private:
  size_t nkeypoints_ = 0;
  size_t nlimbs_ = 0;
  std::vector<cv::Scalar> colors_;
};  // class PoseOsd

}  // namespace cns_openpose

