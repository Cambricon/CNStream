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

#include "feature_extractor.hpp"

namespace cnstream {

FeatureExtractor::FeatureExtractor(const std::shared_ptr<edk::ModelLoader>& model_loader,
                                   int device_id)
    : model_loader_(model_loader) {
  if (!model_loader_) {
    LOGI(TRACK) << "[FeatureExtractor] Model not set, using opencv to extract feature on CPU";
  } else {
    device_id_ = device_id;

    // 1.Check model I/O
    if (model_loader_->InputNum() != 1) {
      LOGE(TRACK) << "[FeatureExtractor] model should have exactly one input";
      return;
    }

    // 2.prepare input and output memory
    mem_op_.SetModel(model_loader_);
    input_cpu_ptr_ = mem_op_.AllocCpuInput();
    input_mlu_ptr_ = mem_op_.AllocMluInput();
    output_mlu_ptr_ = mem_op_.AllocMluOutput();
    output_cpu_ptr_ = mem_op_.AllocCpuOutput();

    // 3.init cninfer
    infer_.Init(model_loader_, device_id_);
    LOGI(TRACK) << "[FeatureExtractor] to extract feature on MLU";
  }
}

FeatureExtractor::~FeatureExtractor() {
  LOGI(TRACK) << "[FeatureExtractor] release resources";
  if (model_loader_) {
    if (input_mlu_ptr_) mem_op_.FreeMluInput(input_mlu_ptr_);
    if (output_mlu_ptr_) mem_op_.FreeMluOutput(output_mlu_ptr_);
    if (input_cpu_ptr_) mem_op_.FreeCpuInput(input_cpu_ptr_);
    if (output_cpu_ptr_) mem_op_.FreeCpuOutput(output_cpu_ptr_);
    input_mlu_ptr_ = output_mlu_ptr_ = input_cpu_ptr_ = output_cpu_ptr_ = nullptr;
  }
}

void FeatureExtractor::ExtractFeature(const cv::Mat& image,
                                      const CNInferObjsPtr& objs_holder,
                                      std::vector<std::vector<float>>* features) {
  features->clear();
  if (!model_loader_) {
    ExtractFeatureOnCpu(image, objs_holder, features);
  } else {
    ExtractFeatureOnMlu(image, objs_holder, features);
  }
}

void FeatureExtractor::ExtractFeatureOnMlu(const cv::Mat& image,
                                           const CNInferObjsPtr& objs_holder,
                                           std::vector<std::vector<float>>* features) {
  uint32_t n = model_loader_->InputShapes()[0].n;
  std::vector<std::vector<float*>> batch_inputs;
  std::vector<std::vector<float>> batch_outputs;
  std::vector<cv::Mat> batch_mats;
  for (size_t i = 0; i < objs_holder->objs_.size(); i += n) {
    for (size_t j = 0; j < n; ++j) {
      size_t idx = i + j;
      if (idx < objs_holder->objs_.size()) {
        auto obj = objs_holder->objs_[idx];
        cv::Mat obj_image = CropImage(image, obj->bbox);
        cv::Mat preproc_image = Preprocess(obj_image);
        batch_mats.push_back(preproc_image);
        batch_inputs.push_back({reinterpret_cast<float*>(preproc_image.data)});
      }
    }
    RunBatch(batch_inputs, &batch_outputs);
    features->insert(features->end(), batch_outputs.begin(), batch_outputs.end());
    batch_inputs.clear();
    batch_outputs.clear();
    batch_mats.clear();
  }
}

void FeatureExtractor::ExtractFeatureOnCpu(const cv::Mat& image,
                                           const CNInferObjsPtr& objs_holder,
                                           std::vector<std::vector<float>>* features) {
  for (uint32_t num = 0; num < objs_holder->objs_.size(); ++num) {
    auto obj = objs_holder->objs_[num];
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
  if (image.rows != static_cast<int>(in_shape.h) || image.cols != static_cast<int>(in_shape.w)) {
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


int FeatureExtractor::RunBatch(const std::vector<std::vector<float*>>& inputs,
                               std::vector<std::vector<float>>* outputs) {
  if (inputs.empty()) {
    return 0;
  }

  // prepare inputs
  for (size_t i = 0; i < inputs.size(); ++i) {
    for (size_t j = 0; j < model_loader_->InputNum(); ++j) {
      uint64_t data_count = model_loader_->InputShapes()[j].hwc();
      float *cpu_input = static_cast<float*>(input_cpu_ptr_[j]) + i * data_count;
      memcpy(cpu_input, inputs[i][j], data_count * sizeof(float));
    }
  }

  // do copy and inference
  mem_op_.MemcpyInputH2D(input_mlu_ptr_, input_cpu_ptr_);
  infer_.Run(input_mlu_ptr_, output_mlu_ptr_);
  mem_op_.MemcpyOutputD2H(output_cpu_ptr_, output_mlu_ptr_);

  // parse outputs
  for (size_t i = 0; i < inputs.size(); ++i) {
    std::vector<std::vector<float>> output;
    for (size_t j = 0; j < model_loader_->OutputNum(); ++j) {
      uint64_t data_count = model_loader_->OutputShapes()[j].hwc();
      float *cpu_output = static_cast<float*>(output_cpu_ptr_[j]) + i * data_count;
      output.push_back(std::vector<float>(cpu_output, cpu_output + data_count));
    }
    outputs->push_back(output[0]);
  }
  return 0;
}


}  // namespace cnstream
