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
#include "cnstream_logging.hpp"
#include "video_helper.h"
#include "video_preproc.hpp"
#include "video_preprocess_common.hpp"

/**
 * @brief Video preprocessing for YOLOv3 neural network
 */
class VideoPreprocYolov3 : public cnstream::VideoPreproc {
 public:
  /**
   * @brief Execute YOLOv3 neural network preprocessing
   *
   * @param model_input: the input of neural network. The preproc result should be set to it.
   * @param input_data: the raw input data. The user could get infer_server::video::VideoFrame object from it.
   * @param model_info: model information, e.g., input/output number, shape and etc.
   *
   * @return return true if succeed
   */
  bool Execute(infer_server::ModelIO* model_input, const infer_server::InferData& input_data,
               const infer_server::ModelInfo& model_info) {
    // check model input number and shape
    uint32_t input_num = model_info.InputNum();
    if (input_num != 1) {
      LOGE(DEMO) << "[VideoPreprocYolov3] model input number not supported. It should be 1, but " << input_num;
      return false;
    }
    infer_server::Shape input_shape;
    input_shape = model_info.InputShape(0);
    if (input_shape.GetC() != 4) {
      LOGE(DEMO) << "[VideoPreprocYolov3] model input shape not supported, `c` should be 4, but " << input_shape.GetC();
      return false;
    }
    if (model_info.InputLayout(0).dtype != infer_server::DataType::UINT8 &&
      model_info.InputLayout(0).dtype != infer_server::DataType::FLOAT32) {
      std::string dtype_str = "";
        switch (model_info.InputLayout(0).dtype) {
          case infer_server::DataType::FLOAT16: dtype_str = "FLOAT16"; break;
          case infer_server::DataType::INT16: dtype_str = "INT16"; break;
          case infer_server::DataType::INT32: dtype_str = "INT32"; break;
          case infer_server::DataType::INVALID: dtype_str = "INVALID"; break;
          default: dtype_str = "UNKNOWN"; break;
        }
        LOGE(DEMO) << "[VideoPreprocYolov3] model input data type not supported. It should be uint8/float32, but "
                  << dtype_str;
        return false;
    }

    // do preproc
    const infer_server::video::VideoFrame& frame = input_data.GetLref<infer_server::video::VideoFrame>();

    size_t src_w = frame.width;
    size_t src_h = frame.height;
    uint32_t dst_w = input_shape.GetW();
    uint32_t dst_h = input_shape.GetH();

    uint8_t* img_data = new (std::nothrow) uint8_t[frame.GetTotalSize()];
    if (!img_data) {
      LOGE(DEMO) << "[VideoPreprocYolov3] Failed to alloc memory, size: " << frame.GetTotalSize();
      return false;
    }
    uint8_t* img_data_tmp = img_data;

    for (auto plane_idx = 0u; plane_idx < frame.plane_num; ++plane_idx) {
      memcpy(img_data_tmp, frame.plane[plane_idx].Data(), frame.GetPlaneSize(plane_idx));
      img_data_tmp += frame.GetPlaneSize(plane_idx);
    }

    // convert color space from src to dst
    cv::Mat dst_cvt_color_img;
    if (!ConvertColorSpace(src_w, src_h, frame.format, model_input_pixel_format_, img_data, &dst_cvt_color_img)) {
      LOGW(DEMO) << "[VideoPreprocYolov3] Unsupport pixel format. src: " << static_cast<int>(frame.format)
                 << " dst: " << static_cast<int>(model_input_pixel_format_);
      delete[] img_data;
      return false;
    }

    cv::Mat dst_pad_img = dst_cvt_color_img;
    if (src_h != dst_h || src_w != dst_w) {
      cv::Mat pad_img(dst_h, dst_w, dst_cvt_color_img.type(), cv::Scalar(128, 128, 128));
      const float scaling_factors = std::min(1.0 * dst_w / src_w, 1.0 * dst_h / src_h);
      LOGF_IF(DEMO, scaling_factors <= 0);
      LOGF_IF(DEMO, scaling_factors > 1);
      cv::Mat resized_img(src_h * scaling_factors, src_w * scaling_factors, dst_cvt_color_img.type());
      cv::resize(dst_cvt_color_img, resized_img, cv::Size(resized_img.cols, resized_img.rows));
      cv::Rect roi;
      roi.x = (pad_img.cols - resized_img.cols) / 2;
      roi.y = (pad_img.rows - resized_img.rows) / 2;
      roi.width = resized_img.cols;
      roi.height = resized_img.rows;
      resized_img.copyTo(pad_img(roi));
      dst_pad_img = pad_img;
    }

    // copy data to model_input buffer
    if (model_info.InputLayout(0).dtype == infer_server::DataType::FLOAT32) {
      // input data type is float32
      if (dst_pad_img.channels() == 4) {
        cv::Mat dst_img(dst_h, dst_w, CV_32FC4, model_input->buffers[0].MutableData());
        dst_pad_img.convertTo(dst_img, CV_32FC4);
      } else {
        cv::Mat dst_img(dst_h, dst_w, CV_32FC3, model_input->buffers[0].MutableData());
        dst_pad_img.convertTo(dst_img, CV_32FC3);
      }
    } else {
      // input data type is uint8
      cv::Mat dst_img(dst_h, dst_w, dst_pad_img.type(), model_input->buffers[0].MutableData());
      dst_pad_img.copyTo(dst_img);
    }

    delete[] img_data;
    return true;
  }

 private:
  DECLARE_REFLEX_OBJECT_EX(VideoPreprocYolov3, cnstream::VideoPreproc);
};  // class VideoPreprocYolov3

IMPLEMENT_REFLEX_OBJECT_EX(VideoPreprocYolov3, cnstream::VideoPreproc);
