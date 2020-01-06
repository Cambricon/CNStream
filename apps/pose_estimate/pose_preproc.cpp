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

#include <glog/logging.h>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>

#include <memory>
#include <string>
#include <vector>

#include "pose_utils.hpp"
#include "preproc.hpp"

namespace openpose {
/**
 * @brief openpose pre process
 */
class PreprocPose : public cnstream::Preproc {
 public:
  ~PreprocPose() {
    if (input_img_data_) {
      delete[] input_img_data_;
      input_img_data_ = nullptr;
    }
  }

  int Execute(const std::vector<float *> &net_inputs, const std::shared_ptr<edk::ModelLoader> &model,
              const cnstream::CNFrameInfoPtr &package) override;

 private:
  uint8_t *input_img_data_ = nullptr;
  DECLARE_REFLEX_OBJECT_EX(PreprocPose, cnstream::Preproc);
};  //  class PreprocPose

IMPLEMENT_REFLEX_OBJECT_EX(PreprocPose, cnstream::Preproc)

int PreprocPose::Execute(const std::vector<float *> &net_inputs, const std::shared_ptr<edk::ModelLoader> &model,
                         const cnstream::CNFrameInfoPtr &package) {
  // check params
  auto input_shapes = model->InputShapes();
  if (net_inputs.size() != 1 || input_shapes[0].c != 3) {
    LOG(ERROR) << "[PreprocPose] model input shape not supported";
    return -1;
  }

  DLOG(INFO) << "[PreprocPose] do preproc...";

  int src_width = package->frame.width;
  int src_height = package->frame.height;
  int dst_width = input_shapes[0].w;
  int dst_height = input_shapes[0].h;
  if (!input_img_data_) {
    input_img_data_ = new uint8_t[package->frame.GetBytes()];
  }

  uint8_t *tmp_in = input_img_data_;
  for (int i = 0; i < package->frame.GetPlanes(); ++i) {
    memcpy(tmp_in, package->frame.data[i]->GetCpuData(),  //  NOLINT
           package->frame.GetPlaneBytes(i));
    tmp_in += package->frame.GetPlaneBytes(i);
  }

  //  convert color space
  cv::Mat img;
  switch (package->frame.fmt) {
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21: {
      img = cv::Mat(src_height * 3 / 2, src_width, CV_8UC1, input_img_data_);
      cv::Mat bgr(src_height, src_width, CV_8UC3);
      cv::cvtColor(img, bgr, cv::COLOR_YUV2BGR_NV21);
      img = bgr;
    } break;
    default:
      LOG(WARNING) << "[PosePreprocess] Unsupport pixel format, only support "
                      "NV21 currently.";
      return -1;
  }

  //  resize image for input
  cv::Mat tmp_img = getScaledImg(img, cv::Size(dst_width, dst_height));
  cv::Mat dst(dst_height, dst_width, CV_32FC3, net_inputs[0]);
  tmp_img.convertTo(dst, CV_32F, 1 / 256.f, -0.5);
  return 0;
}

}  //  namespace openpose
