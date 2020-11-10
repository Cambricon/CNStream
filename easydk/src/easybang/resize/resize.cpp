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

#include "easybang/resize.h"

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include "cnrt.h"

using std::string;

struct ResizeKernelParam;

extern int PrepareKernelParam(uint32_t s_row, uint32_t s_col, uint32_t src_stride_y, uint32_t src_stride_uv,
                              uint32_t d_row, uint32_t d_col, uint32_t batch, uint32_t channel_id,
                              ResizeKernelParam** param, string* estr);

extern void FreeKernelParam(ResizeKernelParam* param);

extern float Resize(void** dstY, void** dstUV, void** srcY, void** srcUV, ResizeKernelParam* param,
                    cnrtFunctionType_t func_type, cnrtDim3_t dim, cnrtQueue_t queue, string* estr);

namespace edk {

class MluResizePrivate {
 public:
  bool queue_is_exclusive_ = true;
  cnrtFunctionType_t ftype_ = CNRT_FUNC_TYPE_BLOCK;
  cnrtQueue_t queue_ = nullptr;
  ResizeKernelParam* kparam_ = nullptr;
  void **y_ptrs_cpu_ = nullptr, **uv_ptrs_cpu_ = nullptr;
  void **y_ptrs_mlu_ = nullptr, **uv_ptrs_mlu_ = nullptr;
  void **dst_y_cpu_ = nullptr, **dst_uv_cpu_ = nullptr;
  void **dst_y_mlu_ = nullptr, **dst_uv_mlu_ = nullptr;
  std::string estr_;
  std::deque<std::pair<void*, void*>> yuv_ptrs_cache_;
  MluResize::Attr attr_;
};  // MluResziePrivate

MluResize::MluResize() { d_ptr_ = new MluResizePrivate; }

MluResize::~MluResize() {
  if (d_ptr_) {
    Destroy();
    delete d_ptr_;
    d_ptr_ = nullptr;
  }
}

const MluResize::Attr& MluResize::GetAttr() { return d_ptr_->attr_; }

cnrtQueue_t MluResize::GetMluQueue() const { return d_ptr_->queue_; }

void MluResize::SetMluQueue(cnrtQueue_t queue, bool exclusive) {
  if (d_ptr_->queue_is_exclusive_) {
    DestroyMluQueue();
  }
  d_ptr_->queue_is_exclusive_ = exclusive;
  d_ptr_->queue_ = queue;
}

void MluResize::DestroyMluQueue() {
  if (d_ptr_->queue_ != nullptr) {
    cnrtDestroyQueue(d_ptr_->queue_);
  }
  d_ptr_->queue_ = nullptr;
}

std::string MluResize::GetLastError() const { return d_ptr_->estr_; }

#define CHECK_CONDITION_WITH_CODE(cond, _estr, msg, code, ret_value) \
  do {                                                               \
    if (!(cond)) {                                                   \
      _estr = msg;                                                   \
      { code }                                                       \
      return ret_value;                                              \
    }                                                                \
  } while (0)

#define CHECK_CNRT_RET(cnrt_ret, _estr, msg, code, ret_value) \
  CHECK_CONDITION_WITH_CODE((cnrt_ret == CNRT_RET_SUCCESS), _estr, msg, code, ret_value)

bool MluResize::Init(const MluResize::Attr& attr) {
  d_ptr_->attr_ = attr;

  d_ptr_->y_ptrs_cpu_ = reinterpret_cast<void**>(malloc(sizeof(char*) * attr.batch_size));
  d_ptr_->uv_ptrs_cpu_ = reinterpret_cast<void**>(malloc(sizeof(char*) * attr.batch_size));
  cnrtRet_t cnret = cnrtMalloc(reinterpret_cast<void**>(&d_ptr_->y_ptrs_mlu_), sizeof(char*) * attr.batch_size);
  CHECK_CNRT_RET(cnret, d_ptr_->estr_, "Malloc src mlu buffer failed. cnrt error code:" + std::to_string(cnret), {},
                 false);
  cnret = cnrtMalloc(reinterpret_cast<void**>(&d_ptr_->uv_ptrs_mlu_), sizeof(char*) * attr.batch_size);
  CHECK_CNRT_RET(cnret, d_ptr_->estr_, "Malloc src mlu buffer failed. cnrt error code:" + std::to_string(cnret), {},
                 false);

  d_ptr_->dst_y_cpu_ = reinterpret_cast<void**>(malloc(sizeof(char*) * attr.batch_size));
  d_ptr_->dst_uv_cpu_ = reinterpret_cast<void**>(malloc(sizeof(char*) * attr.batch_size));
  cnret = cnrtMalloc(reinterpret_cast<void**>(&d_ptr_->dst_y_mlu_), sizeof(char*) * attr.batch_size);
  CHECK_CNRT_RET(cnret, d_ptr_->estr_, "Malloc dst mlu buffer failed. cnrt error code:" + std::to_string(cnret), {},
                 false);
  cnret = cnrtMalloc(reinterpret_cast<void**>(&d_ptr_->dst_uv_mlu_), sizeof(char*) * attr.batch_size);
  CHECK_CNRT_RET(cnret, d_ptr_->estr_, "Malloc dst mlu buffer failed. cnrt error code:" + std::to_string(cnret), {},
                 false);

  switch (attr.core) {
    case 1:
      d_ptr_->ftype_ = CNRT_FUNC_TYPE_BLOCK;
      break;
    case 4:
      d_ptr_->ftype_ = CNRT_FUNC_TYPE_UNION1;
      break;
    case 8:
      d_ptr_->ftype_ = CNRT_FUNC_TYPE_UNION2;
      break;
    default:
      d_ptr_->estr_ = "Unsupport union mode. Only support 1(block), 4(u1), 8(u2)";
      return false;
  }

  if (CNRT_RET_SUCCESS != cnrtCreateQueue(&d_ptr_->queue_)) {
    d_ptr_->estr_ = "cnrtCreateQueue failed";
    return false;
  }
  return 0 == ::PrepareKernelParam(d_ptr_->attr_.src_h, d_ptr_->attr_.src_w, d_ptr_->attr_.src_stride_y,
                                   d_ptr_->attr_.src_stride_uv, d_ptr_->attr_.dst_h, d_ptr_->attr_.dst_w,
                                   d_ptr_->attr_.batch_size, d_ptr_->attr_.channel_id, &d_ptr_->kparam_,
                                   &d_ptr_->estr_);
}

int MluResize::InvokeOp(void* dst_y, void* dst_uv, void* srcY, void* srcUV) {
  if (nullptr == d_ptr_->queue_) {
    THROW_EXCEPTION(Exception::INTERNAL, "cnrt queue is null.");
  }
  if (d_ptr_->attr_.batch_size != 1) {
    THROW_EXCEPTION(Exception::INVALID_ARG,
                    "InvokeOp is vaild only if the batchsize is 1. Please Use BatchingUp "
                    "and SyncOneOutput to replase InvokeOp.");
  }
  BatchingUp(srcY, srcUV);
  if (!SyncOneOutput(dst_y, dst_uv)) {
    return -1;
  }
  return 0;
}

void MluResize::BatchingUp(void* src_y, void* src_uv) {
  d_ptr_->yuv_ptrs_cache_.push_back(std::make_pair(src_y, src_uv));
}

bool MluResize::SyncOneOutput(void* dst_y, void* dst_uv) {
  if (nullptr == d_ptr_->queue_) {
    THROW_EXCEPTION(Exception::INTERNAL, "cnrt queue is null.");
  }
  CHECK_CONDITION_WITH_CODE(static_cast<int>(d_ptr_->yuv_ptrs_cache_.size()) >= d_ptr_->attr_.batch_size, d_ptr_->estr_,
                            "Batchsize is " + std::to_string(d_ptr_->attr_.batch_size) + ", but only has" +
                                std::to_string(d_ptr_->yuv_ptrs_cache_.size()),
                            {}, false);
  if (static_cast<int>(d_ptr_->yuv_ptrs_cache_.size()) < d_ptr_->attr_.batch_size) {
    d_ptr_->estr_ = "Batchsize is " + std::to_string(d_ptr_->attr_.batch_size) + ", but only has" +
                    std::to_string(d_ptr_->yuv_ptrs_cache_.size());
    return false;
  }
  size_t y_plane_size = d_ptr_->attr_.dst_h * d_ptr_->attr_.dst_w;
  size_t uv_plane_size = d_ptr_->attr_.dst_h * d_ptr_->attr_.dst_w / 2;
  for (int bi = 0; bi < d_ptr_->attr_.batch_size; ++bi) {
    d_ptr_->y_ptrs_cpu_[bi] = d_ptr_->yuv_ptrs_cache_.front().first;
    d_ptr_->uv_ptrs_cpu_[bi] = d_ptr_->yuv_ptrs_cache_.front().second;
    d_ptr_->yuv_ptrs_cache_.pop_front();

    d_ptr_->dst_y_cpu_[bi] = reinterpret_cast<void*>(reinterpret_cast<int64_t>(dst_y) + bi * y_plane_size);
    d_ptr_->dst_uv_cpu_[bi] = reinterpret_cast<void*>(reinterpret_cast<int64_t>(dst_uv) + bi * uv_plane_size);
  }
  cnrtRet_t cnret = cnrtMemcpy(d_ptr_->y_ptrs_mlu_, reinterpret_cast<void**>(d_ptr_->y_ptrs_cpu_),
                               sizeof(char*) * d_ptr_->attr_.batch_size, CNRT_MEM_TRANS_DIR_HOST2DEV);
  CHECK_CNRT_RET(cnret, d_ptr_->estr_, "Memcpy host to device failed. cnrt error code:" + std::to_string(cnret), {},
                 false);
  cnret = cnrtMemcpy(d_ptr_->uv_ptrs_mlu_, reinterpret_cast<void**>(d_ptr_->uv_ptrs_cpu_),
                     sizeof(char*) * d_ptr_->attr_.batch_size, CNRT_MEM_TRANS_DIR_HOST2DEV);
  CHECK_CNRT_RET(cnret, d_ptr_->estr_, "Memcpy host to device failed. cnrt error code:" + std::to_string(cnret), {},
                 false);

  cnret = cnrtMemcpy(d_ptr_->dst_y_mlu_, reinterpret_cast<void**>(d_ptr_->dst_y_cpu_),
                     sizeof(char*) * d_ptr_->attr_.batch_size, CNRT_MEM_TRANS_DIR_HOST2DEV);
  CHECK_CNRT_RET(cnret, d_ptr_->estr_, "Memcpy host to device failed. cnrt error code:" + std::to_string(cnret), {},
                 false);
  cnret = cnrtMemcpy(d_ptr_->dst_uv_mlu_, reinterpret_cast<void**>(d_ptr_->dst_uv_cpu_),
                     sizeof(char*) * d_ptr_->attr_.batch_size, CNRT_MEM_TRANS_DIR_HOST2DEV);
  CHECK_CNRT_RET(cnret, d_ptr_->estr_, "Memcpy host to device failed. cnrt error code:" + std::to_string(cnret), {},
                 false);
  cnrtDim3_t dim;
  dim.x = d_ptr_->attr_.core;
  dim.y = 1;
  dim.z = 1;
  return -1 != ::Resize(d_ptr_->dst_y_mlu_, d_ptr_->dst_uv_mlu_, d_ptr_->y_ptrs_mlu_, d_ptr_->uv_ptrs_mlu_,
                        d_ptr_->kparam_, d_ptr_->ftype_, dim, d_ptr_->queue_, &d_ptr_->estr_);
}

void MluResize::Destroy() {
  if (d_ptr_->kparam_) {
    ::FreeKernelParam(d_ptr_->kparam_);
    d_ptr_->kparam_ = nullptr;
  }
  if (d_ptr_->y_ptrs_cpu_) {
    free(d_ptr_->y_ptrs_cpu_);
    d_ptr_->y_ptrs_cpu_ = nullptr;
  }
  if (d_ptr_->uv_ptrs_cpu_) {
    free(d_ptr_->uv_ptrs_cpu_);
    d_ptr_->uv_ptrs_cpu_ = nullptr;
  }
  if (d_ptr_->dst_y_cpu_) {
    free(d_ptr_->dst_y_cpu_);
    d_ptr_->dst_y_cpu_ = nullptr;
  }
  if (d_ptr_->dst_uv_cpu_) {
    free(d_ptr_->dst_uv_cpu_);
    d_ptr_->dst_uv_cpu_ = nullptr;
  }
  if (d_ptr_->y_ptrs_mlu_) {
    cnrtFree(d_ptr_->y_ptrs_mlu_);
    d_ptr_->y_ptrs_mlu_ = nullptr;
  }
  if (d_ptr_->uv_ptrs_mlu_) {
    cnrtFree(d_ptr_->uv_ptrs_mlu_);
    d_ptr_->uv_ptrs_mlu_ = nullptr;
  }
  if (d_ptr_->dst_y_mlu_) {
    cnrtFree(d_ptr_->dst_y_mlu_);
    d_ptr_->dst_y_mlu_ = nullptr;
  }
  if (d_ptr_->dst_uv_mlu_) {
    cnrtFree(d_ptr_->dst_uv_mlu_);
    d_ptr_->dst_uv_mlu_ = nullptr;
  }
  d_ptr_->yuv_ptrs_cache_.clear();
  if (d_ptr_->queue_is_exclusive_) {
    DestroyMluQueue();
  }
}

}  // namespace edk
