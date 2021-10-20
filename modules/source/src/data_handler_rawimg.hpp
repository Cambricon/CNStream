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

#ifndef MODULES_SOURCE_HANDLER_RAWIMGMEM_HPP_
#define MODULES_SOURCE_HANDLER_RAWIMGMEM_HPP_

#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "cnstream_logging.hpp"
#include "data_handler_util.hpp"
#include "data_source.hpp"

namespace cnstream {

class RawImgMemHandlerImpl : public SourceRender {
 public:
  explicit RawImgMemHandlerImpl(DataSource *module, RawImgMemHandler *handler)  // NOLINT
      : SourceRender(handler), module_(module), handler_(*handler) {
    stream_id_ = handler_.GetStreamId();
  }

  ~RawImgMemHandlerImpl() {}

  bool Open();
  void Close();

  /**
   * @brief Sends raw image with cv::Mat. Only BGR data with 8UC3 type is supported, and data is continuous.
   * @param
         - mat_data: cv::Mat pointer with bgr24 format image data, feed mat_data as nullptr when feed data end.
         - pts: pts for image, should be different for each image
   * @retval 0: The data is write successfully,
   * @retval -1: Write failed, maybe eos got or handler is closed.
   * @retval -2: Invalid data.
   */
  int Write(const cv::Mat *mat_data, const uint64_t pts = 0);

  /**
   * @brief Sends raw image with image data and image infomation, support formats: bgr24, rgb24, nv21 and nv12.
   bgr24/rgb24/nv21/nv12 format).
   * @param
          - data: image data pointer(one continuous buffer), feed data as nullptr and size as 0 when feed data end
          - size: image data size
          - pts: pts for image, should be different for each image
          - width: image width
          - height: image height
          - pixel_fmt: image pixel format, support bgr24/rgb24/nv21/nv12 format.
   * @retval 0: The data is write successfully,
   * @retval -1: Write failed, maybe eos got or handler is closed.
   * @retval -2: Invalid data.
   */
  int Write(const uint8_t *data, const int size, const uint64_t pts = 0, const int width = 0, const int height = 0,
            const CNDataFormat pixel_fmt = CNDataFormat::CN_INVALID);

 private:
  DataSource *module_ = nullptr;
  RawImgMemHandler &handler_;
  std::string stream_id_;
  DataSourceParam param_;

#ifdef UNIT_TEST
 public:  // NOLINT
#else
 private:  // NOLINT
#endif
  bool CheckRawImageParams(const uint8_t *data, const int size, const int width,
      const int height, const CNDataFormat pixel_fmt);

  bool PrepareConvertCtx(const uint8_t *data, const int size, const int width,
      const int height, const CNDataFormat pixel_fmt);

  bool CvtColorWithStride(const uint8_t *data, const int size, const int width,
      const int height, const CNDataFormat pixel_fmt, uint8_t *dst_nv12, const int dst_stride);

  bool ProcessImage(const uint8_t *data, const int size, const int width,
      const int height, const CNDataFormat pixel_fmt, const uint64_t pts);

 private:
  std::atomic<bool> eos_got_{false};
  uint64_t frame_id_ = 0;

  cv::Mat *src_mat_ = nullptr;  // src mat with bgr24 or rgb24 format
  cv::Mat *dst_mat_ = nullptr;  // dst mat with I420 format

  int src_width_ = 0;
  int src_height_ = 0;
  CNDataFormat src_fmt_ = CNDataFormat::CN_INVALID;

#ifdef UNIT_TEST
 public:  // NOLINT
  void SetDecodeParam(const DataSourceParam &param) { param_ = param; }
#endif
};  // class RawImgMemHandlerImpl

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_RAWIMGMEM_HPP_
