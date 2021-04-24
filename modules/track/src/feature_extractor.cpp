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

const int kFeatureSizeForCpu = 512;

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
    if (model_loader_->OutputNum() != 1) {
      LOGE(TRACK) << "[FeatureExtractor] model should have exactly one output";
      return;
    }

    // 2.prepare input and output memory
    mem_op_.SetModel(model_loader_);
    input_mlu_ptr_ = mem_op_.AllocMluInput();
    output_mlu_ptr_ = mem_op_.AllocMluOutput();
    output_cpu_ptr_ = mem_op_.AllocCpuOutput();

    // 3.init cninfer
    infer_.Init(model_loader_, device_id_);
    LOGI(TRACK) << "[FeatureExtractor] to extract feature on MLU";

    batch_size_ = static_cast<int>(model_loader_->InputShape(0).N());
  }
}

FeatureExtractor::~FeatureExtractor() {
  LOGI(TRACK) << "[FeatureExtractor] release resources";
  if (model_loader_) {
    if (input_mlu_ptr_) mem_op_.FreeMluInput(input_mlu_ptr_);
    if (output_mlu_ptr_) mem_op_.FreeMluOutput(output_mlu_ptr_);
    if (output_cpu_ptr_) mem_op_.FreeCpuOutput(output_cpu_ptr_);
    input_mlu_ptr_ = output_mlu_ptr_ = output_cpu_ptr_ = nullptr;
    if (rc_) {
      rc_->Destroy();
    }
  }
}

bool FeatureExtractor::Init(CNDataFormat src_fmt, edk::CoreVersion core_ver) {
  if (!model_loader_) {
    return true;
  }
  if (is_initialized_) {
    LOGW(TRACK) << "[FeatureExtractor] should not init twice.";
  }
  edk::MluResizeConvertOp::Attr attr;
  attr.core_version = core_ver;

  if (src_fmt == CN_PIXEL_FORMAT_YUV420_NV21) {
    attr.color_mode = edk::MluResizeConvertOp::ColorMode::YUV2ARGB_NV21;
  } else if (src_fmt == CN_PIXEL_FORMAT_YUV420_NV12) {
    attr.color_mode = edk::MluResizeConvertOp::ColorMode::YUV2ARGB_NV12;
  } else {
    LOGE(TRACK) << "[FeatureExtractor] Init failed, unsupported src format.";
    return false;
  }

  attr.data_mode = edk::MluResizeConvertOp::DataMode::UINT8ToUINT8;
  attr.dst_w = static_cast<int>(model_loader_->InputShape(0).W());
  attr.dst_h = static_cast<int>(model_loader_->InputShape(0).H());
  attr.batch_size = batch_size_;
  attr.keep_aspect_ratio = false;

  rc_.reset(new edk::MluResizeConvertOp);
  if (rc_->Init(attr)) {
    is_initialized_ = true;
    return true;
  }
  return false;
}

void FeatureExtractor::ExtractFeature(const CNDataFramePtr& frame,
                                      const CNInferObjsPtr& objs_holder,
                                      std::vector<std::vector<float>>* features) {
  features->clear();
  if (!model_loader_) {
    ExtractFeatureOnCpu(frame, objs_holder, features);
  } else {
    ExtractFeatureOnMlu(frame, objs_holder, features);
  }
}

void FeatureExtractor::ExtractFeatureOnMlu(const CNDataFramePtr& frame,
                                           const CNInferObjsPtr& objs_holder,
                                           std::vector<std::vector<float>>* features) {
  if (!is_initialized_) {
    LOGW(TRACK) << "[FeatureExtractor] Please Init first.";
    return;
  }
  std::vector<std::vector<float>> batch_outputs;
  edk::MluResizeConvertOp::InputData input_data;
  for (unsigned idx = 0; idx < objs_holder->objs_.size(); ++idx) {
    auto &obj = objs_holder->objs_[idx];
    input_data.src_w = frame->width;
    input_data.src_h = frame->height;
    input_data.src_stride = frame->stride[0];
    input_data.crop_x = obj->bbox.x * frame->width;
    input_data.crop_y = obj->bbox.y * frame->height;
    input_data.crop_w = obj->bbox.w * frame->width;
    input_data.crop_h = obj->bbox.h * frame->height;
    input_data.planes[0] = const_cast<void*>(frame->data[0]->GetMluData());
    input_data.planes[1] = const_cast<void*>(frame->data[1]->GetMluData());
    rc_->BatchingUp(input_data);
    if ((idx + 1) % batch_size_ == 0 || (idx + 1) == objs_holder->objs_.size()) {
      if (!rc_->SyncOneOutput(input_mlu_ptr_[0])) {
        LOGE(TRACK) << "[FeatureExtractor] RC: " << rc_->GetLastError();
      }
      uint32_t batch_input_size = idx % batch_size_ + 1;
      RunBatch(batch_input_size, &batch_outputs);
      features->insert(features->end(), batch_outputs.begin(), batch_outputs.end());
      batch_outputs.clear();
    }
  }
}

void FeatureExtractor::ExtractFeatureOnCpu(const CNDataFramePtr& frame,
                                           const CNInferObjsPtr& objs_holder,
                                           std::vector<std::vector<float>>* features) {
  const cv::Mat image = frame->ImageBGR();
  for (uint32_t num = 0; num < objs_holder->objs_.size(); ++num) {
    auto obj = objs_holder->objs_[num];
    cv::Rect rect = cv::Rect(obj->bbox.x * image.cols, obj->bbox.y * image.rows, obj->bbox.w * image.cols,
                             obj->bbox.h * image.rows);
    cv::Mat obj_img(image, rect);
#if (CV_MAJOR_VERSION == 2)  // NOLINT
    cv::Ptr<cv::ORB> processer = new cv::ORB(kFeatureSizeForCpu);
#elif (CV_MAJOR_VERSION >= 3)  //  NOLINT
    cv::Ptr<cv::ORB> processer = cv::ORB::create(kFeatureSizeForCpu);
#endif
    std::vector<cv::KeyPoint> keypoints;
    processer->detect(obj_img, keypoints);
    cv::Mat desc;
    processer->compute(obj_img, keypoints, desc);
    std::vector<float> feature;
    for (int i = 0; i < kFeatureSizeForCpu; i++) {
      feature.push_back((i < desc.rows ? CalcFeatureOfRow(desc, i) : 0));
    }
    features->push_back(feature);
  }
}

float FeatureExtractor::CalcFeatureOfRow(const cv::Mat& image, int n) {
  float result = 0;
  for (int i = 0; i < image.cols; i++) {
    int grey = image.ptr<uchar>(n)[i];
    result += grey > 127 ? static_cast<float>(grey) / 255 : -static_cast<float>(grey) / 255;
  }
  return result;
}

int FeatureExtractor::RunBatch(const uint32_t& inputs_size, std::vector<std::vector<float>>* outputs) {
  infer_.Run(input_mlu_ptr_, output_mlu_ptr_);
  mem_op_.MemcpyOutputD2H(output_cpu_ptr_, output_mlu_ptr_);

  // parse outputs
  uint64_t data_count = model_loader_->OutputShapes()[0].hwc();
  for (size_t i = 0; i < inputs_size; ++i) {
    float *cpu_output = static_cast<float*>(output_cpu_ptr_[0]) + i * data_count;
    std::vector<float> output(cpu_output, cpu_output + data_count);
    outputs->push_back(output);
  }
  return 0;
}

}  // namespace cnstream
