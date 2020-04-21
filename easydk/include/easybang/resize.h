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
 * @file resize.h
 *
 * This file contains a declaration of the MluResize class.
 */

#ifndef EASYBANG_RESIZE_H_
#define EASYBANG_RESIZE_H_

#include <string>
#include "cxxutil/exception.h"
#include "easyinfer/easy_infer.h"

struct ResizeKernelParam;

namespace edk {

TOOLKIT_REGISTER_EXCEPTION(MluResize);

class MluResizePrivate;

/**
 * @brief Mlu resize operator helper class
 */
class MluResize {
 public:
  /**
   * @brief Construct a new Mlu Resize object
   */
  MluResize();
  /**
   * @brief Destroy the Mlu Resize object
   */
  ~MluResize();

  /**
   * @brief Params to initialize resize operator
   */
  struct Attr {
    /// Input image resolution
    uint32_t src_w, src_h;
    /// Output image resolution
    uint32_t dst_w, dst_h;
    /// Kernel batch size
    int batch_size = 1;
    int core = 4;
    uint32_t channel_id = 0;
  };

  /**
   * @brief Set the mlu task queue
   *
   * @param queue[in] Shared mlu task queue on which run kernel
   */
  void SetMluQueue(MluTaskQueue_t queue);

  /**
   * @brief Get the mlu task queue
   *
   * @return MluTaskQueue_t
   */
  MluTaskQueue_t GetMluQueue() const;

  /**
   * @brief Initialize operator
   *
   * @param attr[in] Params to initialize operator
   */
  bool Init(const Attr& attr);

  /**
   * @brief Get operator attribute
   *
   * @return attribute
   */
  const Attr& GetAttr();

  /**
   * @brief Deinitialize resources
   */
  void Destroy();

  /**
   * @brief Get the last error string while get an false or -1 from InvokeOp or SyncOneOutput
   *
   * @return Last error message
   */
  std::string GetLastError() const;

  /**
   * @brief Batching up one yuv image
   *
   * @param src_y[in] input y plane in MLU memory
   * @param src_uv[in] input uv plane in MLU memory
   */
  void BatchingUp(void* src_y, void* src_uv);

  /**
   * @brief Execute Operator and return an operator output (a whole batch)
   *
   * @param dst[out] Operator output MLU memory, containing a whole batch
   * @return Return false if execute failed
   */
  bool SyncOneOutput(void* dst);

 private:
  MluResizePrivate* d_ptr_ = nullptr;

  MluResize(const MluResize&) = delete;
  MluResize& operator=(const MluResize&) = delete;
};  // class MluResize

}  // namespace edk

#endif  // EASYBANG_RESIZE_H_
