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
#include <memory>
#include <vector>
#include "preproc.hpp"

namespace iva {

class PreprocMultiStep : public cnstream::Preproc {
 public:
  DECLARE_REFLEX_OBJECT_EX(iva::PreprocMultiStep, cnstream::Preproc);

  int Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& package, cnstream::CNInferBoundingBox bbox) override;
};  // class PreprocMultiStep

IMPLEMENT_REFLEX_OBJECT_EX(iva::PreprocMultiStep, cnstream::Preproc)

int PreprocMultiStep::Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
                              const cnstream::CNFrameInfoPtr& package, cnstream::CNInferBoundingBox bbox) {
  // get the frame bgr data.
  const auto input_shapes = model->InputShapes();
  cv::Mat img = *package->frame.ImageBGR();

  int32_t box_x = bbox.x * img.cols;
  int32_t box_y = bbox.y * img.rows;
  int32_t box_w = bbox.w * img.cols;
  int32_t box_h = bbox.h * img.rows;
  box_x = box_x < 0 ? 0 : box_x;
  box_y = box_y < 0 ? 0 : box_y;
  box_w = box_x + box_w > img.cols ? img.cols - box_x : box_w;
  box_h = box_y + box_h > img.rows ? img.rows - box_y : box_h;

  cv::Rect cut_rect(box_x, box_y, box_w, box_h);
  cv::Mat cut_img;
  img(cut_rect).copyTo(cut_img);
  // resize
  int dst_w = input_shapes[0].w;
  int dst_h = input_shapes[0].h;
  cv::Mat resized;
  if (cut_img.rows != dst_h || cut_img.cols != dst_w) {
    cv::resize(cut_img, resized, cv::Size(dst_w, dst_h));
  } else {
    resized = cut_img;
  }
  cv::Mat dst(dst_h, dst_w, CV_32FC3, net_inputs[0]);
  resized.convertTo(dst, CV_32F);

  return 0;
}

}  // namespace iva
