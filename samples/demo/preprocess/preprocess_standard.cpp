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

#include <memory>
#include <utility>
#include <vector>

#ifdef HAVE_OPENCV
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#else
#error OpenCV required
#endif

#include "cnstream_frame_va.hpp"
#include "easyinfer/model_loader.h"
#include "easyinfer/shape.h"
#include "preproc.hpp"

/**
 * @brief standard pre process
 */
class PreprocCpu : public cnstream::Preproc {
 public:
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

  DECLARE_REFLEX_OBJECT_EX(PreprocCpu, cnstream::Preproc);
};  // class PreprocCpu

IMPLEMENT_REFLEX_OBJECT_EX(PreprocCpu, cnstream::Preproc)

int PreprocCpu::Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
                        const cnstream::CNFrameInfoPtr& package) {
  // check params
  auto input_shapes = model->InputShapes();
  if (net_inputs.size() != 1 || (input_shapes[0].c != 3 && input_shapes[0].c != 4)) {
    LOG(ERROR) << "[PreprocCpu] model input shape not supported, net_input.size = " << net_inputs.size()
               << ", input_shapes[0].c = " << input_shapes[0].c;
    return -1;
  }

  DLOG(INFO) << "[PreprocCpu] do preproc...";

  cnstream::CNDataFramePtr frame = cnstream::GetCNDataFramePtr(package);

  int width = frame->width;
  int height = frame->height;
  int dst_w = input_shapes[0].w;
  int dst_h = input_shapes[0].h;

  uint8_t* img_data = new (std::nothrow) uint8_t[frame->GetBytes()];
  if (!img_data) {
    LOG(ERROR) << "Failed to alloc memory, size: " << frame->GetBytes();
    return -1;
  }
  uint8_t* t = img_data;

  for (int i = 0; i < frame->GetPlanes(); ++i) {
    memcpy(t, frame->data[i]->GetCpuData(), frame->GetPlaneBytes(i));
    t += frame->GetPlaneBytes(i);
  }

  // convert color space
  cv::Mat img;
  switch (frame->fmt) {
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_BGR24:
      img = cv::Mat(height, width, CV_8UC3, img_data);
      break;
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_RGB24:
      img = cv::Mat(height, width, CV_8UC3, img_data);
      cv::cvtColor(img, img, cv::COLOR_RGB2BGR);
      break;
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12: {
      img = cv::Mat(height * 3 / 2, width, CV_8UC1, img_data);
      cv::Mat bgr(height, width, CV_8UC3);
      cv::cvtColor(img, bgr, cv::COLOR_YUV2BGR_NV12);
      img = bgr;
    } break;
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21: {
      img = cv::Mat(height * 3 / 2, width, CV_8UC1, img_data);
      cv::Mat bgr(height, width, CV_8UC3);
      cv::cvtColor(img, bgr, cv::COLOR_YUV2BGR_NV21);
      img = bgr;
    } break;
    default:
      LOG(WARNING) << "[Encoder] Unsupport pixel format.";
      delete[] img_data;
      return -1;
  }

  // resize if needed
  if (height != dst_h || width != dst_w) {
    cv::Mat dst(dst_h, dst_w, CV_8UC3);
    cv::resize(img, dst, cv::Size(dst_w, dst_h));
    img.release();
    img = dst;
  }

  // since model input data type is float, convert image to float
  cv::Mat dst(dst_h, dst_w, CV_32FC3, net_inputs[0]);
  img.convertTo(dst, CV_32F);

  delete[] img_data;
  return 0;
}

/**
 * @brief standard object pre process
 */
class ObjPreprocCpu : public cnstream::ObjPreproc {
 public:
  int Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& finfo, const std::shared_ptr<cnstream::CNInferObject>& pobj) override;

  DECLARE_REFLEX_OBJECT_EX(ObjPreprocCpu, cnstream::ObjPreproc);
};  // class ObjPreprocCpu

IMPLEMENT_REFLEX_OBJECT_EX(ObjPreprocCpu, cnstream::ObjPreproc)

int ObjPreprocCpu::Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
                           const cnstream::CNFrameInfoPtr& finfo,
                           const std::shared_ptr<cnstream::CNInferObject>& pobj) {
  cnstream::CNDataFramePtr frame = cnstream::GetCNDataFramePtr(finfo);
  // origin frame
  cv::Mat frame_bgr = *frame->ImageBGR();

  // crop objct from frame
  int w = frame->width;
  int h = frame->height;
  cv::Rect obj_roi(pobj->bbox.x * w, pobj->bbox.y * h, pobj->bbox.w * w, pobj->bbox.h * h);
  cv::Mat obj_bgr = frame_bgr(obj_roi);

  // resize
  int input_w = model->InputShapes()[0].w;
  int input_h = model->InputShapes()[0].h;
  cv::Mat obj_bgr_resized;
  cv::resize(obj_bgr, obj_bgr_resized, cv::Size(input_w, input_h));

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
