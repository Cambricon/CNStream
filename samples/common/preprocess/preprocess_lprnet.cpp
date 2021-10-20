/*************************************************************************
 * copyright (c) [2021] by cambricon, inc. all rights reserved
 *
 *  licensed under the apache license, version 2.0 (the "license");
 *  you may not use this file except in compliance with the license.
 *  you may obtain a copy of the license at
 *
 *     http://www.apache.org/licenses/license-2.0
 *
 * the above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the software.
 * the software is provided "as is", without warranty of any kind, express
 * or implied, including but not limited to the warranties of merchantability,
 * fitness for a particular purpose and noninfringement. in no event shall
 * the authors or copyright holders be liable for any claim, damages or other
 * liability, whether in an action of contract, tort or otherwise, arising from,
 * out of or in connection with the software or the use or other dealings in
 * the software.
 *************************************************************************/

#include <opencv2/opencv.hpp>

#include <memory>
#include <utility>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "preproc.hpp"

class PreprocLprnet : public cnstream::ObjPreproc {
 public:
  int Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& finfo, const std::shared_ptr<cnstream::CNInferObject>& pobj) override;

  DECLARE_REFLEX_OBJECT_EX(PreprocLprnet, cnstream::ObjPreproc);
};  // class PreprocLprnet

IMPLEMENT_REFLEX_OBJECT_EX(PreprocLprnet, cnstream::ObjPreproc)

int PreprocLprnet::Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
                        const cnstream::CNFrameInfoPtr& finfo,
                        const std::shared_ptr<cnstream::CNInferObject>& pobj) {
  cnstream::CNDataFramePtr frame = finfo->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag);
  // origin frame
  cv::Mat frame_bgr = frame->ImageBGR();

  // crop objct from frame
  int w = frame->width;
  int h = frame->height;
  cv::Rect obj_roi(pobj->bbox.x * w, pobj->bbox.y * h, pobj->bbox.w * w, pobj->bbox.h * h);
  cv::Mat obj_bgr = frame_bgr(obj_roi);

  // resize
  int input_w = model->InputShape(0).W();
  int input_h = model->InputShape(0).H();
  cv::Mat obj_bgr_resized;
  cv::resize(obj_bgr, obj_bgr_resized, cv::Size(input_h, input_w));

  // transpose
  cv::transpose(obj_bgr_resized, obj_bgr_resized);

  // bgr2bgra
  cv::Mat obj_bgra;
  cv::Mat a(input_h, input_w, CV_8UC1, cv::Scalar(0.0));
  std::vector<cv::Mat> vec_mat = {obj_bgr_resized, a};
  cv::merge(std::move(vec_mat), obj_bgra);

  // convert to float32, required by inferencer module
  cv::Mat obj_bgra_float32(input_h, input_w, CV_32FC4, net_inputs[0]);
  obj_bgra.convertTo(obj_bgra_float32, CV_32FC4);

  return 0;
}
