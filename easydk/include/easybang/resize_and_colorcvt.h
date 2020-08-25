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
 * @file resize_and_colorcvt.h
 *
 * This file contains a declaration of the MluResizeConvertOp class.
 */

#ifndef EASYBANG_RESIZE_AND_CONVERT_H_
#define EASYBANG_RESIZE_AND_CONVERT_H_

#include <algorithm>
#include <ostream>
#include <string>
#include <vector>
#include "device/mlu_context.h"
#include "cxxutil/edk_attribute.h"
#include "cxxutil/exception.h"
#include "easyinfer/easy_infer.h"

struct KernelParam;

namespace edk {

class MluResizeConvertPrivate;

/**
 * @brief Mlu resize and convert operator helper class
 */
class MluResizeConvertOp {
 public:
  static constexpr float MAXIMUM_SCALE_UP_FACTOR = 100.0f;  ///<  Maximum magnification supported by operator
  static constexpr uint32_t MAXIMUM_WIDTH = 7680;  ///< Maximum input width supported by operator
  /**
   * @brief Construct a new Mlu Resize Convert Operator object
   */
  MluResizeConvertOp();
  /**
   * @brief Destroy the Mlu Resize Convert Operator object
   */
  ~MluResizeConvertOp();

  /**
   * @brief Enumeration to specify color convert mode
   */
  enum class ColorMode {
    RGBA2RGBA = 0,      ///< Convert color from RGBA to RGBA
    YUV2RGBA_NV12 = 1,  ///< Convert color from NV12 to RGBA
    YUV2RGBA_NV21 = 2,  ///< Convert color from NV21 to RGBA
    YUV2BGRA_NV12 = 3,  ///< Convert color from NV12 to BGRA
    YUV2BGRA_NV21 = 4,  ///< Convert color from NV21 to BGRA
    YUV2ARGB_NV12 = 5,  ///< Convert color from NV12 to ARGB
    YUV2ARGB_NV21 = 6,  ///< Convert color from NV21 to ARGB
    YUV2ABGR_NV12 = 7,  ///< Convert color from NV12 to ABGR
    YUV2ABGR_NV21 = 8   ///< Convert color from NV21 to ABGR
  };                    // enum Mode

  /**
   * @brief Enumeration to specify date transform mode
   */
  enum class DataMode {
    FP16ToFP16 = 0,   ///< Transform data type from float16 to float16
    FP16ToUINT8 = 1,  ///< Transform data type from float16 to uint8
    UINT8ToFP16 = 2,  ///< Transform data type from uint8 to float16
    UINT8ToUINT8 = 3  ///< Transform data type from uint8 to uint8
  };                  // enum DataMode

  /**
   * @brief Params to initialize resize and color convert operator
   */
  struct Attr {
    /// Color convert mode
    ColorMode color_mode = ColorMode::YUV2RGBA_NV21;
    /// Data type transform mode
    DataMode data_mode = DataMode::UINT8ToUINT8;
    /// Input image resolution @deprecated set input image resolution in InputData
    uint32_t src_w = 0, src_h = 0, src_stride = 0;
    /// Output image resolution
    uint32_t dst_w, dst_h;
    /// Crop rectangle (top-left coordinate, width, height) @deprecated set crop rectangle in InputData
    uint32_t crop_x = 0, crop_y = 0, crop_w = 0, crop_h = 0;
    /// Kernel batch size
    int batch_size = 1;
    /// device id
    CoreVersion core_version = CoreVersion::MLU270;
    /// keep aspec ratio
    bool keep_aspect_ratio = false;
    /// the number of ipu cores used per execution.
    /// When this value is 0, the number of cores used per execution is equal to batch_size
    int core_number = 0;
    /// A flag indicates the pad method, 0:padding is on both sides, 1: padding is on the right or
    /// bottom side.
    int padMethod = 0;
  };

  /**
   * @brief Input data struct
   **/
  struct InputData {
    /// Input image resolution
    uint32_t src_w, src_h, src_stride;
    /// Crop rectangle
    uint32_t crop_x = 0, crop_y = 0, crop_w = 0, crop_h = 0;
    /// image data, support YUV420SP NV21/NV12
    void* planes[2];
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
   * @brief Check if ResizeConvertOp shared MluTaskQueue with other task
   *
   * @return true if queue is shared, false otherwise
   */
  bool IsSharedQueue() const;

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
   * @brief Excute operator, use BatchingUp and SyncOneOutput instead
   * @deprecated
   *
   * @param dst[out] Operator output MLU memory
   * @param src_y[in] Operator input y plane in MLU memory
   * @param src_uv[in] Operator input uv plane in MLU memory
   * @return Return 0 if invoke succeeded, otherwise return -1
   */
  attribute_deprecated int InvokeOp(void* dst, void* src_y, void* src_uv);

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
   * @deprecated Use void BatchingUp(InputData) instead
   * @brief Batching up one yuv image
   *
   * @param src_y[in] input y plane in MLU memory
   * @param src_uv[in] input uv plane in MLU memory
   *
   * @attention image size set when Init called. support YUV420SP NV21/NV12
   */
  attribute_deprecated void BatchingUp(void* src_y, void* src_uv);

  /**
   * @brief Batching up one yuv image
   *
   * @param input_data[in] yuv data (YUV420SP NV21/NV12)
   *
   * @attention input_data.crop_w will be set to input_data.src_w when input_data.crop_w is zero.
   *            input_data.crop_h will be sett to input_data.src_h when input_data.crop_h is zero.
   *            RCOpScaleUpError will be thrown when scale-up factor is greater than `MAXIMUM_SCALE_UP_FACTOR`.
   *            RCOpWidthOverLimitError will be thrown when input_data.crop_w is greater than `MAXIMUM_INPUT_WIDTH`.
   **/
  void BatchingUp(const InputData& input_data);

  /**
   * @brief Execute Operator and return an operator output (a whole batch)
   *
   * @param dst[out] Operator output MLU memory, containing a whole batch
   * @return Return false if execute failed
   */
  bool SyncOneOutput(void* dst);

  /**
   * @brief Get informations about last batch of input data
   *
   * @return Return last batch of input data
   **/
  std::vector<InputData> GetLastBatchInput() const;

 private:
  MluResizeConvertPrivate* d_ptr_ = nullptr;

  MluResizeConvertOp(const MluResizeConvertOp&) = delete;
  MluResizeConvertOp& operator=(const MluResizeConvertOp&) = delete;
};  // class MluResizeConvertOp

/**
 * @brief Exception throwed by MluResizeConvertOp
 *
 * @see MluResizeConvertOp
 **/
class MluResizeConvertOpError : public Exception {
 public:
  using Attr = MluResizeConvertOp::Attr;
  using InputData = MluResizeConvertOp::InputData;

  MluResizeConvertOpError(const Attr& attr,
                                   const InputData& input_data)
      : Exception("Mlu resize convert error."), attr_(attr), data_(input_data) {}

  MluResizeConvertOpError(const std::string& err_str, const Attr& attr,
                          const InputData& input_data)
      : Exception(err_str), err_str_(err_str), attr_(attr), data_(input_data) {}

  /**
   * @brief Get operator attribute
   *
   * @return attribute
   **/
  const Attr& GetRCOpAttr() const { return attr_; }

  /**
   * @brief Get input data
   *
   * @return input data which caused scale-up error
   **/
  const InputData& GetInputData() const { return data_; }

  /**
   * @brief Returns the explanatory string
   *
   * @return Returns the explanatory string
   **/
  const char* what() const noexcept override { return err_str_.c_str(); }

 protected:
  std::string err_str_ = "";

 private:
  Attr attr_;
  InputData data_;
};  // class MluResizeConvertOpError

/**
 * @brief Exception class for scale-up
 *
 * @see MluResizeConvertOp::BatchingUp(const InputData&)
 **/
class RCOpScaleUpError : public MluResizeConvertOpError {
 public:
  explicit RCOpScaleUpError(const Attr& attr, const InputData& input_data) :
      MluResizeConvertOpError(attr, input_data) {
    err_str_ = "Maximum magnification limit exceeded. Maximum magnification: "
               + std::to_string(MluResizeConvertOp::MAXIMUM_SCALE_UP_FACTOR) + ". Current magnification: "
               + std::to_string(ScaleUpFactor()) + ".";
  }

  /**
   * @brief Get scale-up factor
   **/
  inline float ScaleUpFactor() const {
    float scale_w = 1.0f * GetRCOpAttr().dst_w / GetInputData().crop_w;
    float scale_h = 1.0f * GetRCOpAttr().dst_h / GetInputData().crop_h;
    if (GetRCOpAttr().keep_aspect_ratio) {
      return std::min(scale_w, scale_h);
    }
    return scale_w;
  }
};  // class ScaleUpError

/**
 * @brief Exception class for the width of the input image
 *        exceeds the maximum width supported by the operator
 *
 * @see MluResizeConvertOp::BatchingUp(const InputData&)
 **/
class RCOpWidthOverLimitError : public MluResizeConvertOpError {
 public:
  explicit RCOpWidthOverLimitError(const Attr& attr, const InputData& input_data) :
      MluResizeConvertOpError(attr, input_data) {
    err_str_ = "Maximum input width limit exceeded. Maximum input width: "
               + std::to_string(MluResizeConvertOp::MAXIMUM_WIDTH) + ". Current input width: "
               + std::to_string(GetWidth()) + ".";
  }

  inline uint32_t GetWidth() const { return GetInputData().crop_w; }
};  // class WidthOverLimitError

}  // namespace edk

std::ostream& operator<<(std::ostream& os, const edk::MluResizeConvertOp::InputData& data);

#endif  // EASYBANG_RESIZE_AND_CONVERT_H_
