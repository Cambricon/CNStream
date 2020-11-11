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

#include "easyplugin/resize_yuv_to_yuv.h"

#include <deque>
#include <memory>
#include <string>
#include <utility>

#include "cnrt.h"
#include "glog/logging.h"

using std::string;
using std::to_string;
extern bool CreateResizeYuv2Yuv(const edk::MluResizeAttr& attr, ResizeYuv2Yuv** yuv2yuv_ptr, string* estr);

extern bool DestroyResizeYuv2Yuv(ResizeYuv2Yuv* yuv2yuv, string* estr);

extern bool ComputeResizeYuv2Yuv(void* dst_y, void* dst_uv, void* src_y, void* src_uv, ResizeYuv2Yuv* yuv2yuv,
                                 cnrtQueue_t queue, string* estr);

namespace edk {

class MluResizeYuv2YuvPrivate {
 public:
  bool queue_is_exclusive_ = true;
  cnrtQueue_t queue_ = nullptr;
  ResizeYuv2Yuv* yuv2yuv_ = nullptr;
  void *src_y_ptrs_cpu_ = nullptr, *src_uv_ptrs_cpu_ = nullptr;
  void *src_y_ptrs_mlu_ = nullptr, *src_uv_ptrs_mlu_ = nullptr;
  void *dst_y_ptrs_cpu_ = nullptr, *dst_uv_ptrs_cpu_ = nullptr;
  void *dst_y_ptrs_mlu_ = nullptr, *dst_uv_ptrs_mlu_ = nullptr;
  std::string estr_;
  std::deque<std::pair<void*, void*>> src_yuv_ptrs_cache_;
  std::deque<std::pair<void*, void*>> dst_yuv_ptrs_cache_;
  MluResizeAttr attr_;
};

MluResizeYuv2Yuv::MluResizeYuv2Yuv() { d_ptr_ = new MluResizeYuv2YuvPrivate; }
MluResizeYuv2Yuv::MluResizeYuv2Yuv(const MluResizeAttr& attr) {
  d_ptr_ = new MluResizeYuv2YuvPrivate;
  Init(attr);
}

MluResizeYuv2Yuv::~MluResizeYuv2Yuv() {
  if (d_ptr_) {
    Destroy();
    delete d_ptr_;
    d_ptr_ = nullptr;
  }
}

const MluResizeAttr& MluResizeYuv2Yuv::GetAttr() { return d_ptr_->attr_; }

cnrtQueue_t MluResizeYuv2Yuv::GetMluQueue() const { return d_ptr_->queue_; }

void MluResizeYuv2Yuv::SetMluQueue(cnrtQueue_t queue, bool exclusive) {
  if (d_ptr_->queue_is_exclusive_) {
    DestroyMluQueue();
  }
  d_ptr_->queue_is_exclusive_ = exclusive;
  d_ptr_->queue_ = queue;
}

void MluResizeYuv2Yuv::DestroyMluQueue() {
  if (d_ptr_->queue_ != nullptr) {
    cnrtDestroyQueue(d_ptr_->queue_);
  }
  d_ptr_->queue_ = nullptr;
}

std::string MluResizeYuv2Yuv::GetLastError() const { return d_ptr_->estr_; }

bool MluResizeYuv2Yuv::Init(const MluResizeAttr& attr) {
  d_ptr_->attr_ = attr;

  if (attr.core_version == CoreVersion::MLU270) {
    switch (attr.core_number) {
      case 0:
        d_ptr_->attr_.core_number = kMLU270CoreNum;
        break;
      case 4: case 8: case 16:
        break;
      default:
        std::cout << "[ResizeYuv2Rgba] core number: " << attr.core_number
                  << " is not supported. Choose from 4, 8, 16 instead." << std::endl;
        d_ptr_->estr_ = "Wrong core number. Choose from 4, 8, 16 instead.";
        return false;
    }
  } else if (attr.core_version == CoreVersion::MLU220) {
    switch (attr.core_number) {
      case 0:
        d_ptr_->attr_.core_number = kMLU220CoreNum;
        break;
      default:
        std::cout << "[ResizeYuv2Rgba] core number: " << attr.core_number
          << " is not supported. Use 4 instead." << std::endl;
        d_ptr_->estr_ = "Wrong core number. Should be 4.";
        return false;
    }
  }
  // malloc cpu
  d_ptr_->src_y_ptrs_cpu_ = malloc(sizeof(void*) * attr.batch_size);
  d_ptr_->src_uv_ptrs_cpu_ = malloc(sizeof(void*) * attr.batch_size);
  d_ptr_->dst_y_ptrs_cpu_ = malloc(sizeof(void*) * attr.batch_size);
  d_ptr_->dst_uv_ptrs_cpu_ = malloc(sizeof(void*) * attr.batch_size);
  // malloc mlu
  cnrtRet_t cnret = cnrtMalloc(&d_ptr_->src_y_ptrs_mlu_, sizeof(void*) * attr.batch_size);
  if (!CnrtCheck(cnret, &d_ptr_->estr_, "Malloc src y mlu buffer failed.")) return false;
  cnret = cnrtMalloc(&d_ptr_->src_uv_ptrs_mlu_, sizeof(void*) * attr.batch_size);
  if (!CnrtCheck(cnret, &d_ptr_->estr_, "Malloc src uv mlu buffer failed.")) return false;
  cnret = cnrtMalloc(&d_ptr_->dst_y_ptrs_mlu_, sizeof(void*) * attr.batch_size);
  if (!CnrtCheck(cnret, &d_ptr_->estr_, "Malloc dst y mlu buffer failed.")) return false;
  cnret = cnrtMalloc(&d_ptr_->dst_uv_ptrs_mlu_, sizeof(void*) * attr.batch_size);
  if (!CnrtCheck(cnret, &d_ptr_->estr_, "Malloc dst uv mlu buffer failed.")) return false;

  VLOG(4) <<  "Init ResizeYuvToYuv Operator";

  bool success = ::CreateResizeYuv2Yuv(d_ptr_->attr_, &d_ptr_->yuv2yuv_, &d_ptr_->estr_);
  if (!success) {
    LOG(ERROR) << "Create ResizeYuvToYuv failed. Error: " << d_ptr_->estr_;
    if (d_ptr_->yuv2yuv_) {
      if (!::DestroyResizeYuv2Yuv(d_ptr_->yuv2yuv_, &d_ptr_->estr_)) {
        LOG(ERROR) << "DestroyResizeYuv2Yuv Error: " << d_ptr_->estr_;
      }
      d_ptr_->yuv2yuv_ = nullptr;
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

int MluResizeYuv2Yuv::InvokeOp(void* dst_y, void* dst_uv, void* src_y, void* src_uv) {
  if (nullptr == d_ptr_->queue_) {
    THROW_EXCEPTION(Exception::INTERNAL, "cnrt queue is null.");
  }
  if (d_ptr_->attr_.batch_size != 1) {
    THROW_EXCEPTION(Exception::INVALID_ARG,
                    "InvokeOp is vaild only if the batchsize is 1. Please Use BatchingUp "
                    "and SyncOneOutput to replace InvokeOp.");
  }
  SrcBatchingUp(src_y, src_uv);
  DstBatchingUp(dst_y, dst_uv);
  if (!SyncOneOutput()) {
    return -1;
  }
  return 0;
}

void MluResizeYuv2Yuv::SrcBatchingUp(void* y, void* uv) {
  VLOG(5) << "Store resize yuv2yuv input for batching, " << y << ", " <<  uv;
  d_ptr_->src_yuv_ptrs_cache_.push_back(std::make_pair(y, uv));
}

void MluResizeYuv2Yuv::DstBatchingUp(void* y, void* uv) {
  VLOG(5) << "Store resize yuv2yuv output for batching, " << y << ", " <<  uv;
  d_ptr_->dst_yuv_ptrs_cache_.push_back(std::make_pair(y, uv));
}

bool MluResizeYuv2Yuv::SyncOneOutput() {
  if (nullptr == d_ptr_->queue_) {
    THROW_EXCEPTION(Exception::INTERNAL, "cnrt queue is null.");
  }
  if (static_cast<int>(d_ptr_->src_yuv_ptrs_cache_.size()) < d_ptr_->attr_.batch_size ||
      static_cast<int>(d_ptr_->dst_yuv_ptrs_cache_.size()) < d_ptr_->attr_.batch_size) {
    d_ptr_->estr_ = "Batchsize is " + std::to_string(d_ptr_->attr_.batch_size) +
                    ", but only has input: " + std::to_string(d_ptr_->src_yuv_ptrs_cache_.size()) +
                    ", output: " + std::to_string(d_ptr_->dst_yuv_ptrs_cache_.size());
    return false;
  }
  for (int bi = 0; bi < d_ptr_->attr_.batch_size; ++bi) {
    reinterpret_cast<void**>(d_ptr_->src_y_ptrs_cpu_)[bi] = d_ptr_->src_yuv_ptrs_cache_.front().first;
    reinterpret_cast<void**>(d_ptr_->src_uv_ptrs_cpu_)[bi] = d_ptr_->src_yuv_ptrs_cache_.front().second;
    reinterpret_cast<void**>(d_ptr_->dst_y_ptrs_cpu_)[bi] = d_ptr_->dst_yuv_ptrs_cache_.front().first;
    reinterpret_cast<void**>(d_ptr_->dst_uv_ptrs_cpu_)[bi] = d_ptr_->dst_yuv_ptrs_cache_.front().second;
    d_ptr_->src_yuv_ptrs_cache_.pop_front();
    d_ptr_->dst_yuv_ptrs_cache_.pop_front();
  }
  cnrtRet_t cnret = cnrtMemcpy(d_ptr_->src_y_ptrs_mlu_, d_ptr_->src_y_ptrs_cpu_,
                               sizeof(void*) * d_ptr_->attr_.batch_size, CNRT_MEM_TRANS_DIR_HOST2DEV);
  if (!CnrtCheck(cnret, &d_ptr_->estr_, "Memcpy src y from host to device failed.")) return false;
  cnret = cnrtMemcpy(d_ptr_->src_uv_ptrs_mlu_, d_ptr_->src_uv_ptrs_cpu_, sizeof(void*) * d_ptr_->attr_.batch_size,
                     CNRT_MEM_TRANS_DIR_HOST2DEV);
  if (!CnrtCheck(cnret, &d_ptr_->estr_, "Memcpy src uv from host to device failed.")) return false;
  cnret = cnrtMemcpy(d_ptr_->dst_y_ptrs_mlu_, d_ptr_->dst_y_ptrs_cpu_, sizeof(void*) * d_ptr_->attr_.batch_size,
                     CNRT_MEM_TRANS_DIR_HOST2DEV);
  if (!CnrtCheck(cnret, &d_ptr_->estr_, "Memcpy dst y from host to device failed.")) return false;
  cnret = cnrtMemcpy(d_ptr_->dst_uv_ptrs_mlu_, d_ptr_->dst_uv_ptrs_cpu_, sizeof(void*) * d_ptr_->attr_.batch_size,
                     CNRT_MEM_TRANS_DIR_HOST2DEV);
  if (!CnrtCheck(cnret, &d_ptr_->estr_, "Memcpy dst uv from host to device failed.")) return false;

  VLOG(5) << "Do resize yuv2yuv process, dst_y: " << d_ptr_->dst_y_ptrs_mlu_
             << ", dst_uv: " << d_ptr_->dst_y_ptrs_mlu_;
  return ::ComputeResizeYuv2Yuv(d_ptr_->dst_y_ptrs_mlu_, d_ptr_->dst_uv_ptrs_mlu_,
                                d_ptr_->src_y_ptrs_mlu_, d_ptr_->src_uv_ptrs_mlu_,
                                d_ptr_->yuv2yuv_, d_ptr_->queue_, &d_ptr_->estr_);
}

void MluResizeYuv2Yuv::Destroy() {
  if (d_ptr_) {
    if (d_ptr_->yuv2yuv_) {
      if (!::DestroyResizeYuv2Yuv(d_ptr_->yuv2yuv_, &d_ptr_->estr_)) {
        LOG(ERROR) << "DestroyResizeYuv2Yuv Error: " << d_ptr_->estr_;
      }
      d_ptr_->yuv2yuv_ = nullptr;
    }
    if (d_ptr_->src_y_ptrs_cpu_) {
      free(d_ptr_->src_y_ptrs_cpu_);
      d_ptr_->src_y_ptrs_cpu_ = nullptr;
    }
    if (d_ptr_->src_uv_ptrs_cpu_) {
      free(d_ptr_->src_uv_ptrs_cpu_);
      d_ptr_->src_uv_ptrs_cpu_ = nullptr;
    }
    if (d_ptr_->dst_y_ptrs_cpu_) {
      free(d_ptr_->dst_y_ptrs_cpu_);
      d_ptr_->dst_y_ptrs_cpu_ = nullptr;
    }
    if (d_ptr_->dst_uv_ptrs_cpu_) {
      free(d_ptr_->dst_uv_ptrs_cpu_);
      d_ptr_->dst_uv_ptrs_cpu_ = nullptr;
    }
    if (d_ptr_->src_y_ptrs_mlu_) {
      cnrtFree(d_ptr_->src_y_ptrs_mlu_);
      d_ptr_->src_y_ptrs_mlu_ = nullptr;
    }
    if (d_ptr_->src_uv_ptrs_mlu_) {
      cnrtFree(d_ptr_->src_uv_ptrs_mlu_);
      d_ptr_->src_uv_ptrs_mlu_ = nullptr;
    }
    if (d_ptr_->dst_y_ptrs_mlu_) {
      cnrtFree(d_ptr_->dst_y_ptrs_mlu_);
      d_ptr_->dst_y_ptrs_mlu_ = nullptr;
    }
    if (d_ptr_->dst_uv_ptrs_mlu_) {
      cnrtFree(d_ptr_->dst_uv_ptrs_mlu_);
      d_ptr_->dst_uv_ptrs_mlu_ = nullptr;
    }
    d_ptr_->src_yuv_ptrs_cache_.clear();
    d_ptr_->dst_yuv_ptrs_cache_.clear();
    if (d_ptr_->queue_is_exclusive_) {
      DestroyMluQueue();
    }
  }
}

}  // namespace edk
