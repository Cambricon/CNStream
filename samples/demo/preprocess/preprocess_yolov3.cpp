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

#include <algorithm>
#include <memory>
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
#include "preproc.hpp"

class PreprocYolov3 : public cnstream::Preproc {
 public:
  int Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
              const std::shared_ptr<cnstream::CNFrameInfo>& package) {
    // check params
    auto input_shapes = model->InputShapes();
    if (net_inputs.size() != 1 || input_shapes[0].c != 3) {
      LOG(ERROR) << "[PreprocCpu] model input shape not supported";
      return -1;
    }
    cnstream::CNDataFramePtr frame =
        cnstream::any_cast<cnstream::CNDataFramePtr>(package->datas[cnstream::CNDataFramePtrKey]);

    int width = frame->width;
    int height = frame->height;
    int dst_w = input_shapes[0].w;
    int dst_h = input_shapes[0].h;

    uint8_t* img_data = new (std::nothrow) uint8_t[frame->GetBytes()];
    if (!img_data) {
      LOG(ERROR) << "Failed to alloc memory, size:" << frame->GetBytes();
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

    // resize
    if (height != dst_h || width != dst_w) {
      cv::Mat dst(dst_h, dst_w, CV_8UC3, cv::Scalar(128, 128, 128));
      const float scaling_factors = std::min(1.0 * dst_w / width, 1.0 * dst_h / height);
      CHECK_GT(scaling_factors, 0);
      CHECK_LE(scaling_factors, 1);
      cv::Mat resized(height * scaling_factors, width * scaling_factors, CV_8UC3);
      cv::resize(img, resized, cv::Size(resized.cols, resized.rows));
      cv::Rect roi;
      roi.x = (dst.cols - resized.cols) / 2;
      roi.y = (dst.rows - resized.rows) / 2;
      roi.width = resized.cols;
      roi.height = resized.rows;

      resized.copyTo(dst(roi));
      img = dst;
    }

    // since model input data type is float, convert image to float
    cv::Mat dst(dst_h, dst_w, CV_32FC3, net_inputs[0]);
    img.convertTo(dst, CV_32F);

    delete[] img_data;
    return 0;
  }

 private:
  DECLARE_REFLEX_OBJECT_EX(PreprocYolov3, cnstream::Preproc);
};  // class PreprocYolov3

IMPLEMENT_REFLEX_OBJECT_EX(PreprocYolov3, cnstream::Preproc);
