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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "opencv2/features2d/features2d.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif


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
  if (session_) {
    infer_server::RemovePreprocHandler(model_->GetKey());
    infer_server::RemovePostprocHandler(model_->GetKey());
    server_->DestroySession(session_);
  }
}

bool FeatureExtractor::Init(InferVideoPixelFmt model_input_format,
                            int engine_num, uint32_t batch_timeout, int priority) {
  if (!model_) {
    return true;
  }
  if (is_initialized_) {
    LOGW(TRACK) << "[FeatureExtractor] should not init twice.";
  }

  infer_server::SessionDesc desc;
  desc.engine_num = engine_num;
  desc.model = model_;
  desc.priority = priority;
  desc.strategy = infer_server::BatchStrategy::DYNAMIC;
  if (model_->BatchSize() == 1) {
    desc.strategy = infer_server::BatchStrategy::STATIC;
  }
  desc.batch_timeout = batch_timeout;
  desc.show_perf = false;
  desc.name = "Track/FeatureExtractor";
  desc.preproc = infer_server::Preprocessor::Create();
  infer_server::SetPreprocHandler(model_->GetKey(), this);
  desc.model_input_format = model_input_format;
  desc.postproc = infer_server::Postprocessor::Create();
  infer_server::SetPostprocHandler(model_->GetKey(), this);

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
  if (session_) {
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
  cnrtSetDevice(device_id_);
  if (info->collection.HasValue(kCNInferObjsTag)) {
    CNInferObjsPtr objs_holder = info->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
    std::unique_lock<std::mutex> guard(objs_holder->mutex_);

    auto pack = infer_server::Package::Create(objs_holder->objs_.size(), info->stream_id);
    for (unsigned idx = 0; idx < objs_holder->objs_.size(); ++idx) {
      auto& obj = objs_holder->objs_[idx];
      infer_server::PreprocInput tmp;
      tmp.surf = info->collection.Get<CNDataFramePtr>(kCNDataFrameTag)->buf_surf;
      tmp.has_bbox = true;
      tmp.bbox = obj->bbox;
      pack->data[idx]->Set(std::move(tmp));
      pack->data[idx]->SetUserData(obj);
    }

    guard.unlock();
    if (!server_->Request(session_, std::move(pack), info)) {
      LOGW(TRACK) << "[FeatureExtractor] Extract feature failed";
      return false;
    }
    return true;
  }
  return true;
}

bool FeatureExtractor::ExtractFeatureOnCpu(const CNFrameInfoPtr& info) {
  const CNDataFramePtr& frame = info->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
  if (info->collection.HasValue(kCNInferObjsTag)) {
    CNInferObjsPtr objs_holder = info->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
    std::unique_lock<std::mutex> guard(objs_holder->mutex_);

    const cv::Mat image = frame->ImageBGR();
    for (uint32_t num = 0; num < objs_holder->objs_.size(); ++num) {
      auto& obj = objs_holder->objs_[num];
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

    guard.unlock();
    callback_(info, true);
    return true;
  }
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

int FeatureExtractor::OnTensorParams(const infer_server::CnPreprocTensorParams* params) {
  uint32_t model_input_c;
  if (params->input_order == infer_server::DimOrder::NHWC) {
    model_input_c = params->input_shape[3];
  } else if (params->input_order == infer_server::DimOrder::NCHW) {
    model_input_c = params->input_shape[1];
  } else {
    LOGE(TRACK) << "[FeatureExtractor] Unsupported input order";
    return -1;
  }

  if (GetNetworkInfo(params, &info_) < 0) {
    LOGE(TRACK) << "[FeatureExtractor] Get network information failed";
    return -1;
  }

  if (model_input_c != 3) {
    LOGE(TRACK) << "[FeatureExtractor] input c is not 3, not suppoted yet";
    return -1;
  }
  return 0;
}

int FeatureExtractor::OnPreproc(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
                                const std::vector<CnedkTransformRect>& src_rects) {
  if (src_rects.size() != src->GetNumFilled()) return -1;

  CnedkBufSurface* src_buf = src->GetBufSurface();
  CnedkBufSurface* dst_buf = dst->GetBufSurface();

  uint32_t batch_size = src->GetNumFilled();
  std::vector<CnedkTransformRect> src_rect(batch_size);

  CnedkTransformParams params;
  memset(&params, 0, sizeof(params));
  params.transform_flag = 0;
  params.transform_flag |= CNEDK_TRANSFORM_CROP_SRC;
  params.src_rect = src_rect.data();

  for (uint32_t i = 0; i < batch_size; i++) {
    CnedkTransformRect* src_bbox = &src_rect[i];
    *src_bbox = src_rects[i];
    // validate bbox, at least 2 bytes aligned
    src_bbox->left -= src_bbox->left & 1;
    src_bbox->top -= src_bbox->top & 1;
    src_bbox->width -= src_bbox->width & 1;
    src_bbox->height -= src_bbox->height & 1;
    while (src_bbox->left + src_bbox->width > src_buf->surface_list[i].width) src_bbox->width -= 2;
    while (src_bbox->top + src_bbox->height > src_buf->surface_list[i].height) src_bbox->height -= 2;
  }
  cnrtSetDevice(device_id_);
  CnedkTransformConfigParams config;
  memset(&config, 0, sizeof(config));
  config.compute_mode = CNEDK_TRANSFORM_COMPUTE_MLU;
  CnedkTransformSetSessionParams(&config);

  if (CnedkTransform(src_buf, dst_buf, &params) < 0) {
    LOGE(TRACK) << "[FeatureExtractor] CnedkTransform failed";
    return -1;
  }
  return 0;
}

int FeatureExtractor::OnPostproc(const std::vector<infer_server::InferData*>& data_vec,
                                 const infer_server::ModelIO& model_output,
                                 const infer_server::ModelInfo* model_info) {
  cnedk::BufSurfWrapperPtr output = model_output.surfs[0];
  if (!output->GetHostData(0)) {
    LOGE(TRACK) << "[FeatureExtractor] postproc_func failed, copy data to host first.";
    return -1;
  }
  CnedkBufSurfaceSyncForCpu(output->GetBufSurface(), -1, -1);

  for (size_t batch_idx = 0; batch_idx < data_vec.size(); batch_idx++) {
    float* res = static_cast<float*>(output->GetHostData(0, batch_idx));
    std::vector<float> feat(res, res + model_output.shapes[0].DataCount());
    CNInferObjectPtr obj = data_vec[batch_idx]->GetUserData<CNInferObjectPtr>();
    obj->AddFeature("track", std::move(feat));
  }
  return 0;
}

}  // namespace cnstream
