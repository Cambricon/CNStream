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
#ifndef LIBSTREAM_INCLUDE_CNINFER_MLU_MEMORY_OP_H_
#define LIBSTREAM_INCLUDE_CNINFER_MLU_MEMORY_OP_H_

#include <memory>
#include "cnbase/cntypes.h"
#include "cnbase/streamlibs_error.h"
#include "cninfer/model_loader.h"

namespace libstream {

STREAMLIBS_REGISTER_EXCEPTION(MluMemoryOp);

class MluMemoryOp {
 public:
  MluMemoryOp();
  void set_loader(std::shared_ptr<ModelLoader> ploader);
  std::shared_ptr<ModelLoader> loader() const;
  /***********************************************************
   * @brief
   * @param
   *   batch_size[in]:
   * @return
   *   cpu data described by input_desc_array
   **********************************************************/
  void** alloc_mem_on_cpu_for_input(uint32_t batch_size) const;
  void** alloc_mem_on_cpu_for_output(uint32_t batch_size) const;
  void* alloc_mem_on_mlu(size_t nBytes, uint32_t batch_size) const;
  /***********************************************************
   * @brief
   * @param
   *   batch_size[in]:
   * @return
   *   mlu data described by input_desc_array for success
   *   throw an MluMemoryOp for failed
   **********************************************************/
  void** alloc_mem_on_mlu_for_input(uint32_t batch_size) const;
  void** alloc_mem_on_mlu_for_output(uint32_t batch_size) const;

  /**********************************************************
   * @brief
   * @param
   *   mem_num[in]: input num or output num
   **********************************************************/
  void free_input_mem_on_cpu(void **ptr) const;
  void free_output_mem_on_cpu(void **ptr) const;
  void free_mem_array_on_mlu(void **ptr, uint32_t mem_num) const;
  void free_mem_on_mlu(void *ptr) const;

  /**********************************************************
   * @brief throw an MluMemoryOp for failed
   * @param
   *   cpu_ptr[in]: data in cpu
   *   mlu_ptr[in]: buffer in mlu
   *   batch_size[in]:
   *   nBytes[in]: input
   **********************************************************/
  void memcpy_input_h2d(void **cpu_ptr, void **mlu_ptr,
                        uint32_t batch_size) const;
  void memcpy_output_d2h(void **mlu_ptr, void **cpu_ptr,
                         uint32_t batch_size) const;
  void memcpy_h2d(void *cpu_ptr, void *mlu_ptr,
                  size_t nBytes, uint32_t batch_size) const;
  void memcpy_d2h(void *mlu_ptr, void *cpu_ptr,
                  size_t nBytes, uint32_t batch_size) const;

 private:
  std::shared_ptr<ModelLoader> ploader_;
};  // class MluMemoryOp

}  // namespace libstream

#endif  // LIBSTREAM_INCLUDE_CNINFER_MLU_MEMORY_OP_H_
