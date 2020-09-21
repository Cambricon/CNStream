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

#ifndef MODULES_IMAGE_PREPROC_HPP_
#define MODULES_IMAGE_PREPROC_HPP_

#include <glog/logging.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>  // av_image_alloc
#include <libswscale/swscale.h>
}

#ifdef HAVE_OPENCV
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#else
#error OpenCV required
#endif

#include <string>

#include "common.hpp"
#include "encode.hpp"

namespace cnstream {

class ImagePreproc {
 public:
  struct ImagePreprocParam {
    uint32_t src_height = 0;
    uint32_t src_width = 0;
    uint32_t src_stride = 0;
    uint32_t dst_height = 0;
    uint32_t dst_width = 0;
    uint32_t dst_stride = 0;
    CNPixelFormat src_pix_fmt = BGR24;
    CNPixelFormat dst_pix_fmt = BGR24;
    std::string preproc_type = "cpu";
    bool use_ffmpeg = false;
    int device_id = -1;
  };

  explicit ImagePreproc(ImagePreprocParam param);
  virtual ~ImagePreproc();
  bool Init();
  bool SetSrcWidthHeight(uint32_t width, uint32_t height, uint32_t stride = 0);
  bool Bgr2Bgr(const cv::Mat &src_image, cv::Mat dst_image);
  bool Bgr2Yuv(const cv::Mat &src_image, uint8_t *dst);
  bool Bgr2Yuv(const cv::Mat &src_image, uint8_t *dst_y, uint8_t *dst_uv);
  bool Yuv2Yuv(const uint8_t *src_y, const uint8_t *src_uv, uint8_t *dst);
  bool Yuv2Yuv(const uint8_t *src_y, const uint8_t *src_uv, uint8_t *dst_y, uint8_t *dst_uv);

  bool ResizeYuvNearest(const uint8_t *src, uint8_t *dst);
  bool Bgr2YUV420NV(const cv::Mat &bgr, uint8_t *nv_data);
  bool ConvertWithFFmpeg(const uint8_t *src_buffer, const size_t src_buffer_size, uint8_t *dst_buffer,
                         const size_t dst_buffer_size);

 private:
  bool InitForFFmpeg();

  ImagePreprocParam preproc_param_;
  bool is_init_ = false;
  uint32_t src_align_ = 1;
  uint32_t dst_align_ = 1;
  SwsContext *swsctx_ = nullptr;
  AVFrame *src_pic_ = nullptr;
  AVFrame *dst_pic_ = nullptr;
  AVPixelFormat src_pix_fmt_ = AV_PIX_FMT_NONE;  // AV_PIX_FMT_BGR24
  AVPixelFormat dst_pix_fmt_ = AV_PIX_FMT_NONE;
};  // class ImagePreproc

}  // namespace cnstream

#endif  // MODULES_IMAGE_PREPROC_HPP_
