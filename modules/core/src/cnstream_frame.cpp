/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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
#include <cassert>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "cnstream_module.hpp"

#define ROUND_UP(addr, boundary) (((u32_t)(addr) + (boundary)-1) & ~((boundary)-1))

namespace cnstream {

CNDataFrame::CNDataFrame() {}

CNDataFrame::~CNDataFrame() {
  if (nullptr != mlu_data) {
    CALL_CNRT_BY_CONTEXT(cnrtFree(mlu_data), ctx.dev_id, ctx.ddr_channel);
  }
}

size_t CNDataFrame::GetPlaneBytes(int plane_idx) const {
  if (plane_idx < 0 || plane_idx >= GetPlanes()) return 0;

  switch (fmt) {
    case CN_PIXEL_FORMAT_BGR24:
    case CN_PIXEL_FORMAT_RGB24:
      return height * strides[0] * 3;
    case CN_PIXEL_FORMAT_YUV420_NV12:
    case CN_PIXEL_FORMAT_YUV420_NV21:
      if (0 == plane_idx)
        return height * strides[0];
      else if (1 == plane_idx)
        return height * strides[1] / 2;
      else
        assert(false);
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

void CNDataFrame::CopyFrameFromMLU(int dev_id, int ddr_channel, CNDataFormat fmt, int width, int height, void** data,
                                   const uint32_t* strides) {
  if (DevContext::MLU == ctx.dev_type) {
    // memory already on MLU
    LOG(FATAL) << "Unsupport";
    assert(false);
  } else if (DevContext::CPU == ctx.dev_type) {
    // memory on CPU. free it?
    LOG(FATAL) << "Unsupport";
    assert(false);
  }

  void* ptr = nullptr;
  size_t bytes = 0;
  switch (fmt) {
    case CN_PIXEL_FORMAT_BGR24:
    case CN_PIXEL_FORMAT_RGB24:
      bytes = height * strides[0] * 3;
      break;
    case CN_PIXEL_FORMAT_YUV420_NV12:
    case CN_PIXEL_FORMAT_YUV420_NV21:
      bytes = height * strides[0] + height / 2 * strides[1];
      break;
    default:
      LOG(ERROR) << "[CopyFrameFromMLU]: Unknown pixel format: " << static_cast<int>(fmt);
  }

  bytes = ROUND_UP(bytes, 64 * 1024);

  CALL_CNRT_BY_CONTEXT(cnrtMalloc(&ptr, bytes), ctx.dev_id, ctx.ddr_channel);

  const int planes = CNGetPlanes(fmt);
  void* dst = ptr;
  for (int i = 0; i < planes; ++i) {
    size_t plane_size = 0;
    switch (fmt) {
      case CN_PIXEL_FORMAT_BGR24:
      case CN_PIXEL_FORMAT_RGB24:
        plane_size = height * strides[0] * 3;
        break;
      case CN_PIXEL_FORMAT_YUV420_NV12:
      case CN_PIXEL_FORMAT_YUV420_NV21:
        if (0 == i)
          plane_size = height * strides[0];
        else if (1 == i)
          plane_size = height * strides[1] / 2;
        else
          assert(false);
        break;
      default:
        LOG(ERROR) << "[CopyFrameFromMLU]: Unknown pixel format: " << static_cast<int>(fmt);
        return;
    }
    CALL_CNRT_BY_CONTEXT(cnrtMemcpy(dst, data[i], plane_size, CNRT_MEM_TRANS_DIR_DEV2DEV), ctx.dev_id, ctx.ddr_channel);
    dst = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(ptr) + plane_size);
  }

  {
    this->width = width;
    this->height = height;
    this->mlu_data = ptr;
    for (int i = 0; i < planes; ++i) {
      this->strides[i] = strides[i];
    }
    this->fmt = fmt;
    this->ctx.dev_id = dev_id;
    this->ctx.dev_type = DevContext::MLU;
    this->ctx.ddr_channel = ddr_channel;

    auto t = reinterpret_cast<uint8_t*>(ptr);
    for (int i = 0; i < planes; ++i) {
      size_t plane_size = GetPlaneBytes(i);
      this->data[i].reset(new CNSyncedMemory(plane_size, ctx.dev_id, ctx.ddr_channel));
      this->data[i]->SetMluDevContext(dev_id, ddr_channel);
      this->data[i]->SetMluData(t);
      t += plane_size;
    }
  }
}

void CNDataFrame::ReallocMemory(int width, int height) {
  if (DevContext::CPU == ctx.dev_type) {
    this->width = width;
    this->height = height;
    switch (fmt) {
      case CNDataFormat::CN_PIXEL_FORMAT_BGR24:
      case CNDataFormat::CN_PIXEL_FORMAT_RGB24:
        this->strides[0] = width;
        break;
      case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
      case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
        this->strides[0] = this->strides[1] = width;
      default:
        return;
    }

    for (int i = 0; i < GetPlanes(); ++i) {
      this->data[i].reset(new CNSyncedMemory(GetPlaneBytes(i), ctx.dev_id, ctx.ddr_channel));
    }
  }
}

void CNDataFrame::ReallocMemory(CNDataFormat format) {
  if (fmt == format) return;

  size_t old_bytes = GetBytes();
  size_t new_bytes = 0;
  switch (format) {
    case CNDataFormat::CN_PIXEL_FORMAT_BGR24:
    case CNDataFormat::CN_PIXEL_FORMAT_RGB24:
      new_bytes = width * height * 3;
      break;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
      new_bytes = width * height * 3 / 2;
      break;
    default:
      return;
  }

  if (new_bytes == old_bytes) return;

  fmt = format;

  for (int i = 0; i < GetPlanes(); ++i) strides[i] = width;

  for (int i = 0; i < GetPlanes(); ++i) {
    size_t plane_size = GetPlaneBytes(i);
    data[i].reset(new CNSyncedMemory(plane_size, ctx.dev_id, ctx.ddr_channel));
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

}  // namespace cnstream
