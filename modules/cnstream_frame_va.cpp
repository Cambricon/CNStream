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
#include "cnstream_frame_va.hpp"

#include <unistd.h>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "cnrt.h"
#include "cnstream_logging.hpp"
#include "cnstream_module.hpp"
#include "libyuv.h"

namespace cnstream {

namespace color_cvt {

static cv::Mat YUV420SPToBGR(const CNDataFrame& frame, bool nv21) {
  cnedk::BufSurfWrapperPtr surf = frame.buf_surf;
  CnedkBufSurfaceSyncForCpu(surf->GetBufSurface(), -1, -1);
  const uint8_t* y_plane = static_cast<const uint8_t*>(surf->GetHostData(0));
  const uint8_t* uv_plane = static_cast<const uint8_t*>(surf->GetHostData(1));
  int width = surf->GetWidth();
  int height = surf->GetHeight();
  if (width <= 0 || height <= 1) {
    LOGF(FRAME) << "Invalid width or height, width = " << width << ", height = " << height;
  }
  height = height & (~static_cast<int>(1));

  int y_stride = surf->GetStride(0);
  int uv_stride = surf->GetStride(1);
  cv::Mat bgr(height, width, CV_8UC3);
  uint8_t* dst_bgr24 = bgr.data;
  int dst_stride = width * 3;
  // kYvuH709Constants make it to BGR
  if (nv21)
    libyuv::NV21ToRGB24Matrix(y_plane, y_stride, uv_plane, uv_stride, dst_bgr24, dst_stride, &libyuv::kYvuH709Constants,
                              width, height);
  else
    libyuv::NV12ToRGB24Matrix(y_plane, y_stride, uv_plane, uv_stride, dst_bgr24, dst_stride, &libyuv::kYvuH709Constants,
                              width, height);

  return bgr;
}

static inline cv::Mat NV12ToBGR(const CNDataFrame& frame) { return YUV420SPToBGR(frame, false); }

static inline cv::Mat NV21ToBGR(const CNDataFrame& frame) { return YUV420SPToBGR(frame, true); }

static inline cv::Mat FrameToImageBGR(const CNDataFrame& frame) {
  switch (frame.buf_surf->GetColorFormat()) {
    case CNEDK_BUF_COLOR_FORMAT_NV12:
      return NV12ToBGR(frame);
    case CNEDK_BUF_COLOR_FORMAT_NV21:
      return NV21ToBGR(frame);
    default:
      LOGF(FRAME) << "Unsupported pixel format. fmt[" << static_cast<int>(frame.buf_surf->GetColorFormat()) << "]";
  }
  // never be here
  return cv::Mat();
}

}  // namespace color_cvt

cv::Mat CNDataFrame::ImageBGR() {
  std::lock_guard<std::mutex> lk(mtx);
  if (!bgr_mat.empty()) {
    return bgr_mat;
  }
  bgr_mat = color_cvt::FrameToImageBGR(*this);
  return bgr_mat;
}

bool CNInferObject::AddAttribute(const std::string& key, const CNInferAttr& value) {
  std::lock_guard<std::mutex> lk(attribute_mutex_);
  if (attributes_.find(key) != attributes_.end()) return false;

  attributes_.insert(std::make_pair(key, value));
  return true;
}

bool CNInferObject::AddAttribute(const std::pair<std::string, CNInferAttr>& attribute) {
  std::lock_guard<std::mutex> lk(attribute_mutex_);
  if (attributes_.find(attribute.first) != attributes_.end()) return false;

  attributes_.insert(attribute);
  return true;
}

CNInferAttr CNInferObject::GetAttribute(const std::string& key) {
  std::lock_guard<std::mutex> lk(attribute_mutex_);
  if (attributes_.find(key) != attributes_.end()) return attributes_[key];

  return CNInferAttr();
}

bool CNInferObject::AddExtraAttribute(const std::string& key, const std::string& value) {
  std::lock_guard<std::mutex> lk(attribute_mutex_);
  if (extra_attributes_.find(key) != extra_attributes_.end()) return false;

  extra_attributes_.insert(std::make_pair(key, value));
  return true;
}

bool CNInferObject::AddExtraAttributes(const std::vector<std::pair<std::string, std::string>>& attributes) {
  std::lock_guard<std::mutex> lk(attribute_mutex_);
  bool ret = true;
  for (auto& attribute : attributes) {
    ret &= AddExtraAttribute(attribute.first, attribute.second);
  }
  return ret;
}

std::string CNInferObject::GetExtraAttribute(const std::string& key) {
  std::lock_guard<std::mutex> lk(attribute_mutex_);
  if (extra_attributes_.find(key) != extra_attributes_.end()) {
    return extra_attributes_[key];
  }
  return "";
}

bool CNInferObject::RemoveExtraAttribute(const std::string& key) {
  std::lock_guard<std::mutex> lk(attribute_mutex_);
  if (extra_attributes_.find(key) != extra_attributes_.end()) {
    extra_attributes_.erase(key);
  }
  return true;
}

StringPairs CNInferObject::GetExtraAttributes() {
  std::lock_guard<std::mutex> lk(attribute_mutex_);
  return StringPairs(extra_attributes_.begin(), extra_attributes_.end());
}

bool CNInferObject::AddFeature(const std::string& key, const CNInferFeature& feature) {
  std::lock_guard<std::mutex> lk(feature_mutex_);
  if (features_.find(key) != features_.end()) {
    return false;
  }
  features_.insert(std::make_pair(key, feature));
  return true;
}

CNInferFeature CNInferObject::GetFeature(const std::string& key) {
  std::lock_guard<std::mutex> lk(feature_mutex_);
  if (features_.find(key) != features_.end()) {
    return features_[key];
  }
  return CNInferFeature();
}

CNInferFeatures CNInferObject::GetFeatures() {
  std::lock_guard<std::mutex> lk(feature_mutex_);
  return CNInferFeatures(features_.begin(), features_.end());
}

}  // namespace cnstream
