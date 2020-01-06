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

/**
 * @file mlu_memory_op.h
 *
 * This file contains a declaration of the MluMemoryOp class.
 */

#ifndef EASYINFER_MLU_MEMORY_OP_H_
#define EASYINFER_MLU_MEMORY_OP_H_

#include <memory>
#include "cxxutil/exception.h"
#include "easyinfer/model_loader.h"

namespace edk {

TOOLKIT_REGISTER_EXCEPTION(MluMemoryOp);

/**
 * @brief MluMemoryOp is a MLU memory helper class.
 * @note It provides a easy way to manage memory on MLU.
 */
class MluMemoryOp {
 public:
  /**
   * @brief Construct a new Mlu Memory Op object
   */
  MluMemoryOp();

  /**
   * @brief Set ModelLoader
   *
   * @note model loader is used for manage model's input and output memory easily,
   *       do not need to set if this feature is not used.
   * @param ploader[in] Model loader
   * @see ModelLoader
   */
  void SetLoader(std::shared_ptr<ModelLoader> ploader);

  /**
   * @brief Get model loader
   *
   * @return Model loader
   */
  std::shared_ptr<ModelLoader> Loader() const;

  /**
   * @brief Alloc memory on CPU for model input
   *
   * @note Input data shape is described by input_shapes from ModelLoader
   * @attention Ensure SetLoader has been called once
   * @param batch_size[in]: batch size
   * @return Alloced CPU memory
   */
  void **AllocCpuInput(uint32_t batch_size) const;

  /**
   * @brief Alloc memory on CPU for model output
   *
   * @note Output data shape is described by output_shapes from ModelLoader
   * @attention Ensure SetLoader has been called once
   * @param batch_size[in]: batch size
   * @return Alloced CPU memory
   */
  void **AllocCpuOutput(uint32_t batch_size) const;

  /**
   * @brief Alloc memory on MLU according to nBytes.
   *
   * @param nBytes Alloced memory size in bytes
   * @param batch_size Batch size
   * @return Alloced MLU memory
   */
  void *AllocMlu(size_t nBytes, uint32_t batch_size) const;

  /**
   * @brief Alloc memory on MLU for model input
   *
   * @note Input data shape is described by input_data_descs from ModelLoader
   * @attention Ensure SetLoader has been called once
   * @param batch_size[in]: Batch size
   * @return Alloced MLU memory
   */
  void **AllocMluInput(uint32_t batch_size) const;

  /**
   * @brief Alloc memory on MLU for model output
   *
   * @note Input data shape is described by output_data_descs from ModelLoader
   * @attention Ensure SetLoader has been called once
   * @param batch_size[in]: Batch size
   * @return Alloced MLU memory
   */
  void **AllocMluOutput(uint32_t batch_size) const;

  /**
   * @brief Free input memory on CPU.
   *
   * @attention Ensure SetLoader has been called once
   * @param ptr[in] CPU memory pointer
   */
  void FreeCpuInput(void **ptr) const;

  /**
   * @brief Free output memory on CPU.
   *
   * @attention Ensure SetLoader has been called once
   * @param ptr[in] CPU memory pointer
   */
  void FreeCpuOutput(void **ptr) const;

  /**
   * @brief Free memory array on MLU
   *
   * @param ptr[in] Memory array on MLU
   * @param mem_num[in]: Memory number, usually input_num or output_num got from ModelLoader
   */
  void FreeArrayMlu(void **ptr, uint32_t mem_num) const;

  /**
   * @brief Free memory on MLU
   *
   * @param ptr[in] MLU memory pointer
   */
  void FreeMlu(void *ptr) const;

  /**
   * @brief Copy model input data, from host(CPU) to device(MLU)
   *
   * @attention Ensure SetLoader has been called once
   * @param mlu_dst[in] Copy destination, memory on MLU
   * @param cpu_src[in] Copy source, data on CPU
   * @param batch_size[in] Batch size
   */
  void MemcpyInputH2D(void **mlu_dst, void **cpu_src, uint32_t batch_size) const;

  /**
   * @brief Copy model output data, from device to host
   *
   * @attention Ensure SetLoader has been called once
   * @param cpu_dst[in] Copy destination, memory on CPU
   * @param mlu_src[in] Copy source, data on MLU
   * @param batch_size[in] Batch size
   */
  void MemcpyOutputD2H(void **cpu_dst, void **mlu_src, uint32_t batch_size) const;

  /**
   * @brief Copy data from host to device
   *
   * @param mlu_dst[in] Copy destination, memory on MLU
   * @param cpu_src[in] Copy source, data on CPU
   * @param nBytes[in] Memory size in bytes
   * @param batch_size[in] Batch size
   */
  void MemcpyH2D(void *mlu_dst, void *cpu_src, size_t nBytes, uint32_t batch_size) const;

  /**
   * @brief Copy data from device to host
   *
   * @param cpu_dst[in] Copy destination, memory on CPU
   * @param mlu_src[in] Copy source, data on MLU
   * @param nBytes[in] Memory size in bytes
   * @param batch_size[in] Batch size
   */
  void MemcpyD2H(void *cpu_dst, void *mlu_src, size_t nBytes, uint32_t batch_size) const;

  /**
   * @brief Copy data from device to device
   *
   * @param mlu_dst[in] Copy destination, memory on MLU
   * @param mlu_src[in] Copy source, data on MLU
   * @param nBytes[in] Memory size in bytes
   */
  void MemcpyD2D(void *mlu_dst, void *mlu_src, size_t nBytes) const;

 private:
  std::shared_ptr<ModelLoader> ploader_;
};  // class MluMemoryOp

}  // namespace edk

#endif  // EASYINFER_MLU_MEMORY_OP_H_
