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

#include "feature_extractor.hpp"

#include <opencv2/features2d/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#if (CV_MAJOR_VERSION >= 3)
#include <opencv2/imgcodecs/imgcodecs.hpp>
#endif

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnis/processor.h"
#include "cnis/contrib/video_helper.h"

namespace cnstream {

const int kFeatureSizeForCpu = 512;

class FeatureObserver : public infer_server::Observer {
 public:
  explicit FeatureObserver(std::function<void(const CNFrameInfoPtr, bool)> callback) : callback_(callback) {}
  void Response(infer_server::Status status, infer_server::PackagePtr data,
                infer_server::any user_data) noexcept override {
    callback_(infer_server::any_cast<const CNFrameInfoPtr>(user_data), status == infer_server::Status::SUCCESS);
  }

 private:
  std::function<void(const CNFrameInfoPtr, bool)> callback_;
};

FeatureExtractor::FeatureExtractor(const std::shared_ptr<infer_server::ModelInfo>& model,
                                   std::function<void(const CNFrameInfoPtr, bool)> callback, int device_id)
    : model_(model), callback_(callback) {
  if (!model_) {
    LOGI(TRACK) << "[FeatureExtractor] Model not set, using opencv to extract feature on CPU";
  } else {
    device_id_ = device_id;

    // 1.Check model I/O
    if (model_->InputNum() != 1) {
      LOGE(TRACK) << "[FeatureExtractor] model should have exactly one input";
      return;
    }
    if (model_->OutputNum() != 1) {
      LOGE(TRACK) << "[FeatureExtractor] model should have exactly one output";
      return;
    }

    server_.reset(new infer_server::InferServer(device_id));

    LOGI(TRACK) << "[FeatureExtractor] to extract feature on MLU";
  }
}

FeatureExtractor::~FeatureExtractor() {
  LOGI(TRACK) << "[FeatureExtractor] release resources";
  if (session_) server_->DestroySession(session_);
}

bool FeatureExtractor::Init(int engine_num) {
  if (!model_) {
    return true;
  }
  if (is_initialized_) {
    LOGW(TRACK) << "[FeatureExtractor] should not init twice.";
  }

  bool use_magicmind = infer_server::Predictor::Backend() == "magicmind";

  infer_server::SessionDesc desc;
  desc.engine_num = engine_num;
  desc.strategy = infer_server::BatchStrategy::DYNAMIC;
  desc.model = model_;
  desc.batch_timeout = 100;
  desc.show_perf = false;
  desc.name = "Track/FeatureExtractor";
  desc.preproc = infer_server::video::PreprocessorMLU::Create();
  desc.postproc = infer_server::Postprocessor::Create();
  if (use_magicmind) {
    desc.preproc->SetParams("dst_format", infer_server::video::PixelFmt::RGB24, "preprocess_type",
                            infer_server::video::PreprocessType::CNCV_PREPROC, "keep_aspect_ratio", false, "mean",
                            std::vector<float>({0.485, 0.456, 0.406}), "std", std::vector<float>({0.229, 0.224, 0.225}),
                            "normalize", true);
  } else {
    desc.preproc->SetParams("dst_format", infer_server::video::PixelFmt::ARGB, "preprocess_type",
                            infer_server::video::PreprocessType::RESIZE_CONVERT);
  }

  auto postproc_func = [](infer_server::InferData* data, const infer_server::ModelIO& model_output,
                          const infer_server::ModelInfo* model) {
    const float* res = reinterpret_cast<const float*>(model_output.buffers[0].Data());
    std::vector<float> feat;
    feat.insert(feat.end(), res, res + model_output.shapes[0].DataCount());
    CNInferObjectPtr obj = data->GetUserData<CNInferObjectPtr>();
    obj->AddFeature("track", std::move(feat));
    return true;
  };
  desc.postproc->SetParams("process_function", infer_server::Postprocessor::ProcessFunction(postproc_func));

  session_ = server_->CreateSession(desc, std::make_shared<FeatureObserver>(callback_));

  if (session_) {
    is_initialized_ = true;
    return true;
  } else {
    LOGE(TRACK) << "[FeatureExtractor] Init failed, create infer session failed.";
    return false;
  }
  return true;
}

void FeatureExtractor::WaitTaskDone(const std::string& stream_id) {
  if (model_) {
    server_->WaitTaskDone(session_, stream_id);
  }
}

bool FeatureExtractor::ExtractFeature(const CNFrameInfoPtr& info) {
  if (!model_) {
    return ExtractFeatureOnCpu(info);
  } else {
    return ExtractFeatureOnMlu(info);
  }
}

bool FeatureExtractor::ExtractFeatureOnMlu(const CNFrameInfoPtr& info) {
  if (!is_initialized_) {
    LOGW(TRACK) << "[FeatureExtractor] Please Init first.";
    return false;
  }
  std::vector<std::shared_ptr<CNInferObject>> objs;
  if (info->collection.HasValue(kCNInferObjsTag)) {
    objs = info->collection.Get<CNInferObjsPtr>(kCNInferObjsTag)->objs_;
  }
  infer_server::video::VideoFrame vframe;
  if (objs.size()) {
    const CNDataFramePtr& frame = info->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
    if (frame->fmt != CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12 &&
        frame->fmt != CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21) {
      LOGE(TRACK) << "Frame format only support NV12 / NV21.";
      return false;
    }
    vframe.width = frame->width;
    vframe.height = frame->height;
    vframe.stride[0] = frame->stride[0];
    vframe.stride[1] = frame->stride[1];
    vframe.plane[0] =
        infer_server::Buffer(const_cast<void*>(frame->data[0]->GetMluData()), frame->data[0]->GetSize(), nullptr);
    vframe.plane[1] =
        infer_server::Buffer(const_cast<void*>(frame->data[1]->GetMluData()), frame->data[1]->GetSize(), nullptr);
    switch (frame->fmt) {
      case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
        vframe.format = infer_server::video::PixelFmt::NV12;
        break;
      case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
        vframe.format = infer_server::video::PixelFmt::NV21;
        break;
      default:
        LOGE(TRACK) << "Unsupported pixel format";
        return false;
    }
  }
  auto pack = infer_server::Package::Create(objs.size(), info->stream_id);
  for (unsigned idx = 0; idx < objs.size(); ++idx) {
    auto& obj = objs[idx];
    infer_server::video::VideoFrame tmp = vframe;
    tmp.roi.x = obj->bbox.x;
    tmp.roi.y = obj->bbox.y;
    tmp.roi.w = obj->bbox.w;
    tmp.roi.h = obj->bbox.h;
    pack->data[idx]->Set(std::move(tmp));
    pack->data[idx]->SetUserData(obj);
  }

  if (!server_->Request(session_, std::move(pack), info)) {
    LOGW(TRACK) << "[FeatureExtractor] Extract feature failed";
    return false;
  }
  return true;
}

bool FeatureExtractor::ExtractFeatureOnCpu(const CNFrameInfoPtr& info) {
  const CNDataFramePtr& frame = info->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
  std::vector<std::shared_ptr<CNInferObject>> objs;
  if (info->collection.HasValue(kCNInferObjsTag)) {
    objs = info->collection.Get<CNInferObjsPtr>(kCNInferObjsTag)->objs_;
  }
  const cv::Mat image = frame->ImageBGR();
  for (uint32_t num = 0; num < objs.size(); ++num) {
    auto& obj = objs[num];
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
    feature.reserve(kFeatureSizeForCpu);
    for (int i = 0; i < kFeatureSizeForCpu; i++) {
      feature.push_back((i < desc.rows ? CalcFeatureOfRow(desc, i) : 0));
    }
    obj->AddFeature("track", std::move(feature));
  }
  callback_(info, true);
  return true;
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
