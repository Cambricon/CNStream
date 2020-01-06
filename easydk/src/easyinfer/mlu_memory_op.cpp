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

#include "easyinfer/mlu_memory_op.h"
#include <cstring>
#include <memory>
#include <string>
#include "cxxutil/logger.h"
#include "easyinfer/model_loader.h"
#include "model_loader_internal.h"

namespace edk {

#define CHECK_MODEL_LOADER                         \
  if (!ploader_) {                                 \
    throw MluMemoryOpError("ModelLoader Not Set"); \
  }

#define CHECK_CNRT_RET(str, err_code)                                                        \
  if (CNRT_RET_SUCCESS != err_code) {                                                        \
    throw MluMemoryOpError(std::string(str) + " error code: " + std::to_string(error_code)); \
  }

#define ONLY_SUPPORT_FLOAT32_ON_CPU                        \
  do {                                                     \
    int num = ploader_->InputNum();                       \
    for (int i = 0; i < num; ++i) {                        \
      DataLayout layout = ploader_->GetCpuInputLayout(i);  \
      if (layout.dtype != DataType::FLOAT32) {             \
        throw MluMemoryOpError(                            \
            "Only support cpu"                             \
            " layout with data type FLOAT32");             \
      }                                                    \
    }                                                      \
    num = ploader_->OutputNum();                          \
    for (int i = 0; i < num; ++i) {                        \
      DataLayout layout = ploader_->GetCpuOutputLayout(i); \
      if (layout.dtype != DataType::FLOAT32) {             \
        throw MluMemoryOpError(                            \
            "Only support cpu"                             \
            " layout with data type FLOAT32");             \
      }                                                    \
    }                                                      \
  } while (0)

extern cnrtDataType CastDataType(const DataType &type);
extern DataType CastDataType(const cnrtDataType &type);
extern cnrtDimOrder CastDimOrder(const DimOrder &order);
extern DimOrder CastDimOrder(const cnrtDimOrder &order);

#ifdef CNSTK_MLU270
static size_t TypeSize(const DataType &type) {
  switch (type) {
    case DataType::UINT8:
      return sizeof(uint8_t);
    case DataType::FLOAT32:
      return sizeof(float);
    case DataType::FLOAT16:
      return sizeof(int16_t);
    case DataType::INT16:
      return sizeof(int16_t);
    case DataType::INT32:
      return sizeof(int32_t);
    default:
      throw MluMemoryOpError("Unsupported data type");
  }
}

static void TransLayout(const DataLayout &src_layout, const DataLayout &dst_layout, void *src_data, void *dst_data,
                        const Shape &shape) {
  if (src_layout.order != DimOrder::NHWC && src_layout.order != DimOrder::NCHW) {
    throw MluMemoryOpError("TransLayout: Unsupport data order(src).");
  }
  if (dst_layout.order != DimOrder::NHWC && dst_layout.order != DimOrder::NCHW) {
    throw MluMemoryOpError("TransLayout: Unsupport data order(dst).");
  }

  char bits = 0;
  if (src_layout.dtype != dst_layout.dtype) bits |= 1 << 0;
  if (src_layout.order != dst_layout.order) bits |= 1 << 1;
  cnrtRet_t error_code = CNRT_RET_SUCCESS;
  int size = shape.DataCount();
  int dim_values[4] = {static_cast<int>(shape.n), static_cast<int>(shape.h), static_cast<int>(shape.w),
                       static_cast<int>(shape.c)};
  int dim_order[4];
  if (dst_layout.order == DimOrder::NHWC) {
    dim_order[0] = 0, dim_order[1] = 2, dim_order[2] = 3, dim_order[3] = 1;
  } else if (dst_layout.order == DimOrder::NCHW) {
    dim_order[0] = 0, dim_order[1] = 3, dim_order[2] = 1, dim_order[3] = 2;
  } else {
    throw MluMemoryOpError("TransLayout: Unsupport data order(dst).");
  }
  switch (bits) {
    case 1 << 0:
      error_code = cnrtCastDataType(src_data, CastDataType(src_layout.dtype), dst_data, CastDataType(dst_layout.dtype),
                                    size, nullptr);
      CHECK_CNRT_RET("Cast data type failed.", error_code);
      break;
    case 1 << 1:
      error_code = cnrtTransDataOrder(src_data, CastDataType(src_layout.dtype), dst_data, 4, dim_values, dim_order);
      CHECK_CNRT_RET("Trans data order failed.", error_code);
      break;
    case 1 << 0 | 1 << 1:
      error_code = cnrtTransOrderAndCast(src_data, CastDataType(src_layout.dtype), dst_data,
                                         CastDataType(dst_layout.dtype), nullptr, 4, dim_values, dim_order);
      CHECK_CNRT_RET("Trans data order and cast data type failed.", error_code);
      break;
    default:
      size_t mem_size = size * TypeSize(src_layout.dtype);
      memcpy(dst_data, src_data, mem_size);
      break;
  }
}
#endif

MluMemoryOp::MluMemoryOp() : ploader_(nullptr) {}

void MluMemoryOp::SetLoader(std::shared_ptr<ModelLoader> ploader) { ploader_ = ploader; }

std::shared_ptr<ModelLoader> MluMemoryOp::Loader() const { return ploader_; }

void **MluMemoryOp::AllocCpuInput(uint32_t batch_size) const {
  CHECK_MODEL_LOADER;
  ONLY_SUPPORT_FLOAT32_ON_CPU;
  auto &shapes = ploader_->InputShapes();
  uint32_t num = ploader_->InputNum();

  LOG(TRACE, "Alloc memory on CPU for model input");

  void **ret = new void *[num];
  for (uint32_t i = 0; i < num; ++i) {
    uint64_t data_cnt = shapes[i].DataCount();
    ret[i] = static_cast<void *>(new float[data_cnt * batch_size]);
  }
  return ret;
}

void **MluMemoryOp::AllocCpuOutput(uint32_t batch_size) const {
  CHECK_MODEL_LOADER;
  ONLY_SUPPORT_FLOAT32_ON_CPU;
  auto &shapes = ploader_->OutputShapes();
  uint32_t num = shapes.size();

  LOG(TRACE, "Alloc memory on CPU for model output");

  void **ret = new void *[num];
  for (uint32_t i = 0; i < num; ++i) {
    uint64_t data_cnt = shapes[i].DataCount();
    ret[i] = static_cast<void *>(new float[data_cnt * batch_size]);
  }
  return ret;
}

void **MluMemoryOp::AllocMluInput(uint32_t batch_size) const {
  CHECK_MODEL_LOADER;
  void **ret = nullptr;
  cnrtRet_t error_code;
  uint32_t num = ploader_->InputNum();
  ModelLoaderInternalInterface interface(ploader_.get());

  LOG(TRACE, "Alloc memory on MLU for model input");

#ifdef CNSTK_MLU100
  cnrtDataDescArray_t desc = interface.InputDescArray();
  error_code = cnrtMallocBatchByDescArray(&ret, desc, num, batch_size);
  CHECK_CNRT_RET("Mlu malloc failed.", error_code);
#elif CNSTK_MLU270
  ret = new void *[num];
  for (uint32_t i = 0; i < num; ++i) {
    void *t = nullptr;
    int64_t size = interface.InputDataSize(i);
    error_code = cnrtMalloc(&t, size);
    CHECK_CNRT_RET("Mlu malloc failed.", error_code);
    ret[i] = t;
  }
#endif
  return ret;
}

void **MluMemoryOp::AllocMluOutput(uint32_t batch_size) const {
  CHECK_MODEL_LOADER;
  void **ret = nullptr;
  cnrtRet_t error_code;
  uint32_t num = ploader_->OutputNum();
  ModelLoaderInternalInterface interface(ploader_.get());

  LOG(TRACE, "Alloc memory on MLU for model output, batch size %u", batch_size);

#ifdef CNSTK_MLU100
  cnrtDataDescArray_t desc = interface.OutputDescArray();
  error_code = cnrtMallocBatchByDescArray(&ret, desc, num, batch_size);
  CHECK_CNRT_RET("Mlu malloc failed.", error_code);
#elif CNSTK_MLU270
  ret = new void *[num];
  for (uint32_t i = 0; i < num; ++i) {
    void *t = nullptr;
    int64_t size = interface.OutputDataSize(i);
    error_code = cnrtMalloc(&t, size);
    CHECK_CNRT_RET("Mlu malloc failed.", error_code);
    ret[i] = t;
  }
#endif
  return ret;
}

void *MluMemoryOp::AllocMlu(size_t nBytes, uint32_t batch_size) const {
  void *ret = nullptr;
  cnrtRet_t error_code;
  LOG(TRACE, "Alloc memory on MLU: %lu bytes, batch size %u", nBytes, batch_size);
#ifdef CNSTK_MLU100
  error_code = cnrtMallocBatch(&ret, nBytes, batch_size);
  CHECK_CNRT_RET("Mlu malloc failed.", error_code);
#elif CNSTK_MLU270
  error_code = cnrtMalloc(&ret, nBytes * batch_size);
  CHECK_CNRT_RET("Mlu malloc failed.", error_code);
#endif
  return ret;
}

void MluMemoryOp::FreeCpuInput(void **ptr) const {
  CHECK_MODEL_LOADER;
  LOG(TRACE, "Free input memory on CPU");
  uint32_t num = ploader_->InputNum();
  for (uint32_t i = 0; i < num; ++i) {
    delete[] static_cast<float *>(ptr[i]);
  }
  delete[] ptr;
}

void MluMemoryOp::FreeCpuOutput(void **ptr) const {
  CHECK_MODEL_LOADER;
  LOG(TRACE, "Free output memory on CPU");
  uint32_t num = ploader_->OutputNum();
  for (uint32_t i = 0; i < num; ++i) {
    delete[] static_cast<float *>(ptr[i]);
  }
  delete[] ptr;
}

void MluMemoryOp::FreeArrayMlu(void **ptr, uint32_t mem_num) const {
  LOG(TRACE, "Free memory array on MLU");
#ifdef CNSTK_MLU100
  cnrtFreeArray(ptr, mem_num);
#elif CNSTK_MLU270
  for (uint32_t i = 0; i < mem_num; ++i) {
    cnrtFree(ptr[i]);
  }
#endif
}

void MluMemoryOp::FreeMlu(void *ptr) const {
  LOG(TRACE, "Free memory on MLU");
  cnrtFree(ptr);
}

void MluMemoryOp::MemcpyInputH2D(void **mlu_dst, void **cpu_src, uint32_t batch_size) const {
  CHECK_MODEL_LOADER;
  ONLY_SUPPORT_FLOAT32_ON_CPU;
  ModelLoaderInternalInterface interface(ploader_.get());
  cnrtRet_t error_code;
  LOG(TRACE, "copy input memory from host to device");

#ifdef CNSTK_MLU100
  error_code = cnrtMemcpyBatchByDescArray(mlu_dst, cpu_src, interface.InputDescArray(), ploader_->InputNum(),
                                          batch_size, CNRT_MEM_TRANS_DIR_HOST2DEV);
  CHECK_CNRT_RET("Memcpy host to device failed.", error_code);
#elif CNSTK_MLU270
  int64_t num = ploader_->InputNum();
  for (int i = 0; i < num; ++i) {
    void *src = cpu_src[i];
    void *dst = mlu_dst[i];
    size_t size = interface.InputDataSize(i) * batch_size;

    // format data
    DataLayout cpu_layout = ploader_->GetCpuInputLayout(i);
    DataLayout mlu_layout = interface.GetMluInputLayout(i);
    Shape sp = ploader_->InputShapes()[i];
    void *temp_data = malloc(size);
    if (nullptr == temp_data) {
      throw MluMemoryOpError("Malloc temp data on cpu failed.");
    }
    TransLayout(cpu_layout, mlu_layout, src, temp_data, sp);
    error_code = cnrtMemcpy(dst, temp_data, size, CNRT_MEM_TRANS_DIR_HOST2DEV);
    CHECK_CNRT_RET("Memcpy host to device failed.", error_code);
    free(temp_data);
  }
#endif
}

void MluMemoryOp::MemcpyOutputD2H(void **cpu_dst, void **mlu_src, uint32_t batch_size) const {
  CHECK_MODEL_LOADER;
  ONLY_SUPPORT_FLOAT32_ON_CPU;
  ModelLoaderInternalInterface interface(ploader_.get());
  LOG(TRACE, "copy output memory from device to host");

#ifdef CNSTK_MLU100
  cnrtRet_t error_code = cnrtMemcpyBatchByDescArray(cpu_dst, mlu_src, interface.OutputDescArray(),
                                                    ploader_->OutputNum(), batch_size, CNRT_MEM_TRANS_DIR_DEV2HOST);
  CHECK_CNRT_RET("Memcpy device to host failed.", error_code);
#elif CNSTK_MLU270
  int64_t num = ploader_->OutputNum();
  for (int i = 0; i < num; ++i) {
    void *src = mlu_src[i];
    void *dst = cpu_dst[i];
    size_t size = interface.OutputDataSize(i) * batch_size;
    void *temp_data = malloc(size);
    if (nullptr == temp_data) {
      throw MluMemoryOpError("Malloc temp data on cpu failed.");
    }
    auto error_code = cnrtMemcpy(temp_data, src, size, CNRT_MEM_TRANS_DIR_DEV2HOST);
    CHECK_CNRT_RET("Memcpy device to host failed.", error_code);
    // format data
    DataLayout cpu_layout = ploader_->GetCpuOutputLayout(i);
    DataLayout mlu_layout = interface.GetMluOutputLayout(i);
    Shape sp = ploader_->OutputShapes()[i];
    TransLayout(mlu_layout, cpu_layout, temp_data, dst, sp);
    free(temp_data);
  }
#endif
}

void MluMemoryOp::MemcpyH2D(void *mlu_dst, void *cpu_src, size_t nBytes, uint32_t batch_size) const {
  cnrtRet_t error_code;
  LOG(TRACE, "copy memory from device to host");
#ifdef CNSTK_MLU100
  error_code = cnrtMemcpyBatch(mlu_dst, cpu_src, nBytes, batch_size, CNRT_MEM_TRANS_DIR_HOST2DEV);
  CHECK_CNRT_RET("Memcpy host to device failed.", error_code);
#elif CNSTK_MLU270
  error_code = cnrtMemcpy(mlu_dst, cpu_src, nBytes * batch_size, CNRT_MEM_TRANS_DIR_HOST2DEV);
  CHECK_CNRT_RET("Memcpy host to device failed.", error_code);
#endif
}

void MluMemoryOp::MemcpyD2H(void *cpu_dst, void *mlu_src, size_t nBytes, uint32_t batch_size) const {
  LOG(TRACE, "copy memory from host to device");
  cnrtRet_t error_code;
#ifdef CNSTK_MLU100
  error_code = cnrtMemcpyBatch(cpu_dst, mlu_src, nBytes, batch_size, CNRT_MEM_TRANS_DIR_DEV2HOST);
  CHECK_CNRT_RET("Memcpy device to host failed.", error_code);
#elif CNSTK_MLU270
  error_code = cnrtMemcpy(cpu_dst, mlu_src, nBytes * batch_size, CNRT_MEM_TRANS_DIR_DEV2HOST);
  CHECK_CNRT_RET("Memcpy host to device failed.", error_code);
#endif
}

void MluMemoryOp::MemcpyD2D(void *mlu_dst, void *mlu_src, size_t nBytes) const {
  LOG(TRACE, "copy memory from device to device");
  cnrtRet_t error_code;
#ifdef CNSTK_MLU100
  error_code = cnrtMemcpy(mlu_dst, mlu_src, nBytes, CNRT_MEM_TRANS_DIR_DEV2DEV);
  CHECK_CNRT_RET("Memcpy device to device failed.", error_code);
#elif CNSTK_MLU270
  error_code = cnrtMemcpy(mlu_dst, mlu_src, nBytes, CNRT_MEM_TRANS_DIR_DEV2DEV);
  CHECK_CNRT_RET("Memcpy device to device failed.", error_code);
#endif
}

}  // namespace edk
