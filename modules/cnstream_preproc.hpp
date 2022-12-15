/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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

#ifndef CNSTREAM_INFERENCE_PREPROC_HPP_
#define CNSTREAM_INFERENCE_PREPROC_HPP_

/**
 *  \file preproc.hpp
 *
 *  This file contains a declaration of class Preproc
 */
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "cnedk_buf_surface.h"
#include "cnedk_transform.h"
#include "cnis/processor.h"
#include "reflex_object.h"

namespace cnstream {
/**
 * @struct CnPreprocNetworkInfo
 *
 * @brief The CnPreprocNetworkInfo is a structure describing the network input information.
 */
typedef struct CnPreprocNetworkInfo {
  uint32_t n;  /*!< The network input shape in the N dimension. */
  uint32_t h;  /*!< The network input shape in the H dimension. */
  uint32_t w;  /*!< The network input shape in the W dimension. */
  uint32_t c;  /*!< The network input shape in the C dimension. */
  infer_server::DataType dtype;  /*!< The network input data type. */
  infer_server::NetworkInputFormat format;  /*!< The network input pixel format. */
} CnPreprocNetworkInfo;

/**
 * @class Preproc
 *
 * @brief Preproc is the base class of network preprocessing for inference module.
 */
class Preproc : virtual public ReflexObjectEx<Preproc> {
 public:
  /**
   * @brief Destructs an object.
   *
   * @return No return value.
   */
  virtual ~Preproc() {}
  /**
   * @brief Creates a preprocess object with the given preprocess's class name.
   *
   * @param[in] proc_name The preprocess class name.
   *
   * @return Returns the pointer to preprocess object.
   */
  static Preproc *Create(const std::string &proc_name);

  /**
   * @brief Initializes preprocessing parameters.
   *
   * @param[in] params The preprocessing parameters.
   *
   * @return Returns 0 for success, otherwise returns <0.
   **/
  virtual int Init(const std::unordered_map<std::string, std::string>& params) { return 0; }
  /**
   * @brief Parses network parameters.
   *
   * @param[in] params The network parameters.
   *
   * @return Returns 0 for success, otherwise returns <0.
   **/
  virtual int OnTensorParams(const infer_server::CnPreprocTensorParams *params) = 0;
  /**
   * @brief Executes preprocess.
   *
   * @param[in] src  The input buffer.
   * @param[out] dst  The output buffer.
   * @param[in] src_rects The interested region of the input, which is valid in secondary inference.
   *
   * @return Returns 0 if successful, otherwise returns -1.
   */
  virtual int Execute(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
                      const std::vector<CnedkTransformRect> &src_rects) = 0;

 public:
  bool hw_accel_ = false;  /*!< Sets whether to use hardware to accelerate the preprocessing. */
};  // class Preproc

/**
 * @brief Gets the coordinates of the valid area of the output while keeping the aspect ratio.
 *
 * @param[in] src_w The width of the source.
 * @param[in] src_h The height of the source.
 * @param[in] dst_w The width of the destination.
 * @param[in] dst_h The height of the destination.
 *
 * @return Returns the valid area coordinates.
 */
CnedkTransformRect KeepAspectRatio(int src_w, int src_h, int dst_w, int dst_h);
/**
 * @brief Gets the information of the network.
 *
 * @param[in] params The parameters of the network.
 * @param[out] info The information of the network.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int GetNetworkInfo(const infer_server::CnPreprocTensorParams *params, CnPreprocNetworkInfo *info);

/**
 * @brief Converts image from YUV420sp NV12 format to BGR format.
 *
 * @param[in] src_y The y plane pointer of the source.
 * @param[in] src_uv The uv plane pointer of the source.
 * @param[in] src_w The width of the source.
 * @param[in] src_h The height of the source.
 * @param[in] src_stride The stride of the source.
 * @param[out] dst_bgr24 The data pointer of the destination.
 * @param[in] dst_w The width of the destination.
 * @param[in] dst_h The height of the destination.
 * @param[in] dst_stride The stride of the destination.
 *
 * @return No return value.
 */
void NV12ToBGR24(uint8_t *src_y, uint8_t *src_uv, int src_w, int src_h, int src_stride, uint8_t *dst_bgr24, int dst_w,
                 int dst_h, int dst_stride);

/**
 * @brief Converts image from YUV420sp NV21 format to BGR format.
 *
 * @param[in] src_y The y plane pointer of the source.
 * @param[in] src_uv The uv plane pointer of the source.
 * @param[in] src_w The width of the source.
 * @param[in] src_h The height of the source.
 * @param[in] src_stride The stride of the source.
 * @param[out] dst_bgr24 The data pointer of the destination.
 * @param[in] dst_w The width of the destination.
 * @param[in] dst_h The height of the destination.
 * @param[in] dst_stride The stride of the destination.
 *
 * @return No return value.
 */
void NV21ToBGR24(uint8_t *src_y, uint8_t *src_uv, int src_w, int src_h, int src_stride, uint8_t *dst_bgr24, int dst_w,
                 int dst_h, int dst_stride);
/**
 * @brief Converts image from YUV420sp NV12/NV21 format to RGBx (RGB/BGR/RGBA/ARGB/BGRA/ABGR) format.
 *
 * @param[in] src_y The y plane pointer of the source.
 * @param[in] src_uv The uv plane pointer of the source.
 * @param[in] src_w The width of the source.
 * @param[in] src_h The height of the source.
 * @param[in] src_y_stride The stride of y plane of the source.
 * @param[in] src_uv_stride The stride of uv plane of the source.
 * @param[in] src_fmt The pixel format of the source.
 * @param[out] dst_rgbx The data pointer of the destination.
 * @param[in] dst_w The width of the destination.
 * @param[in] dst_h The height of the destination.
 * @param[in] dst_stride The stride of the destination.
 * @param[in] dst_fmt The pixel format of the destination.
 *
 * @return No return value.
 */
void YUV420spToRGBx(uint8_t *src_y, uint8_t *src_uv, int src_w, int src_h, int src_y_stride, int srv_uv_stride,
                    CnedkBufSurfaceColorFormat src_fmt, uint8_t *dst_rgbx, int dst_w, int dst_h, int dst_stride,
                    CnedkBufSurfaceColorFormat dst_fmt);
}  // namespace cnstream

#endif  // CNSTREAM_INFERENCE_PREPROC_HPP_
