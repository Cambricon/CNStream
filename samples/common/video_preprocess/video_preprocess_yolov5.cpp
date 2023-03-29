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

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "cnis/contrib/video_helper.h"
#include "cnstream_frame_va.hpp"
#include "cnstream_logging.hpp"
#include "video_preproc.hpp"
#include "video_preprocess_common.hpp"

/**
 * @brief Video preprocessing for YOLOv5 network
 */
class VideoPreprocYolov5 : public cnstream::VideoPreproc {
 public:
  /**
   * @brief Execute YOLOv5 network preprocessing.
   *
   * @param[out] model_input: The input of network. The preproc result should be set to it.
   * @param[in] input_data: The raw input data.
   * @param[in] model_info: The model information, e.g., input/output number, shape and etc.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   */
  bool Execute(infer_server::ModelIO* model_input, const infer_server::InferData& input_data,
               const infer_server::ModelInfo* model_info) {
    // check model input number and shape
    uint32_t input_num = model_info->InputNum();
    if (input_num != 1) {
      LOGE(DEMO) << "[VideoPreprocYolov5] model input number not supported. It should be 1, but " << input_num;
      return false;
    }
    infer_server::Shape input_shape;
    input_shape = model_info->InputShape(0);
    int c_idx = 3;
    int w_idx = 2;
    int h_idx = 1;
    if (model_info->InputLayout(0).order == infer_server::DimOrder::NCHW) {
      c_idx = 1;
      w_idx = 3;
      h_idx = 2;
    }
    if (input_shape[c_idx] != 3) {
      LOGE(DEMO) << "[VideoPreprocYolov5] model input shape not supported, `c` should be 3, but " << input_shape[c_idx];
      return false;
    }
    // do preproc
    const infer_server::video::VideoFrame& frame = input_data.GetLref<infer_server::video::VideoFrame>();

    size_t src_w = frame.width;
    size_t src_h = frame.height;
    size_t src_stride = frame.stride[0];
    uint32_t dst_w = input_shape[w_idx];
    uint32_t dst_h = input_shape[h_idx];
    uint8_t* img_data = new (std::nothrow) uint8_t[frame.GetTotalSize()];
    if (!img_data) {
      LOGE(DEMO) << "[VideoPreprocYolov5] Failed to alloc memory, size: " << frame.GetTotalSize();
      return false;
    }
    uint8_t* img_data_tmp = img_data;

    for (auto plane_idx = 0u; plane_idx < frame.plane_num; ++plane_idx) {
      memcpy(img_data_tmp, frame.plane[plane_idx].Data(), frame.GetPlaneSize(plane_idx));
      img_data_tmp += frame.GetPlaneSize(plane_idx);
    }

    // convert color space from src to dst
    cv::Mat dst_cvt_color_img;
    if (!ConvertColorSpace(src_w, src_h, src_stride, frame.format, model_input_pixel_format_, img_data,
                           &dst_cvt_color_img)) {
      LOGW(DEMO) << "[VideoPreprocYolov5] Unsupport pixel format. src: " << static_cast<int>(frame.format)
                 << " dst: " << static_cast<int>(model_input_pixel_format_);
      delete[] img_data;
      return false;
    }

    cv::Mat img = dst_cvt_color_img;
    // resize
    if (src_h != dst_h || src_w != dst_w) {
      cv::Mat dst(dst_h, dst_w, CV_8UC3, cv::Scalar(0, 0, 0));
      const float scaling_factors = std::min(1.0 * dst_w / src_w, 1.0 * dst_h / src_h);
      cv::Mat resized(src_h * scaling_factors, src_w * scaling_factors, CV_8UC3);
      cv::resize(img, resized, cv::Size(resized.cols, resized.rows));
      cv::Rect roi;
      roi.x = (dst.cols - resized.cols) / 2;
      roi.y = (dst.rows - resized.rows) / 2;
      roi.width = resized.cols;
      roi.height = resized.rows;

      resized.copyTo(dst(roi));
      img = dst;
    }

    // copy data to model_input buffer
    cv::Mat dst(dst_h, dst_w, CV_32FC3, model_input->buffers[0].MutableData());
    img.convertTo(dst, CV_32FC3);
    dst /= 255.0;

    delete[] img_data;
    return true;
  }

 private:
  DECLARE_REFLEX_OBJECT_EX(VideoPreprocYolov5, cnstream::VideoPreproc);
};  // class VideoPreprocYolov5

IMPLEMENT_REFLEX_OBJECT_EX(VideoPreprocYolov5, cnstream::VideoPreproc);
