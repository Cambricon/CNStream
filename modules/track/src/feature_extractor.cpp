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
#include <string>
#include <vector>

#ifdef HAVE_OPENCV
#include <opencv2/features2d/features2d.hpp>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#else
#error OpenCV required
#endif

#include "cnstream_frame_va.hpp"
#include "feature_extractor.hpp"

namespace cnstream {

FeatureExtractor::FeatureExtractor(const std::shared_ptr<edk::ModelLoader>& model_loader, uint32_t batch_size,
                                   int device_id)
    : model_loader_(model_loader) {
  if (!model_loader_) {
    LOG(INFO) << "[FeatureExtractor] Model not set, using opencv to extract feature on CPU";
  } else {
    model_loader_->InitLayout();
    device_id_ = device_id;
    batch_size_ = batch_size;

    // 1.Check model I/O
    if (model_loader_->InputNum() != 1) {
      LOG(ERROR) << "[FeatureExtractor] model should have exactly one input";
      return;
    }

    // 2.prepare input and output memory
    mem_op_.SetLoader(model_loader_);
    input_cpu_ptr_ = mem_op_.AllocCpuInput(batch_size_);
    input_mlu_ptr_ = mem_op_.AllocMluInput(batch_size_);
    output_mlu_ptr_ = mem_op_.AllocMluOutput(batch_size_);
    output_cpu_ptr_ = mem_op_.AllocCpuOutput(batch_size_);

    // 3.init cninfer
    infer_.Init(model_loader_, batch_size_, device_id_);
    LOG(INFO) << "[FeatureExtractor] to extract feature on MLU";
  }
}

FeatureExtractor::~FeatureExtractor() {
  LOG(INFO) << "[FeatureExtractor] release resources";
  if (model_loader_) {
    if (input_mlu_ptr_) mem_op_.FreeArrayMlu(input_mlu_ptr_, model_loader_->InputNum());
    if (output_mlu_ptr_) mem_op_.FreeArrayMlu(output_mlu_ptr_, model_loader_->OutputNum());
    if (input_cpu_ptr_) mem_op_.FreeCpuInput(input_cpu_ptr_);
    if (output_cpu_ptr_) mem_op_.FreeCpuOutput(output_cpu_ptr_);
    input_mlu_ptr_ = output_mlu_ptr_ = input_cpu_ptr_ = output_cpu_ptr_ = nullptr;
  }
}

void FeatureExtractor::ExtractFeature(CNFrameInfoPtr data, ThreadSafeVector<std::shared_ptr<CNInferObject>>& inputs,
                                      std::vector<std::vector<float>>* features) {
  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
  if (!model_loader_) {
    ExtractFeatureOnCpu(*frame->ImageBGR(), inputs, features);
  } else {
    ExtractFeatureOnMlu(data, inputs, features);
  }
}

void FeatureExtractor::ExtractFeatureOnMlu(CNFrameInfoPtr data,
                                           ThreadSafeVector<std::shared_ptr<CNInferObject>>& inputs,
                                           std::vector<std::vector<float>>* features) {
  uint64_t data_count = model_loader_->InputShapes()[0].hwc();
  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
  for (uint32_t i = 0; i < inputs.size(); ++i) {
    auto obj = inputs[i];
    cv::Mat obj_image = CropImage(*(frame->ImageBGR()), obj->bbox);
    // do pre-process

    cv::Mat preproc_image = Preprocess(obj_image);
    float* cpu_input = static_cast<float*>(input_cpu_ptr_[0]);
    memcpy(cpu_input, preproc_image.data, data_count * sizeof(float));

    // do copy and inference
    mem_op_.MemcpyInputH2D(input_mlu_ptr_, input_cpu_ptr_, batch_size_);
    infer_.Run(input_mlu_ptr_, output_mlu_ptr_);
    mem_op_.MemcpyOutputD2H(output_cpu_ptr_, output_mlu_ptr_, batch_size_);

    // do post-process
    const float* begin = reinterpret_cast<float*>(output_cpu_ptr_[0]);
    const float* end = begin + model_loader_->OutputShapes()[0].hwc();
    features->push_back(std::vector<float>(begin, end));
  }
}

void FeatureExtractor::ExtractFeatureOnCpu(const cv::Mat& image,
                                           ThreadSafeVector<std::shared_ptr<CNInferObject>>& inputs,
                                           std::vector<std::vector<float>>* features) {
  for (uint32_t num = 0; num < inputs.size(); ++num) {
    auto obj = inputs[num];
    cv::Rect rect = cv::Rect(obj->bbox.x * image.cols, obj->bbox.y * image.rows, obj->bbox.w * image.cols,
                             obj->bbox.h * image.rows);
    cv::Mat obj_img(image, rect);
#if (CV_MAJOR_VERSION == 2)  // NOLINT
    cv::Ptr<cv::ORB> processer = new cv::ORB(128);
#elif (CV_MAJOR_VERSION >= 3)  //  NOLINT
    cv::Ptr<cv::ORB> processer = cv::ORB::create(128);
#endif
    std::vector<cv::KeyPoint> keypoints;
    processer->detect(obj_img, keypoints);
    cv::Mat desc;
    processer->compute(obj_img, keypoints, desc);
    std::vector<float> feature;
    for (int i = 0; i < 128; i++) {
      feature.push_back((i < desc.rows ? CalcFeatureOfRow(desc, i) : 0));
    }
    features->push_back(feature);
  }
}

cv::Mat FeatureExtractor::CropImage(const cv::Mat& image, const CNInferBoundingBox& bbox) {
  int x = CLIP(bbox.x) * image.cols;
  int y = CLIP(bbox.y) * image.rows;
  int w = CLIP(bbox.w) * image.cols;
  int h = CLIP(bbox.h) * image.rows;

  cv::Rect rect(x, y, w, h);
  return image(rect);
}

cv::Mat FeatureExtractor::Preprocess(const cv::Mat& image) {
  // resize image
  edk::Shape in_shape = model_loader_->InputShapes()[0];
  cv::Mat image_resized;
  if (image.rows != static_cast<int>(in_shape.h || image.cols != static_cast<int>(in_shape.w))) {
    cv::resize(image, image_resized, cv::Size(in_shape.w, in_shape.h));
  } else {
    image_resized = image;
  }

  // convert data type to float 32 and normalize
  cv::Mat image_normalized;
  image_resized.convertTo(image_normalized, CV_32FC3, 1 / 255.0);
  cv::cvtColor(image_normalized, image_normalized, CV_BGR2BGRA);
  return image_normalized;
}

float FeatureExtractor::CalcFeatureOfRow(const cv::Mat& image, int n) {
  float result = 0;
  for (int i = 0; i < image.cols; i++) {
    int grey = image.ptr<uchar>(n)[i];
    result += grey > 127 ? static_cast<float>(grey) / 255 : -static_cast<float>(grey) / 255;
  }
  return result;
}

}  // namespace cnstream
