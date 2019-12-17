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
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "pose_utils.hpp"
#include "postproc.hpp"

namespace openpose {

class PostprocPose : public cnstream::Postproc {
 public:
  ~PostprocPose() {
    if (nms_output_blob_) {
      releaseBlobData(&nms_output_blob_);
    }
    if (input_blob_) {
      releaseBlobData(&input_blob_);
    }
    if (net_output_blob_) {
      releaseBlobData(&net_output_blob_);
    }

    if (input_img_data_) {
      delete[] input_img_data_;
      input_img_data_ = nullptr;
    }

    if (nchw_netout_data_) {
      delete[] nchw_netout_data_;
      nchw_netout_data_ = nullptr;
    }
  }

  int Execute(const std::vector<float *> &net_outputs, const std::shared_ptr<edk::ModelLoader> &model,
              const cnstream::CNFrameInfoPtr &package) override;

 private:
  BlobData *nms_output_blob_ = nullptr;
  BlobData *input_blob_ = nullptr;
  BlobData *net_output_blob_ = nullptr;
  uint8_t *input_img_data_ = nullptr;
  float *nchw_netout_data_ = nullptr;
  DECLARE_REFLEX_OBJECT_EX(PostprocPose, cnstream::Postproc)
};  //  class PostprocPose

IMPLEMENT_REFLEX_OBJECT_EX(PostprocPose, cnstream::Postproc)

int PostprocPose::Execute(const std::vector<float *> &net_outputs, const std::shared_ptr<edk::ModelLoader> &model,
                          const cnstream::CNFrameInfoPtr &package) {
  auto input_shapes = model->InputShapes();
  if (net_outputs.size() != 1) {
    LOG(ERROR) << "[Warning] OpenPose neuron network only has one output,"
                  " but get " +
                      std::to_string(net_outputs.size());
    return -1;
  }

  int input_width = model->InputShapes()[0].w;
  int input_height = model->InputShapes()[0].h;
  int output_n = model->OutputShapes()[0].n;
  int output_c = model->OutputShapes()[0].c;
  int output_h = model->OutputShapes()[0].h;
  int output_w = model->OutputShapes()[0].w;

  cv::Size netin_size = cv::Size(input_width, input_height);
  float *netout_data = net_outputs[0];
  int64_t data_count = model->OutputShapes()[0].DataCount();

  if (!nchw_netout_data_) {
    nchw_netout_data_ = new float[data_count];
  }
  if (!nms_output_blob_) {
    nms_output_blob_ = createBlobData(1, output_c - 1, POSE_MAX_PEOPLE + 1, 3);
  }
  if (!input_blob_) {
    input_blob_ = createBlobData(1, output_c, netin_size.height, netin_size.width);
  }
  if (!net_output_blob_) {
    net_output_blob_ = createBlobData(output_n, output_c, output_h, output_w);
  }

  memset(nms_output_blob_->list, 0,  // NOLINT
         nms_output_blob_->count * sizeof(float));
  memset(input_blob_->list, 0, input_blob_->count * sizeof(float));  // NOLINT
  memset(net_output_blob_->list, 0,                                  // NOLINT
         net_output_blob_->count * sizeof(float));

  // trans nhwc to nchw
  for (int i = 0; i < output_h; i++) {
    for (int j = 0; j < output_w; j++) {
      for (int k = 0; k < output_c; k++) {
        nchw_netout_data_[i * output_w + j + k * output_w * output_h] = netout_data[(i * output_w + j) * output_c + k];
      }
    }
  }

  memcpy(net_output_blob_->list, nchw_netout_data_,  //  NOLINT
         data_count * sizeof(float));
  int width = package->frame.width;
  int height = package->frame.height;
  if (!input_img_data_) {
    input_img_data_ = new uint8_t[package->frame.GetBytes()];
  }
  uint8_t *p = input_img_data_;
  memset(input_img_data_, 0, package->frame.GetBytes());  // NOLINT
  for (int i = 0; i < package->frame.GetPlanes(); i++) {
    memcpy(p, package->frame.data[i]->GetCpuData(),  // NOLINT
           package->frame.GetPlaneBytes(i));
    p += package->frame.GetPlaneBytes(i);
  }

  for (int i = 0; i < net_output_blob_->channels; ++i) {
    cv::Mat um(netin_size.height, netin_size.width, CV_32F,
               input_blob_->list + netin_size.height * netin_size.width * i);

    resize(cv::Mat(net_output_blob_->height, net_output_blob_->width, CV_32F,
                   net_output_blob_->list + net_output_blob_->width * net_output_blob_->height * i),
           um, netin_size, 0, 0, cv::INTER_CUBIC);
  }

  bool maxpositive = true;
  float num_threshold = getDefaultNmsThreshold(maxpositive);
  float interMinAboveThreshold = getDefaultConnectInterMinAboveThreshold(maxpositive);
  float connectInterThreshold = getDefaultConnectInterThreshold(maxpositive);
  float minSubsetCnt = getDefaultMinSubsetCnt(maxpositive);
  float connectMinSubsetScore = getDefaultConnectMinSubsetScore(maxpositive);

  nms(input_blob_, nms_output_blob_, num_threshold);

  std::vector<float> keypoints;
  std::vector<int> shape;
  shape.resize(3);

  connectBodyParts(&keypoints, input_blob_->list, nms_output_blob_->list, netin_size, POSE_MAX_PEOPLE,
                   interMinAboveThreshold, connectInterThreshold, minSubsetCnt, connectMinSubsetScore, 1, maxpositive,
                   &shape);

  // render key points on input img
  float render_threshold = getDefaultRenderThreshold();
  float scale = getScaleFactor(cv::Size(height, width), netin_size);
  renderPoseKeypoints(package->frame.ImageBGR(), keypoints, shape, render_threshold, scale);

  return 0;
}

}  //  namespace openpose
