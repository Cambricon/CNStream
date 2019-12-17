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
#include "cnstream_frame.hpp"
#include "preproc.hpp"

class PreprocVggface : public cnstream::Preproc {
 public:
  // The average of the BGR channel
  const float averageImg[3] = {93.5940, 129.1863, 104.7624};
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
              const cnstream::CNFrameInfoPtr& package, const std::shared_ptr<cnstream::CNInferObject>& obj) override;

  DECLARE_REFLEX_OBJECT_EX(PreprocVggface, cnstream::Preproc);
};  // class PreprocCpu

IMPLEMENT_REFLEX_OBJECT_EX(PreprocVggface, cnstream::Preproc)

int PreprocVggface::Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
                            const cnstream::CNFrameInfoPtr& package,
                            const std::shared_ptr<cnstream::CNInferObject>& obj) {
  // check params
  auto input_shapes = model->InputShapes();
  if (net_inputs.size() != 1 || input_shapes[0].c != 3) {
    LOG(ERROR) << "[PreprocCpu] model input shape not supported";
    return -1;
  }

  // get img size and model input size.
  int dst_w = input_shapes[0].w;
  int dst_h = input_shapes[0].h;

  // get the frame bgr data.
  cv::Mat img = *package->frame.ImageBGR();

  // cut the image
  auto box = obj->bbox;
  cv::Rect cut_rect(box.x, box.y, box.w, box.h);
  cv::Mat cut_img;
  img(cut_rect).copyTo(cut_img);

  // resize if needed
  if (cut_img.rows != dst_h || cut_img.cols != dst_w) {
    cv::resize(cut_img, cut_img, cv::Size(dst_w, dst_h));
  }

  // convert to float32
  cut_img.convertTo(cut_img, CV_32FC3);

  // mean subtract
  cv::Mat channels[3];
  cv::split(cut_img, channels);
  channels[0] -= averageImg[0];
  channels[1] -= averageImg[1];
  channels[2] -= averageImg[2];
  cv::merge(channels, 3, cut_img);

  // input network
  cv::Mat dst(dst_h, dst_w, CV_32FC3, net_inputs[0]);
  cut_img.copyTo(dst);
  return 0;
}
