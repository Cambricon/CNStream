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

#include <cnrt.h>
#include <libyuv.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_logging.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

namespace color_cvt {
static
cv::Mat BGRToBGR(const CNDataFrame& frame) {
  const cv::Mat bgr(frame.height, frame.stride[0], CV_8UC3, const_cast<void*>(frame.data[0]->GetCpuData()));
  return bgr(cv::Rect(0, 0, frame.width, frame.height)).clone();
}

static
cv::Mat RGBToBGR(const CNDataFrame& frame) {
  const cv::Mat rgb(frame.height, frame.stride[0], CV_8UC3, const_cast<void*>(frame.data[0]->GetCpuData()));
  cv::Mat bgr;
  cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
  return bgr(cv::Rect(0, 0, frame.width, frame.height)).clone();
}

static
cv::Mat YUV420SPToBGR(const CNDataFrame& frame, bool nv21) {
  const uint8_t* y_plane = reinterpret_cast<const uint8_t*>(frame.data[0]->GetCpuData());
  const uint8_t* uv_plane = reinterpret_cast<const uint8_t*>(frame.data[1]->GetCpuData());
  int width = frame.width;
  int height = frame.height;
  if (width <= 0 || height <= 1) {
    LOGF(FRAME) << "Invalid width or height, width = " << width << ", height = " << height;
  }
  height = height & (~static_cast<int>(1));

  int y_stride = frame.stride[0];
  int uv_stride = frame.stride[1];
  cv::Mat bgr(height, width, CV_8UC3);
  uint8_t* dst_bgr24 = bgr.data;
  int dst_stride = width * 3;
  // kYvuH709Constants make it to BGR
  if (nv21)
    libyuv::NV21ToRGB24Matrix(y_plane, y_stride, uv_plane, uv_stride,
                              dst_bgr24, dst_stride, &libyuv::kYvuH709Constants, width, height);
  else
    libyuv::NV12ToRGB24Matrix(y_plane, y_stride, uv_plane, uv_stride,
                              dst_bgr24, dst_stride, &libyuv::kYvuH709Constants, width, height);
  return bgr;
}

static inline
cv::Mat NV12ToBGR(const CNDataFrame& frame) {
  return YUV420SPToBGR(frame, false);
}

static inline
cv::Mat NV21ToBGR(const CNDataFrame& frame) {
  return YUV420SPToBGR(frame, true);
}

static inline
cv::Mat FrameToImageBGR(const CNDataFrame& frame) {
  switch (frame.fmt) {
    case CNDataFormat::CN_PIXEL_FORMAT_BGR24:
      return BGRToBGR(frame);
    case CNDataFormat::CN_PIXEL_FORMAT_RGB24:
      return RGBToBGR(frame);
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
      return NV12ToBGR(frame);
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
      return NV21ToBGR(frame);
    default:
      LOGF(FRAME) << "Unsupported pixel format. fmt[" << static_cast<int>(frame.fmt) << "]";
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

size_t CNDataFrame::GetPlaneBytes(int plane_idx) const {
  if (plane_idx < 0 || plane_idx >= GetPlanes()) return 0;
  switch (fmt) {
    case CNDataFormat::CN_PIXEL_FORMAT_BGR24:
    case CNDataFormat::CN_PIXEL_FORMAT_RGB24:
      return height * stride[0] * 3;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
      if (0 == plane_idx)
        return height * stride[0];
      else if (1 == plane_idx)
        return std::ceil(1.0 * height * stride[1] / 2);
      else
        LOGF(FRAME) << "plane index wrong.";
    default:
      return 0;
  }
  return 0;
}

size_t CNDataFrame::GetBytes() const {
  size_t bytes = 0;
  for (int i = 0; i < GetPlanes(); ++i) {
    bytes += GetPlaneBytes(i);
  }
  return bytes;
}

void CNDataFrame::CopyToSyncMem(void** ptr_src, bool dst_mlu) {
  if (this->deAllocator_ != nullptr) {
    /*cndecoder buffer will be used to avoid dev2dev copy*/
    if (dst_mlu) {
      for (int i = 0; i < GetPlanes(); i++) {
        size_t plane_size = GetPlaneBytes(i);
        this->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size, ctx.dev_id, ctx.ddr_channel));
        this->data[i]->SetMluData(ptr_src[i]);
      }
      return;
    }
  }

  /*deep copy*/
  if (this->ctx.dev_type == DevContext::DevType::MLU || this->ctx.dev_type == DevContext::DevType::CPU) {
    bool src_mlu = (this->ctx.dev_type == DevContext::DevType::MLU);
    size_t bytes = GetBytes();
    bytes = ROUND_UP(bytes, 64 * 1024);
    if (dst_mlu) {
      if (dst_device_id < 0 || (ctx.dev_type == DevContext::DevType::MLU && ctx.dev_id != dst_device_id)) {
        LOGF(FRAME) << "CopyToSyncMem: dst_device_id not set, or ctx.dev_id != dst_device_id"
                    << "," << dst_device_id;
        std::terminate();
        return;
      }
      mlu_data = cnMluMemAlloc(bytes, dst_device_id);
      if (nullptr == mlu_data) {
        LOGF(FRAME) << "CopyToSyncMem: failed to alloc mlu memory";
        std::terminate();
      }
    } else {
      cpu_data = cnCpuMemAlloc(bytes);
      if (nullptr == cpu_data) {
        LOGF(FRAME) << "CopyToSyncMem: failed to alloc cpu memory";
        std::terminate();
      }
    }

    if (src_mlu && dst_mlu) {
      void* dst = mlu_data.get();
      for (int i = 0; i < GetPlanes(); i++) {
        size_t plane_size = GetPlaneBytes(i);
        MluDeviceGuard guard(dst_device_id);  // dst_device_id is equal to ctx.devi_id
        cnrtRet_t ret = cnrtMemcpy(dst, ptr_src[i], plane_size, CNRT_MEM_TRANS_DIR_DEV2DEV);
        if (ret != CNRT_RET_SUCCESS) {
          LOGF(FRAME) << "CopyToSyncMem: failed to cnrtMemcpy(CNRT_MEM_TRANS_DIR_DEV2DEV)";
        }
        this->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size, dst_device_id));
        this->data[i]->SetMluData(dst);
        dst = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(dst) + plane_size);
      }
    } else if (src_mlu && !dst_mlu) {
      void* dst = cpu_data.get();
      for (int i = 0; i < GetPlanes(); i++) {
        size_t plane_size = GetPlaneBytes(i);
        MluDeviceGuard guard(ctx.dev_id);
        cnrtRet_t ret = cnrtMemcpy(dst, ptr_src[i], plane_size, CNRT_MEM_TRANS_DIR_DEV2HOST);
        if (ret != CNRT_RET_SUCCESS) {
          LOGF(FRAME) << "CopyToSyncMem: failed to cnrtMemcpy(CNRT_MEM_TRANS_DIR_DEV2HOST)";
        }
        this->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size, dst_device_id));
        this->data[i]->SetCpuData(dst);
        dst = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(dst) + plane_size);
      }
    } else if (!src_mlu && dst_mlu) {
      void* dst = mlu_data.get();
      for (int i = 0; i < GetPlanes(); i++) {
        size_t plane_size = GetPlaneBytes(i);
        MluDeviceGuard guard(dst_device_id);
        cnrtRet_t ret = cnrtMemcpy(dst, ptr_src[i], plane_size, CNRT_MEM_TRANS_DIR_HOST2DEV);
        if (ret != CNRT_RET_SUCCESS) {
          LOGF(FRAME) << "CopyToSyncMem: failed to cnrtMemcpy(CNRT_MEM_TRANS_DIR_HOST2DEV)";
        }
        this->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size, dst_device_id));
        this->data[i]->SetMluData(dst);
        dst = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(dst) + plane_size);
      }
    } else {
      void* dst = cpu_data.get();
      for (int i = 0; i < GetPlanes(); i++) {
        size_t plane_size = GetPlaneBytes(i);
        memcpy(dst, ptr_src[i], plane_size);
        this->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size, dst_device_id));
        this->data[i]->SetCpuData(dst);
        dst = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(dst) + plane_size);
      }
    }
    this->deAllocator_.reset();  // deep-copy is done, release dec-buf-ref
  } else {
    LOGF(FRAME) << "CopyToSyncMem: Unsupported type";
    std::terminate();
  }
}

void CNDataFrame::CopyToSyncMemOnDevice(int device_id) {
  // only support mlu memory sync between different devices
  if (this->ctx.dev_id != device_id && this->ctx.dev_type == DevContext::DevType::MLU) {
    unsigned int can_peer = 0;
    CALL_CNRT_BY_CONTEXT(cnrtGetPeerAccessibility(&can_peer, device_id, this->ctx.dev_id), this->ctx.dev_id,
                         this->ctx.ddr_channel);
    if (1 != can_peer) {
      LOGF(FRAME) << "dst device: " << device_id << " is not peerable to src device: " << this->ctx.dev_id;
    }

    // malloc memory on device_id
    std::shared_ptr<void> peerdev_data = nullptr;
    size_t bytes = GetBytes();
    bytes = ROUND_UP(bytes, 64 * 1024);
    peerdev_data = cnMluMemAlloc(bytes, device_id);
    if (nullptr == peerdev_data) {
      LOGF(FRAME) << "CopyToSyncMemOnDevice: failed to alloc mlu memory";
    }

    // copy data to mlu memory on device_id
    if (deAllocator_ != nullptr) {
      mlu_data = peerdev_data;
      void* dst = mlu_data.get();
      for (int i = 0; i < GetPlanes(); i++) {
        size_t plane_size = GetPlaneBytes(i);
        CNS_CNRT_CHECK(cnrtMemcpy(dst, data[i]->GetMutableMluData(), plane_size, CNRT_MEM_TRANS_DIR_PEER2PEER));
        this->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size, device_id, ctx.ddr_channel));
        this->data[i]->SetMluData(dst);
        dst = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(dst) + plane_size);
      }
    } else if (nullptr != mlu_data) {
      CNS_CNRT_CHECK(cnrtMemcpy(peerdev_data.get(), mlu_data.get(), bytes, CNRT_MEM_TRANS_DIR_PEER2PEER));
      mlu_data = peerdev_data;
      void* dst = mlu_data.get();
      for (int i = 0; i < GetPlanes(); i++) {
        size_t plane_size = GetPlaneBytes(i);
        this->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size, device_id, ctx.ddr_channel));
        this->data[i]->SetMluData(dst);
        dst = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(dst) + plane_size);
      }
    } else {
      LOGF(FRAME) << "invalid mlu data.";
    }
  } else {
    LOGF(FRAME) << "only support mlu memory sync between different devices.";
  }

  // reset ctx.dev_id to device_id (for SOURCE data)
  this->ctx.dev_id = device_id;
  // reset dst_device_id to device id (for CNSyncedMemory data)
  dst_device_id = device_id;
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
