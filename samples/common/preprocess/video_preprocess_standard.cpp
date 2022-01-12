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

#include <memory>
#include <string>
#include <utility>
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
 * @brief Video standard preprocessing
 */
class VideoPreprocCpu : public cnstream::VideoPreproc {
 public:
  /**
   * @brief Execute standard preprocessing
   *
   * @param model_input: the input of neural network. The preproc result should be set to it.
   * @param input_data: the raw input data. The user could get infer_server::video::VideoFrame object from it.
   * @param model_info: model information, e.g., input/output number, shape and etc.
   *
   * @return return true if succeed
   */
  bool Execute(infer_server::ModelIO* model_input, const infer_server::InferData& input_data,
               const infer_server::ModelInfo* model_info) override;

  DECLARE_REFLEX_OBJECT_EX(VideoPreprocCpu, cnstream::VideoPreproc);
};  // class VideoPreprocCpu

IMPLEMENT_REFLEX_OBJECT_EX(VideoPreprocCpu, cnstream::VideoPreproc)

bool VideoPreprocCpu::Execute(infer_server::ModelIO* model_input, const infer_server::InferData& input_data,
                              const infer_server::ModelInfo* model_info) {
  // check model input number and shape
  uint32_t input_num = model_info->InputNum();
  if (input_num != 1) {
    LOGE(DEMO) << "[VideoPreprocCpu] model input number not supported. It should be 1, but " << input_num;
    return false;
  }
  infer_server::Shape input_shape;
  input_shape = model_info->InputShape(0);
  int w_idx = 2;
  int h_idx = 1;
  int c_idx = 3;
  if (model_info->InputLayout(0).order == infer_server::DimOrder::NCHW) {
    w_idx = 3;
    h_idx = 2;
    c_idx = 1;
  }
  if (input_shape[c_idx] != 4) {
    LOGE(DEMO) << "[VideoPreprocCpu] model input shape not supported, `c` should be 4, but " << input_shape[c_idx];
    return false;
  }
  if (model_info->InputLayout(0).dtype != infer_server::DataType::UINT8 &&
      model_info->InputLayout(0).dtype != infer_server::DataType::FLOAT32) {
    std::string dtype_str = "";
      switch (model_info->InputLayout(0).dtype) {
        case infer_server::DataType::FLOAT16: dtype_str = "FLOAT16"; break;
        case infer_server::DataType::INT16: dtype_str = "INT16"; break;
        case infer_server::DataType::INT32: dtype_str = "INT32"; break;
        case infer_server::DataType::INVALID: dtype_str = "INVALID"; break;
        default: dtype_str = "UNKNOWN"; break;
      }
      LOGE(DEMO) << "[VideoPreprocCpu] model input data type not supported. It should be uint8/float32, but "
                 << dtype_str;
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
    LOGE(DEMO) << "[VideoPreprocCpu] Failed to alloc memory, size: " << frame.GetTotalSize();
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
    LOGW(DEMO) << "[VideoPreprocCpu] Unsupport pixel format. src: " << static_cast<int>(frame.format)
                << " dst: " << static_cast<int>(model_input_pixel_format_);
    delete[] img_data;
    return false;
  }

  cv::Mat dst_resized_img = dst_cvt_color_img;
  if (src_h != dst_h || src_w != dst_w) {
    cv::Mat resized_img(dst_h, dst_w, dst_cvt_color_img.type());
    cv::resize(dst_cvt_color_img, resized_img, cv::Size(dst_w, dst_h));
    dst_resized_img = resized_img;
  }

  // copy data to model_input buffer
  if (model_info->InputLayout(0).dtype == infer_server::DataType::FLOAT32) {
    // input data type is float32
    if (dst_resized_img.channels() == 4) {
      cv::Mat dst_img(dst_h, dst_w, CV_32FC4, model_input->buffers[0].MutableData());
      dst_resized_img.convertTo(dst_img, CV_32FC4);
    } else {
      cv::Mat dst_img(dst_h, dst_w, CV_32FC3, model_input->buffers[0].MutableData());
      dst_resized_img.convertTo(dst_img, CV_32FC3);
    }
  } else {
    // input data type is uint8
    cv::Mat dst_img(dst_h, dst_w, dst_resized_img.type(), model_input->buffers[0].MutableData());
    dst_resized_img.copyTo(dst_img);
  }

  delete[] img_data;
  return true;
}

/**
 * @brief Video standard object preprocessing
 */
class VideoObjPreprocCpu : public cnstream::VideoPreproc {
 public:
   /**
   * @brief Execute standard preprocessing for secondary neural network
   *
   * @param model_input: the input of neural network. The preproc result should be set to it.
   * @param input_data: the raw input data. The user could get infer_server::video::VideoFrame object from it.
   * @param model_info: model information, e.g., input/output number, shape and etc.
   *
   * @return return true if succeed
   */
  bool Execute(infer_server::ModelIO* model_input, const infer_server::InferData& input_data,
               const infer_server::ModelInfo* model_info) override;

  DECLARE_REFLEX_OBJECT_EX(VideoObjPreprocCpu, cnstream::VideoPreproc);
};  // class VideoObjPreprocCpu

IMPLEMENT_REFLEX_OBJECT_EX(VideoObjPreprocCpu, cnstream::VideoPreproc)

bool VideoObjPreprocCpu::Execute(infer_server::ModelIO* model_input, const infer_server::InferData& input_data,
                                 const infer_server::ModelInfo* model_info) {
  // check model input number and shape
  uint32_t input_num = model_info->InputNum();
  if (input_num != 1) {
    LOGE(DEMO) << "[VideoObjPreprocCpu] model input number not supported. It should be 1, but " << input_num;
    return false;
  }
  infer_server::Shape input_shape;
  input_shape = model_info->InputShape(0);
  int c_idx = 3;
  int w_idx = 2;
  int h_idx = 1;
  if (model_info->InputLayout(0).order == infer_server::DimOrder::NCHW) {
    w_idx = 3;
    h_idx = 2;
    c_idx = 1;
  }
  if (input_shape[c_idx] != 4) {
    LOGE(DEMO) << "[VideoObjPreprocCpu] model input shape not supported, `c` should be 4, but " << input_shape[c_idx];
    return false;
  }
  if (model_info->InputLayout(0).dtype != infer_server::DataType::UINT8 &&
      model_info->InputLayout(0).dtype != infer_server::DataType::FLOAT32) {
    std::string dtype_str = "";
      switch (model_info->InputLayout(0).dtype) {
        case infer_server::DataType::FLOAT16: dtype_str = "FLOAT16"; break;
        case infer_server::DataType::INT16: dtype_str = "INT16"; break;
        case infer_server::DataType::INT32: dtype_str = "INT32"; break;
        case infer_server::DataType::INVALID: dtype_str = "INVALID"; break;
        default: dtype_str = "UNKNOWN"; break;
      }
      LOGE(DEMO) << "[VideoObjPreprocCpu] model input data type not supported. It should be uint8/float32, but "
                 << dtype_str;
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
    LOGE(DEMO) << "[VideoObjPreprocCpu] Failed to alloc memory, size: " << frame.GetTotalSize();
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
    LOGW(DEMO) << "[VideoObjPreprocCpu] Unsupport pixel format. src: " << static_cast<int>(frame.format)
                << " dst: " << static_cast<int>(model_input_pixel_format_);
    delete[] img_data;
    return false;
  }

  cv::Rect obj_roi(src_w * frame.roi.x, src_h * frame.roi.y, src_w * frame.roi.w, src_h * frame.roi.h);
  cv::Mat obj_img = dst_cvt_color_img(obj_roi);

  cv::Mat dst_obj_resized_img = obj_img;
  if ((unsigned)obj_img.rows != dst_h || (unsigned)obj_img.cols != dst_w) {
    cv::Mat resized_img(dst_h, dst_w, obj_img.type());
    cv::resize(obj_img, resized_img, cv::Size(dst_w, dst_h));
    dst_obj_resized_img.release();
    dst_obj_resized_img = resized_img;
  }

  // copy data to model_input buffer
  if (model_info->InputLayout(0).dtype == infer_server::DataType::FLOAT32) {
    // input data type is float32
    if (dst_obj_resized_img.channels() == 4) {
      cv::Mat dst_img(dst_h, dst_w, CV_32FC4, model_input->buffers[0].MutableData());
      dst_obj_resized_img.convertTo(dst_img, CV_32FC4);
    } else {
      cv::Mat dst_img(dst_h, dst_w, CV_32FC3, model_input->buffers[0].MutableData());
      dst_obj_resized_img.convertTo(dst_img, CV_32FC3);
    }
  } else {
    // input data type is uint8
    cv::Mat dst_img(dst_h, dst_w, dst_obj_resized_img.type(), model_input->buffers[0].MutableData());
    dst_obj_resized_img.copyTo(dst_img);
  }

  delete[] img_data;
  return true;
}
