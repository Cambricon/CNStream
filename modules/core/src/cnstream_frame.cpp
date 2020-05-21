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

#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <glog/logging.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
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

  if (nullptr != mapper_) {
    mapper_.reset();
  }

  if (nullptr != deAllocator_) {
    deAllocator_.reset();
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
  uint8_t* img_data = new (std::nothrow) uint8_t[GetBytes()];
  LOG_IF(FATAL, nullptr == img_data) << "CNDataFrame::ImageBGR() failed to alloc memory";
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
  bgr_mat = new (std::nothrow) cv::Mat();
  LOG_IF(FATAL, nullptr == bgr_mat) << "CNDataFrame::ImageBGR() failed to alloc cv::Mat";
  *bgr_mat = bgr;
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
#ifdef CNS_MLU220_SOC
    if (this->ctx.dev_type == DevContext::MLU_CPU) {
      for (int i = 0; i < GetPlanes(); i++) {
        size_t plane_size = GetPlaneBytes(i);
        this->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size, ctx.dev_id, ctx.ddr_channel));
        this->data[i]->SetMluCpuData(this->ptr_mlu[i], this->ptr_cpu[i]);
      }
    } else {
      LOG(FATAL) << " unsupported dev_type";
    }
#else
    /*cndecoder buffer will be used to avoid dev2dev copy*/
    for (int i = 0; i < GetPlanes(); i++) {
      size_t plane_size = GetPlaneBytes(i);
      this->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size, ctx.dev_id, ctx.ddr_channel));
      this->data[i]->SetMluData(this->ptr_mlu[i]);
    }
#endif
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
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(dst, ptr_mlu[i], plane_size, CNRT_MEM_TRANS_DIR_DEV2DEV), ctx.dev_id,
                           ctx.ddr_channel);
      this->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size, ctx.dev_id, ctx.ddr_channel));
      this->data[i]->SetMluData(dst);
      dst = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(dst) + plane_size);
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
      memcpy(dst, ptr_cpu[i], plane_size);
      this->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size));
      this->data[i]->SetCpuData(dst);
      dst = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(dst) + plane_size);
    }
#ifdef CNS_MLU220_SOC
  } else if (this->ctx.dev_type == DevContext::MLU_CPU) {
    LOG(FATAL) << "MLU220_SOC: MLU_CPU deepCopy not supported yet";
#endif
  } else {
    LOG(FATAL) << "Device type not supported";
  }
}

void CNDataFrame::MmapSharedMem(MemMapType type) {
  if (!GetBytes()) {
    LOG(ERROR) << "GetByte() is 0.";
    return;
  }
  if (map_mem_ptr) {
    LOG(FATAL) << "MmapSharedMem should be called once for each frame";
  }

  if (type == MemMapType::MEMMAP_CPU) {
    // open shared memory
    size_t map_mem_size = ROUND_UP(GetBytes(), 64 * 1024);
    const std::string key = "stream_id_" + stream_id + "_frame_id_" + std::to_string(frame_id);
    map_mem_fd = shm_open(key.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
    if (map_mem_fd < 0) {
      LOG(FATAL) << "Shered memory open failed, fd: " << map_mem_fd << ", error code: " << errno;
    }
    map_mem_ptr = mmap(NULL, map_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, map_mem_fd, 0);
    if (map_mem_ptr == MAP_FAILED) {
      LOG(FATAL) << "Mmap error";
    }

    if (ftruncate(map_mem_fd, map_mem_size) == -1) {
      LOG(FATAL) << "truncate shared memory size failed";
    }
    // sync shared memory
    if (this->ctx.dev_type == DevContext::CPU) {
      // open shared memory, and set to frame syncdata
      auto ptmp = reinterpret_cast<uint8_t*>(map_mem_ptr);
      for (int i = 0; i < GetPlanes(); i++) {
        size_t plane_size = GetPlaneBytes(i);
        this->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size));
        this->data[i]->SetCpuData(ptmp);
        ptmp += plane_size;
      }
    } else if (this->ctx.dev_type == DevContext::MLU) {
      size_t bytes = GetBytes();
      bytes = ROUND_UP(bytes, 64 * 1024);
      CALL_CNRT_BY_CONTEXT(cnrtMalloc(&mlu_data, bytes), ctx.dev_id, ctx.ddr_channel);
      auto dst = reinterpret_cast<uint8_t*>(mlu_data);
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(dst, map_mem_ptr, bytes, CNRT_MEM_TRANS_DIR_HOST2DEV), ctx.dev_id,
                           ctx.ddr_channel);

      for (int i = 0; i < GetPlanes(); i++) {
        size_t plane_size = GetPlaneBytes(i);
        // open shared mem
        CNSyncedMemory* sync_ptr = new (std::nothrow) CNSyncedMemory(plane_size, ctx.dev_id, ctx.ddr_channel);
        this->data[i].reset(sync_ptr);
        this->data[i]->SetMluData(dst);
        dst += plane_size;
      }
    } else {
      LOG(FATAL) << "Device type not supported";
    }
  } else if (type == MemMapType::MEMMAP_MLU) {
    // get shared mlu memory from mlu memory handle
    CALL_CNRT_BY_CONTEXT(cnrtMapMemHandle(&map_mem_ptr, mlu_mem_handle, 0), ctx.dev_id, ctx.ddr_channel);
    if (this->ctx.dev_type == DevContext::CPU) {
      size_t bytes = GetBytes();
      bytes = ROUND_UP(bytes, 64 * 1024);
      CNStreamMallocHost(&cpu_data, bytes);
      if (nullptr == cpu_data) {
        LOG(FATAL) << "MmapSharedMem: failed to alloc cpu memory";
      }
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(cpu_data, map_mem_ptr, bytes, CNRT_MEM_TRANS_DIR_DEV2HOST), ctx.dev_id,
                           ctx.ddr_channel);

      void* dst = cpu_data;
      for (int i = 0; i < GetPlanes(); i++) {
        size_t plane_size = GetPlaneBytes(i);
        this->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size));
        this->data[i]->SetCpuData(dst);
        dst = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(dst) + plane_size);
      }
    } else if (this->ctx.dev_type == DevContext::MLU) {
      void* dst = map_mem_ptr;
      for (int i = 0; i < GetPlanes(); i++) {
        size_t plane_size = GetPlaneBytes(i);
        this->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size, ctx.dev_id, ctx.ddr_channel));
        this->data[i]->SetMluData(dst);
        dst = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(dst) + plane_size);
      }
    } else {
      LOG(FATAL) << "Device type not supported";
    }
  } else {
    LOG(FATAL) << "Mem map type not supported";
  }
}

void CNDataFrame::UnMapSharedMem(MemMapType type) {
  if (!GetBytes()) {
    LOG(ERROR) << "GetByte() is 0.";
    return;
  }
  if (!map_mem_ptr) return;

  if (type == MemMapType::MEMMAP_CPU) {
    size_t map_mem_size = ROUND_UP(GetBytes(), 64 * 1024);
    munmap(map_mem_ptr, map_mem_size);
    close(map_mem_fd);
  } else if (type == MemMapType::MEMMAP_MLU) {
    CALL_CNRT_BY_CONTEXT(cnrtUnMapMemHandle(map_mem_ptr), ctx.dev_id, ctx.ddr_channel);
  } else {
    LOG(FATAL) << "Mem map type not supported";
  }
}

void CNDataFrame::CopyToSharedMem(MemMapType type) {
  if (!GetBytes()) {
    LOG(ERROR) << "GetByte() is 0.";
    return;
  }

  if (shared_mem_ptr) {
    LOG(FATAL) << "CopyToSharedMem should be called once for each frame";
  }

  if (type == MemMapType::MEMMAP_CPU) {
    // create shared memory
    size_t shared_mem_size = ROUND_UP(GetBytes(), 64 * 1024);
    const std::string key = "stream_id_" + stream_id + "_frame_id_" + std::to_string(frame_id);
    // O_EXCL ensure open one time
    shared_mem_fd = shm_open(key.c_str(), O_CREAT | O_TRUNC | O_RDWR /*| O_EXCL*/, S_IRUSR | S_IWUSR);
    if (shared_mem_fd < 0) {
      LOG(FATAL) << "Shared memory create failed, fd: " << shared_mem_fd << ", error code: " << errno;
    }
    if (ftruncate(shared_mem_fd, shared_mem_size) == -1) {
      LOG(FATAL) << "truncate shared size memory failed";
    }
    shared_mem_ptr = mmap(NULL, shared_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, shared_mem_fd, 0);
    if (shared_mem_ptr == MAP_FAILED) {
      LOG(FATAL) << "Mmap error";
    }
    // copy frame data to cpu shared memory
    auto ptmp = reinterpret_cast<uint8_t*>(shared_mem_ptr);
    for (int i = 0; i < GetPlanes(); i++) {
      size_t plane_size = GetPlaneBytes(i);
      memcpy(ptmp, data[i]->GetCpuData(), plane_size);
      ptmp += plane_size;
    }
  } else if (type == MemMapType::MEMMAP_MLU) {
    // acquire cnrt memory handle to share
    if (nullptr != deAllocator_) {
      size_t bytes = GetBytes();
      bytes = ROUND_UP(bytes, 64 * 1024);
      CALL_CNRT_BY_CONTEXT(cnrtMalloc(&shared_mem_ptr, bytes), ctx.dev_id, ctx.ddr_channel);
      void* dst = shared_mem_ptr;
      for (int i = 0; i < GetPlanes(); i++) {
        size_t plane_size = GetPlaneBytes(i);
        CALL_CNRT_BY_CONTEXT(cnrtMemcpy(dst, data[i]->GetMutableMluData(), plane_size, CNRT_MEM_TRANS_DIR_DEV2DEV),
                             ctx.dev_id, ctx.ddr_channel);
        dst = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(dst) + plane_size);
      }
    } else {
      shared_mem_ptr = mlu_data;
    }

    CALL_CNRT_BY_CONTEXT(cnrtAcquireMemHandle(&mlu_mem_handle, shared_mem_ptr), ctx.dev_id, ctx.ddr_channel);
  } else {
    LOG(FATAL) << "Mem map type not supported";
  }
  return;
}

void CNDataFrame::ReleaseSharedMem(MemMapType type) {
  if (!shared_mem_ptr) return;
  if (type == MemMapType::MEMMAP_CPU) {
    const std::string key = "stream_id_" + stream_id + "_frame_id_" + std::to_string(frame_id);
    size_t shared_mem_size = ROUND_UP(GetBytes(), 64 * 1024);
    munmap(shared_mem_ptr, shared_mem_size);
    close(shared_mem_fd);
    shm_unlink(key.c_str());
  } else if (type == MemMapType::MEMMAP_MLU) {
    if (nullptr != deAllocator_) {
      CALL_CNRT_BY_CONTEXT(cnrtFree(shared_mem_ptr), ctx.dev_id, ctx.ddr_channel);
    }
  } else {
    LOG(FATAL) << "Mem map type not supported";
  }

  return;
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

bool CNInferObject::AddExtraAttribute(const std::vector<std::pair<std::string, std::string>>& attributes) {
  std::lock_guard<std::mutex> lk(attribute_mutex_);
  bool ret = true;

  for (auto& attribute : attributes) {
    ret &= AddExtraAttribute(attribute.first, attribute.second);
  }
  return ret;
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

CNSpinLock CNFrameInfo::spinlock_;
std::map<std::string, int> CNFrameInfo::stream_count_map_;
int CNFrameInfo::parallelism_ = 0;

void SetParallelism(int parallelism) { CNFrameInfo::parallelism_ = parallelism; }
int GetParallelism() { return CNFrameInfo::parallelism_; }

std::shared_ptr<CNFrameInfo> CNFrameInfo::Create(const std::string& stream_id, bool eos) {
  if (stream_id == "") {
    LOG(ERROR) << "CNFrameInfo::Create() stream_id is empty string.";
    return nullptr;
  }
  std::shared_ptr<CNFrameInfo> ptr(new (std::nothrow) CNFrameInfo());
  if (!ptr) {
    LOG(ERROR) << "CNFrameInfo::Create() new CNFrameInfo failed.";
    return nullptr;
  }
  ptr->frame.stream_id = stream_id;
  if (eos) {
    ptr->frame.flags |= cnstream::CN_FRAME_FLAG_EOS;
    return ptr;
  }

  if (parallelism_ > 0) {
    CNSpinLockGuard guard(spinlock_);
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
    CNSpinLockGuard guard(spinlock_);
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

uint64_t CNFrameInfo::SetModuleMask(Module* module, Module* current) {
  CNSpinLockGuard guard(mask_lock_);
  auto iter = module_mask_map_.find(module->GetId());
  if (iter != module_mask_map_.end()) {
    iter->second |= (uint64_t)1 << current->GetId();
  } else {
    module_mask_map_[module->GetId()] = (uint64_t)1 << current->GetId();
  }
  return module_mask_map_[module->GetId()];
}

uint64_t CNFrameInfo::GetModulesMask(Module* module) {
  CNSpinLockGuard guard(mask_lock_);
  auto iter = module_mask_map_.find(module->GetId());
  if (iter != module_mask_map_.end()) {
    return iter->second;
  }
  return 0;
}

void CNFrameInfo::ClearModuleMask(Module* module) {
  CNSpinLockGuard guard(mask_lock_);
  auto iter = module_mask_map_.find(module->GetId());
  if (iter != module_mask_map_.end()) {
    iter->second = 0;
  }
}

uint64_t CNFrameInfo::AddEOSMask(Module* module) {
  CNSpinLockGuard guard(eos_lock_);
  eos_mask |= (uint64_t)1 << module->GetId();
  return eos_mask;
}

}  // namespace cnstream
