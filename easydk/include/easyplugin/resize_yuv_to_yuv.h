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

/**
 * @file resize_yuv_to_yuv.h
 *
 * This file contains a declaration of the MluResizeYuv2Yuv class.
 */

#ifndef EASYBANG_RESIZE_YUV_TO_YUV_H_
#define EASYBANG_RESIZE_YUV_TO_YUV_H_

#include <string>
#include "cxxutil/exception.h"
#include "easyplugin/resize_common.h"

struct ResizeYuv2Yuv;

namespace edk {

TOOLKIT_REGISTER_EXCEPTION(MluResizeYuv2Yuv);

class MluResizeYuv2YuvPrivate;

/**
 * @brief Mlu Resize Yuv to Yuv operator helper class
 */
class MluResizeYuv2Yuv {
 public:
  /**
   * @brief Construct a new Mlu Resize Yuv to Yuv Operator object
   */
  MluResizeYuv2Yuv();
  /**
   * @brief Construct a new Mlu Resize Yuv to Yuv Operator object and init with attr
   *
   * @param attr[in] Params for resize operator
   */
  explicit MluResizeYuv2Yuv(const MluResizeAttr& attr);
  /**
   * @brief Destroy the Mlu Resize Yuv to Yuv Operator object
   */
  ~MluResizeYuv2Yuv();

  /**
   * @brief Set the mlu task queue
   *
   * @param queue[in] mlu task queue on which run kernel
   * @param exclusive[in] mlu task queue is exclusive. Therefore it could be destroied.
   */
  void SetMluQueue(cnrtQueue_t queue, bool exclusive = false);

  /**
   * @brief Destroy the mlu task queue
   */
  void DestroyMluQueue();

  /**
   * @brief Get the mlu task queue
   *
   * @return cnrtQueue_t
   */
  cnrtQueue_t GetMluQueue() const;

  /**
   * @brief Initialize operator
   *
   * @param attr[in] Params to initialize operator
   */
  bool Init(const MluResizeAttr& attr);

  /**
   * @brief Get operator attribute
   *
   * @return attribute
   */
  const MluResizeAttr& GetAttr();

  /**
   * @brief Excute operator, use BatchingUp and SyncOneOutput instead
   * @deprecated
   *
   * @param dst_y[out] Operator output y plane in MLU memory
   * @param dst_uv[out] Operator output uv plane in MLU memory
   * @param src_y[in] Operator input y plane in MLU memory
   * @param src_uv[in] Operator input uv plane in MLU memory
   * @return Return 0 if invoke succeeded, otherwise return -1
   */
  int InvokeOp(void* dst_y, void* dst_uv, void* src_y, void* src_uv);

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
   * @brief Batching up one source yuv image
   *
   * @param y[in] y plane in MLU memory
   * @param uv[in] uv plane in MLU memory
   */
  void SrcBatchingUp(void* y, void* uv);

  /**
   * @brief Batching up dst yuv address
   *
   * @param y[in] y plane in MLU memory
   * @param uv[in] uv plane in MLU memory
   */
  void DstBatchingUp(void* y, void* uv);

  /**
   * @brief Execute Operator and return an operator output (a whole batch)
   *
   * @return Return false if execute failed
   */
  bool SyncOneOutput();

 private:
  MluResizeYuv2YuvPrivate* d_ptr_ = nullptr;

  MluResizeYuv2Yuv(const MluResizeYuv2Yuv&) = delete;
  MluResizeYuv2Yuv& operator=(const MluResizeYuv2Yuv&) = delete;
};  // class MluResizeYuv2Yuv

}  // namespace edk

#endif  // EASYBANG_RESIZE_YUV_TO_YUV_H_
