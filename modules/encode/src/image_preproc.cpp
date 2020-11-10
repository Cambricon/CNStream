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

#include "image_preproc.hpp"

#include <glog/logging.h>

#include <string>

#include "cnencode.hpp"
#include "device/mlu_context.h"

namespace cnstream {

ImagePreproc::ImagePreproc(ImagePreprocParam param) {
  preproc_param_ = param;
}

bool ImagePreproc::Init() {
  if (is_init_) {
    LOG(ERROR) << "[ImagePreproc] Init function should be called only once.";
    return false;
  }
  if (preproc_param_.dst_stride == 0) preproc_param_.dst_stride = preproc_param_.dst_width;
  if (preproc_param_.src_stride == 0) preproc_param_.src_stride = preproc_param_.src_width;
  if (preproc_param_.dst_stride != preproc_param_.dst_width) {
    dst_align_ = JPEG_ENC_ALIGNMENT;
  }
  if (preproc_param_.src_stride != preproc_param_.src_width) {
    src_align_ = DEC_ALIGNMENT;
  }

  if (preproc_param_.preproc_type == "mlu") {
    if (preproc_param_.src_pix_fmt == NV12 || preproc_param_.src_pix_fmt == NV21) {
      if (preproc_param_.device_id >= 0) {
        edk::MluContext context;
        try {
          context.SetDeviceId(preproc_param_.device_id);
          context.BindDevice();
        } catch (edk::Exception &err) {
          LOG(ERROR) << "[ImagePreproc] set mlu env failed.";
          return false;
        }
      } else {
        LOG(ERROR) << "[ImagePreproc] device id is invalid.";
        return false;
      }
      // do something to init a mlu resize yuv op
      LOG(ERROR) << "[ImagePreproc] mlu preproc is not supported yet.";
      return false;
    } else {
      LOG(ERROR) << "[ImagePreproc] mlu preproc only support yuv2yuv resize.";
      return false;
    }
  } else if (preproc_param_.use_ffmpeg) {
    if (!InitForFFmpeg()) {
      LOG(ERROR) << "[ImagePreproc] Init ffmpeg failed.";
      return false;
    }
  }
  is_init_ = true;
  return true;
}

bool ImagePreproc::InitForFFmpeg() {
  switch (preproc_param_.src_pix_fmt) {
    case BGR24: src_pix_fmt_ = AV_PIX_FMT_BGR24; break;
    case RGB24: src_pix_fmt_ = AV_PIX_FMT_RGB24; break;
    case NV12: src_pix_fmt_ = AV_PIX_FMT_NV12; break;
    case NV21: src_pix_fmt_ = AV_PIX_FMT_NV21; break;
    default:
      LOG(ERROR) << "[ImagePreproc] Only support source with bgr24/rgb24/nv21/nv12 format";
      return false;
  }
  switch (preproc_param_.dst_pix_fmt) {
    case BGR24: dst_pix_fmt_ = AV_PIX_FMT_BGR24; break;
    case RGB24: dst_pix_fmt_ = AV_PIX_FMT_RGB24; break;
    case NV12: dst_pix_fmt_ = AV_PIX_FMT_NV12; break;
    case NV21: dst_pix_fmt_ = AV_PIX_FMT_NV21; break;
    default:
      LOG(ERROR) << "[ImagePreproc] Only support destination with bgr24/rgb24/nv21/nv12 format";
      return false;
  }

  src_pic_ = av_frame_alloc();
  dst_pic_ = av_frame_alloc();
  if (src_pic_ == nullptr || dst_pic_ == nullptr) {
    LOG(ERROR) << "[ImagePreproc] Failed allocating AVFrame for the src_pic/dst_pic";
    return false;
  }
  swsctx_ = sws_getContext(preproc_param_.src_width, preproc_param_.src_height, src_pix_fmt_,
                           preproc_param_.dst_width, preproc_param_.dst_height, dst_pix_fmt_,
                           SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
  if (swsctx_ == nullptr) {
    LOG(ERROR) << "[ImagePreproc] sws_getContext failed.";
    if (src_pic_) {
      av_frame_free(&src_pic_);
      src_pic_ = nullptr;
    }
    if (dst_pic_) {
      av_frame_free(&dst_pic_);
      dst_pic_ = nullptr;
    }
    return false;
  }
  return true;
}

bool ImagePreproc::SetSrcWidthHeight(uint32_t width, uint32_t height, uint32_t stride) {
  if (width == 0 || height == 0) {
    LOG(ERROR) << "[ImagePreproc] src h or src w is 0.";
    return false;
  }
  if (preproc_param_.src_width != width || preproc_param_.src_height != height) {
    preproc_param_.src_width = width;
    preproc_param_.src_height = height;
    if (preproc_param_.use_ffmpeg) {
      if (swsctx_) {
        sws_freeContext(swsctx_);
        SwsContext* ctx = sws_getContext(preproc_param_.src_width, preproc_param_.src_height, src_pix_fmt_,
                                         preproc_param_.dst_width, preproc_param_.dst_height, dst_pix_fmt_,
                                         SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        if (ctx == nullptr) {
          LOG(ERROR) << "[ImagePreproc] ffmpeg sws get context failed.";
          return false;
        }
        swsctx_ = ctx;
      } else {
        LOG(ERROR) << "[ImagePreproc] please init first";
        return false;
      }
    }
  }
  if (stride == 0) {
    stride = width;
  }
  if (stride != width) {
    src_align_ = DEC_ALIGNMENT;
  } else {
    src_align_ = 1;
  }

  if (preproc_param_.src_stride != stride) {
    preproc_param_.src_stride = stride;
  }
  return true;
}

ImagePreproc::~ImagePreproc() {
  if (src_pic_) {
    av_frame_free(&src_pic_);
    src_pic_ = nullptr;
  }
  if (dst_pic_) {
    av_frame_free(&dst_pic_);
    dst_pic_ = nullptr;
  }
  if (swsctx_) {
    sws_freeContext(swsctx_);
    swsctx_ = nullptr;
  }
}

// bgr to bgr opencv/ffmpeg
bool ImagePreproc::Bgr2Bgr(const cv::Mat &src_image, cv::Mat dst_image) {
  if (src_image.cols * src_image.rows == 0 || dst_image.cols * dst_image.rows == 0) {
    LOG(ERROR) << "[ImagePreproc] src image or dst image has invalid width or height.";
    return false;
  }
  bool ret = true;
  if (preproc_param_.use_ffmpeg) {
    uint32_t input_buf_size = src_image.cols * src_image.rows * 3;
    uint32_t output_buf_size = preproc_param_.dst_width * preproc_param_.dst_height * 3;
    ret = ConvertWithFFmpeg(src_image.data, input_buf_size, dst_image.data, output_buf_size);
  } else {
    cv::resize(src_image, dst_image,
        cv::Size(preproc_param_.dst_width, preproc_param_.dst_height), 0, 0, cv::INTER_LINEAR);
  }
  return ret;
}

// bgr to yuv opencv/ffmpeg
bool ImagePreproc::Bgr2Yuv(const cv::Mat &src_image, uint8_t *dst_y, uint8_t *dst_uv) {
  if (dst_y == nullptr || dst_uv == nullptr) {
    LOG(ERROR) << "[ImagePreproc][Bgr2Yuv] data pointer is nullptr";
    return false;
  }
  if (src_image.cols * src_image.rows == 0 ||
      preproc_param_.dst_height * preproc_param_.dst_width == 0) {
    LOG(ERROR) << "[ImagePreproc][Bgr2Yuv] src w, src h, dst w or dst h is 0";
    return false;
  }
  uint8_t *dst_data = nullptr;
  uint32_t dst_frame_size = preproc_param_.dst_stride * preproc_param_.dst_height;
  uint32_t output_buf_size = dst_frame_size * 3 / 2;
  dst_data = new uint8_t[output_buf_size];
  if (!Bgr2Yuv(src_image, dst_data)) {
    delete[] dst_data;
    return false;
  }
  memcpy(dst_y, dst_data, dst_frame_size * sizeof(uint8_t));
  memcpy(dst_uv, dst_data + dst_frame_size, dst_frame_size / 2 * sizeof(uint8_t));
  delete[] dst_data;
  return true;
}

// bgr to yuv opencv/ffmpeg
bool ImagePreproc::Bgr2Yuv(const cv::Mat &src_image, uint8_t *dst) {
  if (dst == nullptr) {
    LOG(ERROR) << "[ImagePreproc][Bgr2Yuv] data pointer is nullptr";
    return false;
  }
  if (src_image.cols * src_image.rows == 0 ||
      preproc_param_.dst_height * preproc_param_.dst_width == 0) {
    LOG(ERROR) << "[ImagePreproc][Bgr2Yuv] src w, src h, dst w or dst h is 0";
    return false;
  }
  bool ret = true;
  uint32_t dst_frame_size = preproc_param_.dst_stride * preproc_param_.dst_height;
  uint32_t output_buf_size = dst_frame_size * 3 / 2;

  if (preproc_param_.use_ffmpeg) {
    uint32_t input_buf_size = src_image.cols * src_image.rows * 3;
    ret = ConvertWithFFmpeg(src_image.data, input_buf_size, dst, output_buf_size);
  } else {
    cv::Mat resized_image = cv::Mat(preproc_param_.dst_height, preproc_param_.dst_width, CV_8UC3);
    cv::resize(src_image, resized_image,
        cv::Size(preproc_param_.dst_width, preproc_param_.dst_height), 0, 0, cv::INTER_LINEAR);
    ret = Bgr2YUV420NV(resized_image, dst);
  }

  return ret;
}

bool ImagePreproc::Yuv2Yuv(const uint8_t *src_y, const uint8_t *src_uv, uint8_t *dst_y, uint8_t *dst_uv) {
  if (src_y == nullptr || src_uv == nullptr || dst_y == nullptr || dst_uv == nullptr) {
    LOG(ERROR) << "[ImagePreproc][Yuv2Yuv] data pointer is nullptr";
    return false;
  }
  if (preproc_param_.preproc_type == "cpu") {
    uint8_t *dst_data = nullptr;
    uint32_t dst_frame_size = preproc_param_.dst_stride * preproc_param_.dst_height;
    uint32_t output_buf_size = dst_frame_size * 3 / 2;
    dst_data = new uint8_t[output_buf_size];
    if (!Yuv2Yuv(src_y, src_uv, dst_data)) {
      delete[] dst_data;
      return false;
    }
    memcpy(dst_y, dst_data, dst_frame_size * sizeof(uint8_t));
    memcpy(dst_uv, dst_data + dst_frame_size, dst_frame_size / 2 * sizeof(uint8_t));
    delete[] dst_data;
  } else {
    // do mlu resize
    return false;
  }
  return true;
}

// yuv to yuv cpu/ffmpeg/mlu
bool ImagePreproc::Yuv2Yuv(const uint8_t *src_y, const uint8_t *src_uv, uint8_t *dst) {
  bool ret = true;
  if (src_y == nullptr || src_uv == nullptr || dst == nullptr) {
    LOG(ERROR) << "[ImagePreproc][Yuv2Yuv] data pointer is nullptr";
    return false;
  }
  if (preproc_param_.preproc_type != "cpu") {
    return false;
  }
  uint32_t dst_frame_size = preproc_param_.dst_stride * preproc_param_.dst_height;
  if (preproc_param_.src_width == preproc_param_.dst_width &&
      preproc_param_.src_height == preproc_param_.dst_height) {
    if (preproc_param_.dst_stride == preproc_param_.src_stride) {
      memcpy(dst, src_y, dst_frame_size * sizeof(uint8_t));
      memcpy(dst + dst_frame_size, src_uv, dst_frame_size * sizeof(uint8_t) / 2);
    } else {
      for (uint32_t y = 0; y < preproc_param_.dst_height; ++y) {
        memcpy(dst, src_y + preproc_param_.src_stride * y, preproc_param_.src_width * sizeof(uint8_t));
        dst += preproc_param_.dst_stride;
      }
      for (uint32_t uv = 0; uv < preproc_param_.dst_height / 2; ++uv) {
        memcpy(dst, src_uv + preproc_param_.src_stride * uv, preproc_param_.src_width * sizeof(uint8_t));
        dst += preproc_param_.dst_stride;
      }
    }
    return true;
  }
  uint8_t *src_data = nullptr;
  uint32_t src_frame_size = preproc_param_.src_stride * preproc_param_.src_height;
  uint32_t input_buf_size = src_frame_size * 3 / 2;
  uint32_t output_buf_size = preproc_param_.dst_stride * preproc_param_.dst_height * 3 / 2;
  src_data = new uint8_t[input_buf_size];
  memcpy(src_data, src_y, src_frame_size * sizeof(uint8_t));
  memcpy(src_data + src_frame_size, src_uv, src_frame_size / 2 * sizeof(uint8_t));

  if (preproc_param_.use_ffmpeg) {
    ret = ConvertWithFFmpeg(src_data, input_buf_size, dst, output_buf_size);
  } else {
    ret = ResizeYuvNearest(src_data, dst);
  }

  delete[] src_data;

  return ret;
}

// cpu yuv 2 yuv
bool ImagePreproc::ResizeYuvNearest(const uint8_t *src, uint8_t *dst) {
  if (!src || !dst) {
    LOG(ERROR) << "[ImagePreproc][ResizeYuvNearest] src or dst pointer is nullptr";
    return false;
  }

  uint32_t srcy, srcx, src_index;
  uint32_t xrIntFloat_16 = (preproc_param_.src_width << 16) / preproc_param_.dst_width + 1;
  uint32_t yrIntFloat_16 = (preproc_param_.src_height << 16) / preproc_param_.dst_height + 1;

  uint8_t *dst_uv = dst + preproc_param_.dst_height * preproc_param_.dst_stride;
  const uint8_t *src_uv = src + preproc_param_.src_height * preproc_param_.src_stride;
  uint8_t *dst_uv_yScanline = nullptr;
  const uint8_t *src_uv_yScanline = nullptr;
  uint8_t *dst_y_slice = dst;
  const uint8_t *src_y_slice = nullptr;
  uint8_t *sp = nullptr;
  const uint8_t *dp = nullptr;

  for (uint32_t y = 0; y < preproc_param_.dst_height; ++y) {
    srcy = (y * yrIntFloat_16) >> 16;
    src_y_slice = src + srcy * preproc_param_.src_stride;
    if (0 == (y & 1)) {
      dst_uv_yScanline = dst_uv + (y / 2) * preproc_param_.dst_stride;
      src_uv_yScanline = src_uv + (srcy / 2) * preproc_param_.src_stride;
    }
    for (uint32_t x = 0; x < preproc_param_.dst_width; ++x) {
      srcx = (x * xrIntFloat_16) >> 16;
      dst_y_slice[x] = src_y_slice[srcx];
      if ((y & 1) == 0) {    // y is even
        if ((x & 1) == 0) {  // x is even
          src_index = (srcx / 2) * 2;
          sp = dst_uv_yScanline + x;
          dp = src_uv_yScanline + src_index;
          *sp = *dp;
          ++sp;
          ++dp;
          *sp = *dp;
        }
      }
    }
    dst_y_slice += preproc_param_.dst_stride;
  }
  return true;
}

// opencv bgr 2 yuv
bool ImagePreproc::Bgr2YUV420NV(const cv::Mat &bgr, uint8_t *nv_data) {
  if (!nv_data) {
    LOG(ERROR) << "[ImagePreproc][Bgr2YUV420NV] dst nv_data is nullptr.";
    return false;
  }
  if (preproc_param_.dst_pix_fmt != NV12 && preproc_param_.dst_pix_fmt != NV21) {
    LOG(ERROR) << "[ImagePreproc][Bgr2YUV420NV] Unsupported pixel format.";
    return false;
  }

  uint32_t width, height, stride;
  width = bgr.cols;
  height = bgr.rows;
  stride = preproc_param_.dst_stride;
  if (width % 2 || height % 2 || width == 0 || height == 0 || stride == 0) {
    LOG(ERROR) << "[ImagePreproc][Bgr2YUV420NV] width or height is odd number or 0.";
    return false;
  }

  uint8_t *src_y, *src_u, *src_v, *dst_y, *dst_uv;
  cv::Mat yuvI420 = cv::Mat(height * 3 / 2, width, CV_8UC1);
  cv::cvtColor(bgr, yuvI420, cv::COLOR_BGR2YUV_I420);

  src_y = yuvI420.data;
  src_u = yuvI420.data + width * height;
  src_v = src_u + width * height / 4;

  dst_y = nv_data;
  dst_uv = nv_data + stride * height;

  for (uint32_t i = 0; i < height; i++) {
    // y data
    memcpy(dst_y + i * stride, src_y + i * width, width * sizeof(uint8_t));
    // uv data
    if (i % 2 == 0) {
      for (uint32_t j = 0; j < width / 2; j++) {
        if (preproc_param_.dst_pix_fmt == NV21) {
          *(dst_uv + i * stride / 2 + 2 * j) = *(src_v + i * width / 4 + j);
          *(dst_uv + i * stride / 2 + 2 * j + 1) = *(src_u + i * width / 4 + j);
        } else {
          *(dst_uv + i * stride / 2 + 2 * j) = *(src_u + i * width / 4 + j);
          *(dst_uv + i * stride / 2 + 2 * j + 1) = *(src_v + i * width / 4 + j);
        }
      }
    }
  }
  yuvI420.release();
  return true;
}

// ffmpeg bgr2yuv/ yuv2yuv / bgr2bgr
bool ImagePreproc::ConvertWithFFmpeg(const uint8_t *src_buffer, const size_t src_buffer_size, uint8_t *dst_buffer,
                                     const size_t dst_buffer_size) {
  if (!swsctx_|| !src_buffer || !dst_buffer) {
    LOG(ERROR) << "[ImagePreproc] Please init first.";
    return false;
  }
  if ((preproc_param_.dst_pix_fmt == NV12 || preproc_param_.dst_pix_fmt == NV21) &&
      (preproc_param_.dst_stride % 2 || preproc_param_.dst_height % 2)) {
    LOG(ERROR) << "[ImagePreproc][ConvertWithFFmpeg] dst stride or dst height is odd number.";
    return false;
  }
  if ((preproc_param_.src_pix_fmt == NV12 || preproc_param_.src_pix_fmt == NV21) &&
      (preproc_param_.src_stride % 2 || preproc_param_.src_height % 2)) {
    LOG(ERROR) << "[ImagePreproc][ConvertWithFFmpeg] src stride or src height is odd number.";
    return false;
  }
  size_t in_size =
      av_image_get_buffer_size(src_pix_fmt_, preproc_param_.src_width, preproc_param_.src_height, src_align_);
  if (in_size != src_buffer_size) {
    LOG(ERROR) << "[ImagePreproc][ConvertWithFFmpeg] The input buffer size does not match the expected size. Required:"
               << in_size << " Available: " << src_buffer_size;
    return false;
  }

  size_t outsize =
      av_image_get_buffer_size(dst_pix_fmt_, preproc_param_.dst_width, preproc_param_.dst_height, dst_align_);
  if (outsize < dst_buffer_size) {
    LOG(ERROR) << "[ImagePreproc][ConvertWithFFmpeg] The input buffer size does not match the expected size. Required:"
               << outsize << " Available: " << dst_buffer_size;
    return false;
  }

  if (av_image_fill_arrays(src_pic_->data, src_pic_->linesize, src_buffer, src_pix_fmt_,
                           preproc_param_.src_width, preproc_param_.src_height, src_align_) <= 0) {
    LOG(ERROR) << "[ImagePreproc][ConvertWithFFmpeg] Failed filling input frame with input buffer";
    return false;
  }

  if (av_image_fill_arrays(dst_pic_->data, dst_pic_->linesize, dst_buffer, dst_pix_fmt_,
                           preproc_param_.dst_width, preproc_param_.dst_height, dst_align_) <= 0) {
    LOG(ERROR) << "[ImagePreproc][ConvertWithFFmpeg] Failed filling output frame with output buffer";
    return false;
  }

  /* Do the conversion */
  if (sws_scale(swsctx_, src_pic_->data, src_pic_->linesize, 0, preproc_param_.src_height,
                dst_pic_->data, dst_pic_->linesize) < 0) {
    LOG(ERROR) << "[ImagePreproc][ConvertWithFFmpeg] resize and convert failed.";
    return false;
  }
  return true;
}

}  // namespace cnstream
