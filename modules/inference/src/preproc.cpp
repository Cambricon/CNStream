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

#include "preproc.hpp"

#include <opencv2/opencv.hpp>

#include "cnbase/cnshape.h"
#include "cninfer/model_loader.h"

namespace cnstream {

Preproc* Preproc::Create(const std::string& proc_name) {
  return libstream::ReflexObjectEx<Preproc>::CreateObject(proc_name);
}

IMPLEMENT_REFLEX_OBJECT_EX(PreprocCpu, Preproc)

int PreprocCpu::Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<libstream::ModelLoader>& model,
                        const CNFrameInfoPtr& package) {
  // check params
  auto input_shapes = model->input_shapes();
  if (net_inputs.size() != 1 || input_shapes[0].c() != 3) {
    LOG(ERROR) << "[PreprocCpu] model input shape not supported";
    return -1;
  }

  DLOG(INFO) << "[PreprocCpu] do preproc...";

  int width = package->frame.width;
  int height = package->frame.height;
  int dst_w = input_shapes[0].w();
  int dst_h = input_shapes[0].h();

  uint8_t* img_data = new uint8_t[package->frame.GetBytes()];
  uint8_t* t = img_data;

  for (int i = 0; i < package->frame.GetPlanes(); ++i) {
    memcpy(t, package->frame.data[i]->GetCpuData(), package->frame.GetPlaneBytes(i));
    t += package->frame.GetPlaneBytes(i);
  }

  // convert color space
  cv::Mat img;
  switch (package->frame.fmt) {
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

}  // namespace cnstream
