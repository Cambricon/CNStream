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

#include "cnstream_frame.hpp"

#include <cnrt.h>
#include <glog/logging.h>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "cnstream_module.hpp"

#define ROUND_UP(addr, boundary) (((u32_t)(addr) + (boundary)-1) & ~((boundary)-1))

namespace cnstream {

CNDataFrame::~CNDataFrame() {
  if (nullptr != mlu_data) {
    CALL_CNRT_BY_CONTEXT(cnrtFree(mlu_data), ctx.dev_id, ctx.ddr_channel);
  }
  if (nullptr != cpu_data) {
    CNStreamFreeHost(cpu_data), cpu_data = nullptr;
  }
#ifdef HAVE_OPENCV
  if (nullptr != bgr_mat) {
    delete bgr_mat, bgr_mat = nullptr;
  }
#endif
}

#ifdef HAVE_OPENCV
cv::Mat* CNDataFrame::ImageBGR() {
  if (bgr_mat != nullptr) {
    return bgr_mat;
  }
  int stride_ = stride[0];
  cv::Mat bgr(height, stride_, CV_8UC3);
  uint8_t* img_data = new uint8_t[GetBytes()];
  uint8_t* t = img_data;
  for (int i = 0; i < GetPlanes(); ++i) {
    memcpy(t, data[i]->GetCpuData(), GetPlaneBytes(i));
    t += GetPlaneBytes(i);
  }
  switch (fmt) {
    case CNDataFormat::CN_PIXEL_FORMAT_BGR24: {
      bgr = cv::Mat(height, stride_, CV_8UC3, img_data).clone();
    } break;
    case CNDataFormat::CN_PIXEL_FORMAT_RGB24: {
      cv::Mat src = cv::Mat(height, stride_, CV_8UC3, img_data);
      cv::cvtColor(src, bgr, cv::COLOR_RGB2BGR);
    } break;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12: {
      cv::Mat src = cv::Mat(height * 3 / 2, stride_, CV_8UC1, img_data);
      cv::cvtColor(src, bgr, cv::COLOR_YUV2BGR_NV12);
    } break;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21: {
      cv::Mat src = cv::Mat(height * 3 / 2, stride_, CV_8UC1, img_data);
      cv::cvtColor(src, bgr, cv::COLOR_YUV2BGR_NV21);
    } break;
    default: {
      LOG(WARNING) << "Unsupport pixel format.";
      delete[] img_data;
      return nullptr;
    }
  }
  delete[] img_data;
  bgr_mat = new cv::Mat();
  if (bgr_mat) {
    *bgr_mat = bgr;
  }
  return bgr_mat;
}
#endif

size_t CNDataFrame::GetPlaneBytes(int plane_idx) const {
  if (plane_idx < 0 || plane_idx >= GetPlanes()) return 0;

  switch (fmt) {
    case CN_PIXEL_FORMAT_BGR24:
    case CN_PIXEL_FORMAT_RGB24:
      return height * stride[0] * 3;
    case CN_PIXEL_FORMAT_YUV420_NV12:
    case CN_PIXEL_FORMAT_YUV420_NV21:
      if (0 == plane_idx)
        return height * stride[0];
      else if (1 == plane_idx)
        return height * stride[1] / 2;
      else
        LOG(FATAL) << "plane index wrong.";
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

void CNDataFrame::CopyToSyncMem() {
  if (this->deAllocator_ != nullptr) {
    /*cndecoder buffer will be used to avoid dev2dev copy*/
    for (int i = 0; i < GetPlanes(); i++) {
      size_t plane_size = GetPlaneBytes(i);
      this->data[i].reset(new CNSyncedMemory(plane_size, ctx.dev_id, ctx.ddr_channel));
      this->data[i]->SetMluData(this->ptr[i]);
    }
    return;
  }
  /*deep copy*/
  if (this->ctx.dev_type == DevContext::MLU) {
    if (mlu_data != nullptr) {
      LOG(FATAL) << "CopyToSyncMem should be called once for each frame";
    }
    size_t bytes = GetBytes();
    bytes = ROUND_UP(bytes, 64 * 1024);
    CALL_CNRT_BY_CONTEXT(cnrtMalloc(&mlu_data, bytes), ctx.dev_id, ctx.ddr_channel);
    void* dst = mlu_data;
    for (int i = 0; i < GetPlanes(); i++) {
      size_t plane_size = GetPlaneBytes(i);
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(dst, ptr[i], plane_size, CNRT_MEM_TRANS_DIR_DEV2DEV), ctx.dev_id,
                           ctx.ddr_channel);
      this->data[i].reset(new CNSyncedMemory(plane_size, ctx.dev_id, ctx.ddr_channel));
      this->data[i]->SetMluData(dst);
      dst = (void*)((uint8_t*)dst + plane_size);
    }
  } else if (this->ctx.dev_type == DevContext::CPU) {
    if (cpu_data != nullptr) {
      LOG(FATAL) << "CopyToSyncMem should be called once for each frame";
    }
    size_t bytes = GetBytes();
    bytes = ROUND_UP(bytes, 64 * 1024);
    CNStreamMallocHost(&cpu_data, bytes);
    if (nullptr == cpu_data) {
      LOG(FATAL) << "CopyToSyncMem: failed to alloc cpu memory";
    }
    void* dst = cpu_data;
    for (int i = 0; i < GetPlanes(); i++) {
      size_t plane_size = GetPlaneBytes(i);
      memcpy(dst, ptr[i], plane_size);
      this->data[i].reset(new CNSyncedMemory(plane_size));
      this->data[i]->SetCpuData(dst);
      dst = (void*)((uint8_t*)dst + plane_size);
    }
  } else {
    LOG(FATAL) << "Device type not supported";
  }
}

void CNDataFrame::SetModuleMask(Module* module, Module* current) {
  std::unique_lock<std::mutex> lock(modules_mutex);
  auto iter = module_mask_map_.find(module->GetId());
  if (iter != module_mask_map_.end()) {
    iter->second |= (unsigned long)1 << current->GetId();
  } else {
    module_mask_map_[module->GetId()] = (unsigned long)1 << current->GetId();
  }
}

unsigned long CNDataFrame::GetModulesMask(Module* module) {
  std::unique_lock<std::mutex> lock(modules_mutex);
  auto iter = module_mask_map_.find(module->GetId());
  if (iter != module_mask_map_.end()) {
    return iter->second;
  }
  return 0;
}

void CNDataFrame::ClearModuleMask(Module* module) {
  std::unique_lock<std::mutex> lock(modules_mutex);
  auto iter = module_mask_map_.find(module->GetId());
  if (iter != module_mask_map_.end()) {
    iter->second = 0;
  }
}

unsigned long CNDataFrame::AddEOSMask(Module* module) {
  std::lock_guard<std::mutex> lock(eos_mutex);
  eos_mask |= (unsigned long)1 << module->GetId();
  return eos_mask;
}

bool CNInferObject::AddAttribute(const std::string& key, const CNInferAttr& value) {
  std::lock_guard<std::mutex> lk(attribute_mutex_);

  if (attributes_.find(key) != attributes_.end()) return false;

  attributes_.insert(std::make_pair(key, value));
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

std::string CNInferObject::GetExtraAttribute(const std::string& key) {
  std::lock_guard<std::mutex> lk(attribute_mutex_);

  if (extra_attributes_.find(key) != extra_attributes_.end()) return extra_attributes_[key];

  return "";
}

void CNInferObject::AddFeature(const CNInferFeature& feature) {
  std::lock_guard<std::mutex> lk(feature_mutex_);
  features_.push_back(feature);
}

std::vector<CNInferFeature> CNInferObject::GetFeatures() {
  std::lock_guard<std::mutex> lk(feature_mutex_);
  return features_;
}

std::mutex CNFrameInfo::mutex_;
std::map<std::string, int> CNFrameInfo::stream_count_map_;
int CNFrameInfo::parallelism_ = 0;

void SetParallelism(int parallelism) { CNFrameInfo::parallelism_ = parallelism; }

std::shared_ptr<CNFrameInfo> CNFrameInfo::Create(const std::string& stream_id, bool eos) {
  CNFrameInfo* frameInfo = new CNFrameInfo();
  if (!frameInfo) {
    return nullptr;
  }
  frameInfo->frame.stream_id = stream_id;
  std::shared_ptr<CNFrameInfo> ptr(frameInfo);
  if (eos) {
    ptr->frame.flags |= cnstream::CN_FRAME_FLAG_EOS;
    return ptr;
  }

  if (parallelism_ > 0) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto iter = stream_count_map_.find(stream_id);
    if (iter == stream_count_map_.end()) {
      int count = 1;
      stream_count_map_[stream_id] = count;
      // LOG(INFO) << "CNFrameInfo::Create() insert stream_id: " << stream_id;
    } else {
      int count = stream_count_map_[stream_id];
      if (count >= parallelism_) {
        return nullptr;
      }
      stream_count_map_[stream_id] = count + 1;
      // LOG(INFO) << "CNFrameInfo::Create() add count stream_id " << stream_id << ":" << count;
    }
  }
  return ptr;
}

CNFrameInfo::~CNFrameInfo() {
  if (frame.flags & CN_FRAME_FLAG_EOS) {
    return;
  }
  if (frame.ctx.dev_type == DevContext::INVALID) {
    return;
  }
  if (parallelism_ > 0) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto iter = stream_count_map_.find(frame.stream_id);
    if (iter != stream_count_map_.end()) {
      int count = iter->second;
      --count;
      if (count <= 0) {
        stream_count_map_.erase(iter);
        // LOG(INFO) << "CNFrameInfo::~CNFrameInfo() erase stream_id " << frame.stream_id;
      } else {
        iter->second = count;
        // LOG(INFO) << "CNFrameInfo::~CNFrameInfo() update stream_id " << frame.stream_id << " : " << count;
      }
    } else {
      LOG(ERROR) << "Invaid stream_id, please check\n";
    }
  }
}

}  // namespace cnstream
