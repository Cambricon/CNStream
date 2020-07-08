/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#include "easyplugin/resize_yuv_to_rgba.h"

#include <deque>
#include <memory>
#include <string>
#include <utility>

#include "glog/logging.h"

using std::string;
extern bool CreateResizeYuv2Rgba(const edk::MluResizeAttr& attr, ResizeYuv2Rgba** yuv2rgba_ptr, string* estr);

extern bool DestroyResizeYuv2Rgba(ResizeYuv2Rgba* yuv2rgba, string* estr);

extern bool ComputeResizeYuv2Rgba(void* dst, void* src_y, void* src_uv, ResizeYuv2Rgba* yuv2rgba, cnrtQueue_t queue,
                                  string* estr);

namespace edk {

class MluResizeYuv2RgbaPrivate {
 public:
  bool queue_is_exclusive_ = true;
  cnrtQueue_t queue_ = nullptr;
  ResizeYuv2Rgba* yuv2rgba_ = nullptr;
  void *y_ptrs_cpu_ = nullptr, *uv_ptrs_cpu_ = nullptr;
  void *y_ptrs_mlu_ = nullptr, *uv_ptrs_mlu_ = nullptr;
  std::string estr_;
  std::deque<std::pair<void*, void*>> yuv_ptrs_cache_;
  MluResizeAttr attr_;
};

MluResizeYuv2Rgba::MluResizeYuv2Rgba() { d_ptr_ = new MluResizeYuv2RgbaPrivate; }
MluResizeYuv2Rgba::MluResizeYuv2Rgba(const MluResizeAttr& attr) {
  d_ptr_ = new MluResizeYuv2RgbaPrivate;
  Init(attr);
}

MluResizeYuv2Rgba::~MluResizeYuv2Rgba() {
  if (d_ptr_) {
    Destroy();
    delete d_ptr_;
    d_ptr_ = nullptr;
  }
}

const MluResizeAttr& MluResizeYuv2Rgba::GetAttr() { return d_ptr_->attr_; }

cnrtQueue_t MluResizeYuv2Rgba::GetMluQueue() const { return d_ptr_->queue_; }

void MluResizeYuv2Rgba::SetMluQueue(cnrtQueue_t queue, bool exclusive) {
  if (d_ptr_->queue_is_exclusive_) {
    DestroyMluQueue();
  }
  d_ptr_->queue_is_exclusive_ = exclusive;
  d_ptr_->queue_ = queue;
}

void MluResizeYuv2Rgba::DestroyMluQueue() {
  if (d_ptr_->queue_ != nullptr) {
    cnrtDestroyQueue(d_ptr_->queue_);
  }
  d_ptr_->queue_ = nullptr;
}

std::string MluResizeYuv2Rgba::GetLastError() const { return d_ptr_->estr_; }

bool MluResizeYuv2Rgba::Init(const MluResizeAttr& attr) {
  d_ptr_->attr_ = attr;
  uint32_t src_stride = attr.src_w > attr.src_stride ? attr.src_w : attr.src_stride;
  uint32_t crop_x = attr.crop_x >= attr.src_w ? 0 : attr.crop_x;
  uint32_t crop_y = attr.crop_y >= attr.src_h ? 0 : attr.crop_y;
  uint32_t crop_w = attr.crop_w == 0 ? attr.src_w : attr.crop_w;
  crop_w = (crop_w + crop_x) > attr.src_w ? (attr.src_w - crop_x) : crop_w;
  uint32_t crop_h = attr.crop_h == 0 ? attr.src_h : attr.crop_h;
  crop_h = (crop_h + crop_y) > attr.src_h ? (attr.src_h - crop_y) : crop_h;
  d_ptr_->attr_.src_stride = src_stride;
  d_ptr_->attr_.crop_x = crop_x;
  d_ptr_->attr_.crop_y = crop_y;
  d_ptr_->attr_.crop_w = crop_w;
  d_ptr_->attr_.crop_h = crop_h;

  if (attr.core_version == CoreVersion::MLU270) {
    switch (attr.core_number) {
      case 0:
        d_ptr_->attr_.core_number = kMLU270CoreNum;
        break;
      case 1: case 4: case 8: case 16:
        break;
      default:
        std::cout << "[ResizeYuv2Rgba] core number: " << attr.core_number
                  << " is not supported. Choose from 1, 4, 8, 16 instead." << std::endl;
        d_ptr_->estr_ = "Wrong core number. Choose from 1, 4, 8, 16 instead.";
        return false;
    }
  } else if (attr.core_version == CoreVersion::MLU220) {
    switch (attr.core_number) {
      case 0:
        d_ptr_->attr_.core_number = kMLU220CoreNum;
        break;
      case 1: case 4:
        break;
      default:
        std::cout << "[ResizeYuv2Rgba] core number: " << attr.core_number
                  << " is not supported. Choose from 1, 4 instead." << std::endl;
        d_ptr_->estr_ = "Wrong core number. Choose from 1, 4 instead.";
        return false;
    }
  }

  d_ptr_->y_ptrs_cpu_ = malloc(sizeof(void*) * attr.batch_size);
  d_ptr_->uv_ptrs_cpu_ = malloc(sizeof(void*) * attr.batch_size);
  cnrtRet_t cnret = cnrtMalloc(&d_ptr_->y_ptrs_mlu_, sizeof(void*) * attr.batch_size);
  if (cnret != CNRT_RET_SUCCESS) {
    d_ptr_->estr_ = "Malloc mlu buffer failed. Error code:" + std::to_string(cnret);
    return false;
  }
  cnret = cnrtMalloc(&d_ptr_->uv_ptrs_mlu_, sizeof(void*) * attr.batch_size);
  if (cnret != CNRT_RET_SUCCESS) {
    d_ptr_->estr_ = "Malloc mlu buffer failed. Error code:" + std::to_string(cnret);
    return false;
  }

  VLOG(4) <<  "Init ResizeYuv2Rgba Operator";

  bool success = ::CreateResizeYuv2Rgba(d_ptr_->attr_, &d_ptr_->yuv2rgba_, &d_ptr_->estr_);
  if (!success) {
    if (d_ptr_->yuv2rgba_) {
      if (!::DestroyResizeYuv2Rgba(d_ptr_->yuv2rgba_, &d_ptr_->estr_)) {
        LOG(ERROR) << "DestroyResizeYuv2Rgba Error: " << d_ptr_->estr_;
      }
      d_ptr_->yuv2rgba_ = nullptr;
    }
  }
  if (d_ptr_->queue_ == nullptr) {
    auto cnret = cnrtCreateQueue(&d_ptr_->queue_);
    if (cnret != CNRT_RET_SUCCESS) {
      LOG(WARNING) << "Create queue failed. Please SetMluQueue after.";
    }
  }
  return success;
}

int MluResizeYuv2Rgba::InvokeOp(void* dst, void* srcY, void* srcUV) {
  if (nullptr == d_ptr_->queue_) {
    throw MluResizeYuv2RgbaError("cnrt queue is null.");
  }
  if (d_ptr_->attr_.batch_size != 1) {
    throw MluResizeYuv2RgbaError(
        "InvokeOp is vaild only if the batchsize is 1. Please Use BatchingUp "
        "and SyncOneOutput to replase InvokeOp.");
  }
  BatchingUp(srcY, srcUV);
  if (!SyncOneOutput(dst)) {
    return -1;
  }
  return 0;
}

void MluResizeYuv2Rgba::BatchingUp(void* src_y, void* src_uv) {
  VLOG(5) << "Store resize and convert operator input for batching, " << src_y << " , " << src_uv;
  d_ptr_->yuv_ptrs_cache_.push_back(std::make_pair(src_y, src_uv));
}

bool MluResizeYuv2Rgba::SyncOneOutput(void* dst) {
  if (nullptr == d_ptr_->queue_) {
    throw MluResizeYuv2RgbaError("cnrt queue is null.");
  }
  if (static_cast<int>(d_ptr_->yuv_ptrs_cache_.size()) < d_ptr_->attr_.batch_size) {
    d_ptr_->estr_ = "Batchsize is " + std::to_string(d_ptr_->attr_.batch_size) + ", but only has" +
                    std::to_string(d_ptr_->yuv_ptrs_cache_.size());
    return false;
  }
  for (int bi = 0; bi < d_ptr_->attr_.batch_size; ++bi) {
    reinterpret_cast<void**>(d_ptr_->y_ptrs_cpu_)[bi] = d_ptr_->yuv_ptrs_cache_.front().first;
    reinterpret_cast<void**>(d_ptr_->uv_ptrs_cpu_)[bi] = d_ptr_->yuv_ptrs_cache_.front().second;
    d_ptr_->yuv_ptrs_cache_.pop_front();
  }
  cnrtRet_t cnret = cnrtMemcpy(d_ptr_->y_ptrs_mlu_, d_ptr_->y_ptrs_cpu_, sizeof(void*) * d_ptr_->attr_.batch_size,
                               CNRT_MEM_TRANS_DIR_HOST2DEV);
  if (cnret != CNRT_RET_SUCCESS) {
    d_ptr_->estr_ = "Memcpy host to device failed. Error code: " + std::to_string(cnret);
    return false;
  }
  cnret = cnrtMemcpy(d_ptr_->uv_ptrs_mlu_, d_ptr_->uv_ptrs_cpu_, sizeof(void*) * d_ptr_->attr_.batch_size,
                     CNRT_MEM_TRANS_DIR_HOST2DEV);
  if (cnret != CNRT_RET_SUCCESS) {
    d_ptr_->estr_ = "Memcpy host to device failed. Error code: " + std::to_string(cnret);
    return false;
  }
  VLOG(5) << "Do resize and convert process, dst: " << dst;

  return ::ComputeResizeYuv2Rgba(dst, d_ptr_->y_ptrs_mlu_, d_ptr_->uv_ptrs_mlu_, d_ptr_->yuv2rgba_,
                                 d_ptr_->queue_, &d_ptr_->estr_);
}

void MluResizeYuv2Rgba::Destroy() {
  if (d_ptr_) {
    if (d_ptr_->yuv2rgba_) {
      if (!::DestroyResizeYuv2Rgba(d_ptr_->yuv2rgba_, &d_ptr_->estr_)) {
        LOG(ERROR) << "DestroyResizeYuv2Rgba Error: " << d_ptr_->estr_;
      }
      d_ptr_->yuv2rgba_ = nullptr;
    }
    if (d_ptr_->y_ptrs_cpu_) {
      free(d_ptr_->y_ptrs_cpu_);
      d_ptr_->y_ptrs_cpu_ = nullptr;
    }
    if (d_ptr_->uv_ptrs_cpu_) {
      free(d_ptr_->uv_ptrs_cpu_);
      d_ptr_->uv_ptrs_cpu_ = nullptr;
    }
    if (d_ptr_->y_ptrs_mlu_) {
      cnrtFree(d_ptr_->y_ptrs_mlu_);
      d_ptr_->y_ptrs_mlu_ = nullptr;
    }
    if (d_ptr_->uv_ptrs_mlu_) {
      cnrtFree(d_ptr_->uv_ptrs_mlu_);
      d_ptr_->uv_ptrs_mlu_ = nullptr;
    }
    d_ptr_->yuv_ptrs_cache_.clear();

    if (d_ptr_->queue_is_exclusive_) {
      DestroyMluQueue();
    }
  }
}

}  // namespace edk
