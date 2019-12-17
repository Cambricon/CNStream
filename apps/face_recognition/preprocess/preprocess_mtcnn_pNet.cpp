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

#include <iostream>
#include <memory>
#include <vector>

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#else
#error OpenCV required
#endif
#include "preproc.hpp"

class PreprocMtcnnPnet : public cnstream::Preproc {
 public:
  const float MEAN = 127.5;
  const float VAR = 0.0078125;  // inverse of the variance
  /**
   * @brief Execute preproc on origin data
   *
   * @param net_inputs: neural network inputs
   * @param model: model information(you can get input shape and output shape from model)
   * @param package: smart pointer of struct to store origin data
   *
   * @return return 0 if succeed
   *
   * @attention net_inputs is a pointer to pre-allocated cpu memory
   */
  int Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& package) override;

  DECLARE_REFLEX_OBJECT_EX(PreprocMtcnnPnet, cnstream::Preproc);
};  // class PreprocCpu

IMPLEMENT_REFLEX_OBJECT_EX(PreprocMtcnnPnet, cnstream::Preproc)

int PreprocMtcnnPnet::Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
                              const cnstream::CNFrameInfoPtr& package) {
  // check params
  auto input_shapes = model->InputShapes();
  if (net_inputs.size() != 1 || input_shapes[0].c != 3) {
    LOG(ERROR) << "[PreprocCpu] model input shape not supported";
    return -1;
  }

  // get model input size.
  int dst_w = input_shapes[0].w;
  int dst_h = input_shapes[0].h;

  // get the frame's bgr data.
  cv::Mat img = *package->frame.ImageBGR();

  // resize
  cv::Mat outImg;
  cv::resize(img, outImg, cv::Size(dst_w, dst_h));

  // since model input data type is float, convert image to float
  cv::Mat dst(dst_h, dst_w, CV_32FC3, net_inputs[0]);
  outImg.convertTo(dst, CV_32F, VAR, -MEAN * VAR);  // normalize.  (x - mean) / var;
  return 0;
}
