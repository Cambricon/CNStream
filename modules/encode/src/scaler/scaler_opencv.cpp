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

#include "cnstream_logging.hpp"

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "scaler.hpp"

namespace cnstream {

using Buffer = Scaler::Buffer;
using ColorFormat = Scaler::ColorFormat;
using Rect = Scaler::Rect;

extern uint32_t ScalerGetBufferStrideInBytes(const Buffer *buffer);
extern uint32_t ScalerGetBufferStrideInPixels(const Buffer *buffer);
extern void ScalerFillBufferStride(Buffer *buffer);

static bool IsBufferContinuous(const Buffer *buffer) {
  if (buffer->color <= ColorFormat::YUV_NV21) {
    uint32_t stride_y = buffer->stride[0] < buffer->width ? buffer->width : buffer->stride[0];
    if ((buffer->data[1] - buffer->data[0]) != stride_y * buffer->height) return false;
    if (buffer->color == ColorFormat::YUV_I420) {
      uint32_t stride_u = buffer->stride[1] < buffer->width / 2 ? buffer->width / 2 : buffer->stride[1];
      if ((buffer->data[2] - buffer->data[1]) != stride_u * buffer->height / 2) return false;
    }
  }
  return true;
}

static void OpenCVMatToBuffer(const cv::Mat &mat, Buffer *buffer, bool copy = true) {
  if (!buffer) return;

  if (!copy) {
    buffer->width = mat.cols;
    if (buffer->color <= ColorFormat::YUV_NV21) {
      buffer->height = mat.rows * 2 / 3;
      buffer->data[0] = mat.data;
      buffer->stride[0] = mat.step;
      if (buffer->color != ColorFormat::YUV_I420) {
        buffer->data[1] = mat.data + mat.step * buffer->height;
        buffer->stride[1] = mat.step;
      } else {
        buffer->data[1] = mat.data + mat.step * buffer->height;
        buffer->stride[1] = mat.step / 2;
        buffer->data[2] = mat.data + mat.step * buffer->height * 5 / 4;
        buffer->stride[3] = mat.step / 2;
      }
    } else {
      buffer->height = mat.rows;
      buffer->data[0] = mat.data;
      buffer->stride[0] = mat.step;
    }
  } else {
    uint32_t stride = ScalerGetBufferStrideInPixels(buffer);
    if (buffer->color <= ColorFormat::YUV_NV21) {
      unsigned char *mat_u = mat.data + mat.step * buffer->height;
      unsigned char *mat_v = mat.data + mat.step * buffer->height * 5 / 4;
      for (uint32_t i = 0; i < buffer->height; i++) {
        memcpy(buffer->data[0] + stride * i, mat.data + mat.step * i, mat.cols);
        if (i % 2 == 0) {
          if (buffer->color != ColorFormat::YUV_I420) {
            memcpy(buffer->data[1] + stride * i / 2, mat_u + mat.step * i / 2, mat.cols);
          } else {
            memcpy(buffer->data[1] + stride * i / 4, mat_u + mat.step * i / 4, mat.cols / 2);
            memcpy(buffer->data[2] + stride * i / 4, mat_v + mat.step * i / 4, mat.cols / 2);
          }
        }
      }
    } else if (buffer->color <= ColorFormat::RGB) {
      for (uint32_t i = 0; i < buffer->height; i++) {
        memcpy(buffer->data[0] + stride * i * 3, mat.data + mat.step * i, mat.cols * 3);
      }
    } else {
      for (uint32_t i = 0; i < buffer->height; i++) {
        memcpy(buffer->data[0] + stride * i * 4, mat.data + mat.step * i, mat.cols * 4);
      }
    }
  }
}

static void OpenCVBufferToMat(const Buffer *buffer, cv::Mat *mat, bool copy = false) {
  if (!buffer || !mat) return;

  uint32_t stride = ScalerGetBufferStrideInPixels(buffer);
  if (buffer->color <= ColorFormat::YUV_NV21) {
    if (IsBufferContinuous(buffer) && !copy) {
      *mat = cv::Mat(buffer->height * 3 / 2, stride, CV_8UC1, buffer->data[0]);
      mat->cols = buffer->width;
      mat->step = stride;
    } else {
      // LOGI(ScalerOpenCV) << "OpenCVBufferToMat() copy";
      *mat = cv::Mat(buffer->height * 3 / 2, buffer->width, CV_8UC1);
      uint8_t *mat_u = mat->data + buffer->width * buffer->height;
      uint8_t *mat_v = mat->data + buffer->width * buffer->height * 5 / 4;
      for (uint32_t i = 0; i < buffer->height; i++) {
        memcpy(mat->data + buffer->width * i, buffer->data[0] + stride * i, buffer->width);
        if (i % 2 == 0) {
          if (buffer->color != ColorFormat::YUV_I420) {
            memcpy(mat_u + buffer->width * i / 2, buffer->data[1] + stride * i / 2, buffer->width);
          } else {
            memcpy(mat_u + buffer->width * i / 4, buffer->data[1] + stride * i / 4, buffer->width / 2);
            memcpy(mat_v + buffer->width * i / 4, buffer->data[2] + stride * i / 4, buffer->width / 2);
          }
        }
      }
    }
  } else if (buffer->color <= ColorFormat::RGB) {
    if (!copy) {
      *mat = cv::Mat(buffer->height, stride, CV_8UC3, buffer->data[0]);
      mat->cols = buffer->width;
      mat->step = stride * 3;
    } else {
      // LOGI(ScalerOpenCV) << "OpenCVBufferToMat() copy";
      *mat = cv::Mat(buffer->height, buffer->width, CV_8UC3);
      for (uint32_t i = 0; i < buffer->height; i++) {
        memcpy(mat->data + buffer->width * i * 3, buffer->data[0] + stride * i * 3, buffer->width * 3);
      }
    }
  } else {
    if (!copy) {
      *mat = cv::Mat(buffer->height, stride, CV_8UC4, buffer->data[0]);
      mat->cols = buffer->width;
      mat->step = stride * 4;
    } else {
      LOGI(ScalerOpenCV) << "OpenCVBufferToMat() copy";
      *mat = cv::Mat(buffer->height, buffer->width, CV_8UC4);
      for (uint32_t i = 0; i < buffer->height; i++) {
        memcpy(mat->data + buffer->width * i * 4, buffer->data[0] + stride * i * 4, buffer->width * 4);
      }
    }
  }
}

static void OpenCVI420ToYUVsp(cv::Mat *mat, bool nv12 = true) {
  uint32_t width = mat->cols;
  uint32_t stride = mat->step;
  uint32_t height = mat->rows / 3 * 2;
  uint8_t *uv_data = new uint8_t[stride * height / 2];
  uint8_t *u_ptr = uv_data;
  uint8_t *v_ptr = uv_data + stride * height / 4;
  uint8_t *uv_ptr = mat->data + stride * height;
  memcpy(uv_data, uv_ptr, stride * height / 2);
  for (uint32_t i = 0; i < height / 2; i++) {
    for (uint32_t j = 0; j < width / 2; j++) {
      if (nv12) {
        uv_ptr[i * stride + j * 2] = u_ptr[i * stride / 2 + j];
        uv_ptr[i * stride + j * 2 + 1] = v_ptr[i * stride / 2 + j];
      } else {
        uv_ptr[i * stride + j * 2] = v_ptr[i * stride / 2 + j];
        uv_ptr[i * stride + j * 2 + 1] = u_ptr[i * stride / 2 + j];
      }
    }
  }
  delete[] uv_data;
}

static void OpenCVYUVspToI420(cv::Mat *mat, bool nv12 = true) {
  uint32_t width = mat->cols;
  uint32_t stride = mat->step;
  uint32_t height = mat->rows / 3 * 2;
  uint8_t *uv_data = new uint8_t[stride * height / 2];
  uint8_t *uv_ptr = mat->data + stride * height;
  uint8_t *u_ptr = uv_ptr;
  uint8_t *v_ptr = uv_ptr + stride * height / 4;
  memcpy(uv_data, uv_ptr, stride * height / 2);
  for (uint32_t i = 0; i < height / 2; i++) {
    for (uint32_t j = 0; j < width / 2; j++) {
      if (nv12) {
        u_ptr[i * stride / 2 + j] = uv_data[i * stride + j * 2];
        v_ptr[i * stride / 2 + j] = uv_data[i * stride + j * 2 + 1];
      } else {
        v_ptr[i * stride / 2 + j] = uv_data[i * stride + j * 2];
        u_ptr[i * stride / 2 + j] = uv_data[i * stride + j * 2 + 1];
      }
    }
  }
  delete[] uv_data;
}

static void OpenCVNV12ToNV21(cv::Mat *mat) {
  uint32_t width = mat->cols;
  uint32_t stride = mat->step;
  uint32_t height = mat->rows / 3 * 2;
  uint8_t *uv_ptr = mat->data + stride * height;
  uint8_t uv;
  for (uint32_t i = 0; i < height / 2; i++) {
    for (uint32_t j = 0; j < width / 2; j++) {
      uv = uv_ptr[i * stride + j * 2];
      uv_ptr[i * stride + j * 2] = uv_ptr[i * stride + j * 2 + 1];
      uv_ptr[i * stride + j * 2 + 1] = uv;
    }
  }
}

static bool OpenCVConvertColor(const cv::Mat &src, ColorFormat src_color,
                               cv::Mat *dst, ColorFormat dst_color) {
  static const int color_convert_map[5][5] = {
    { -1, cv::COLOR_COLORCVT_MAX, cv::COLOR_COLORCVT_MAX, cv::COLOR_YUV2BGR_I420, cv::COLOR_YUV2RGB_I420, },
    { cv::COLOR_COLORCVT_MAX, -1, cv::COLOR_COLORCVT_MAX, cv::COLOR_YUV2BGR_NV12, cv::COLOR_YUV2RGB_NV12, },
    { cv::COLOR_COLORCVT_MAX, cv::COLOR_COLORCVT_MAX, -1, cv::COLOR_YUV2BGR_NV21, cv::COLOR_YUV2RGB_NV21, },
    { cv::COLOR_BGR2YUV_I420, cv::COLOR_BGR2YUV_I420, cv::COLOR_BGR2YUV_I420, -1, cv::COLOR_BGR2RGB, },
    { cv::COLOR_RGB2YUV_I420, cv::COLOR_RGB2YUV_I420, cv::COLOR_RGB2YUV_I420, cv::COLOR_RGB2BGR, -1, },
  };

  cv::Mat src_mat = src;
  if (src_color == dst_color) {
    *dst = src;
    return true;
  }
  int code = color_convert_map[src_color][dst_color];
  if (code == -1) {
    LOGE(ScalerOpenCV) << "OpenCVConvertColor() unsupport color convert type";
    return false;
  }
  if (src_color <= ColorFormat::YUV_NV21 && dst_color <= ColorFormat::YUV_NV21) {
    if (src_color == ColorFormat::YUV_I420) {
      OpenCVI420ToYUVsp(&src_mat, dst_color == ColorFormat::YUV_NV12);
    } else if (dst_color == ColorFormat::YUV_I420) {
      OpenCVYUVspToI420(&src_mat, src_color == ColorFormat::YUV_NV12);
    } else {
      OpenCVNV12ToNV21(&src_mat);
    }
    *dst = src_mat;
    return true;
  }
  if (code == cv::COLOR_COLORCVT_MAX) {
    *dst = src_mat;
  } else {
    cv::cvtColor(src_mat, *dst, code);
  }
  if (dst_color == ColorFormat::YUV_NV12 || dst_color == ColorFormat::YUV_NV21) {
    OpenCVI420ToYUVsp(dst, dst_color == ColorFormat::YUV_NV12);
  }
  return true;
}

static void OpenCVCopy(const Buffer *src, Buffer *dst) {
  if (src->color <= ColorFormat::YUV_NV21) {
    for (uint32_t i = 0; i < src->height; i++) {
      memcpy(dst->data[0] + dst->stride[0] * i, src->data[0] + src->stride[0] * i, src->width);
    }
    if (src->color != ColorFormat::YUV_I420) {
      for (uint32_t i = 0; i < (src->height / 2); i++) {
        memcpy(dst->data[1] + dst->stride[1] * i, src->data[1] + src->stride[1] * i, src->width);
      }
    } else {
      for (uint32_t i = 0; i < (src->height / 2); i++) {
        memcpy(dst->data[1] + dst->stride[1] * i, src->data[1] + src->stride[1] * i, src->width / 2);
      }
      for (uint32_t i = 0; i < (src->height / 2); i++) {
        memcpy(dst->data[2] + dst->stride[2] * i, src->data[2] + src->stride[2] * i, src->width / 2);
      }
    }
  } else if (src->color <= ColorFormat::RGB) {
    for (uint32_t i = 0; i < src->height; i++) {
      memcpy(dst->data[0] + dst->stride[0] * i, src->data[0] + src->stride[0] * i, src->width * 3);
    }
  } else {
    for (uint32_t i = 0; i < dst->height; i++) {
      memcpy(dst->data[0] + dst->stride[0] * i, src->data[0] + src->stride[0] * i, src->width * 4);
    }
  }
}
/*
static void OpenCVDumpMat(const std::string &file, const cv::Mat &mat, ColorFormat color) {
  cv::Mat dump_mat = mat.clone();
  OpenCVConvertColor(dump_mat, color, &dump_mat, ColorFormat::BGR);
  cv::imwrite(file, dump_mat);
}

static void OpenCVDumpBuffer(const std::string &file, const Buffer *buffer) {
  cv::Mat mat;
  OpenCVBufferToMat(buffer, &mat, true);
  OpenCVConvertColor(mat, buffer->color, &mat, ColorFormat::BGR);
  cv::imwrite(file, mat);
}
*/
static void OpenCVResize(const cv::Mat &src, cv::Mat *dst, uint32_t dst_width, uint32_t dst_height,
                         ColorFormat color) {
  if (color >= ColorFormat::BGR) {
    cv::resize(src, *dst, cv::Size(dst_width, dst_height));
  } else {
    if (color != ColorFormat::YUV_I420) {
      cv::Mat *mat = const_cast<cv::Mat *>(&src);
      OpenCVYUVspToI420(mat, color == ColorFormat::YUV_NV12);
    }
    dst_height = dst_height * 3 / 2;
    cv::resize(src, *dst, cv::Size(dst_width, dst_height));
    if (color != ColorFormat::YUV_I420) {
      OpenCVI420ToYUVsp(dst, color == ColorFormat::YUV_NV12);
    }
  }
}

bool OpenCVProcess(const Buffer *src, Buffer *dst) {
  if (!src || !dst) return false;

  if (src->color > ColorFormat::RGB || dst->color > ColorFormat::RGB) {
    LOGE(ScalerOpenCV) << "OpenCVProcess() unsupport color";
    return false;
  }

  if (src->width == dst->width && src->height == dst->height && src->color == dst->color) {
    OpenCVCopy(src, dst);
    // OpenCVDumpBuffer("OpenCVCopy.jpg", dst);
    return true;
  }
  cv::Mat src_mat, dst_mat;
  OpenCVBufferToMat(src, &src_mat);
  // OpenCVDumpBuffer("OpenCVCrop.jpg", src);
  // Only convert color
  if (src->width == dst->width && src->height == dst->height) {
    if (OpenCVConvertColor(src_mat, src->color, &dst_mat, dst->color)) {
      OpenCVMatToBuffer(dst_mat, dst);
      // OpenCVDumpBuffer("OpenCVConvert.jpg", dst);
      return true;
    }
    return false;
  }
  // Only resize
  if (src->color == dst->color) {
    OpenCVResize(src_mat, &dst_mat, dst->width, dst->height, src->color);
    OpenCVMatToBuffer(dst_mat, dst);
    // OpenCVDumpBuffer("OpenCVResize.jpg", dst);
    return true;
  }
  if (src->color <= ColorFormat::YUV_NV21 && dst->color >= ColorFormat::BGR &&
      (dst->width % 2 == 1 || dst->height % 2 == 1 || dst->width > src->width || dst->height > src->height)) {
    // do color convert first and then resize
    if (!OpenCVConvertColor(src_mat, src->color, &dst_mat, dst->color)) {
      LOGE(ScalerOpenCV) << "OpenCVProcess() convert color 1 failed";
      return false;
    }
    // OpenCVDumpMat("OpenCVConvert1.jpg", dst_mat, dst->color);
    src_mat = dst_mat;
    OpenCVResize(src_mat, &dst_mat, dst->width, dst->height, dst->color);
    OpenCVMatToBuffer(dst_mat, dst);
    // OpenCVDumpBuffer("OpenCVResize1.jpg", dst);
    return true;
  } else {
    // do resize first and then color convert
    OpenCVResize(src_mat, &dst_mat, dst->width, dst->height, src->color);
    // OpenCVDumpMat("OpenCVResize2.jpg", dst_mat, src->color);
    src_mat = dst_mat;
    if (!OpenCVConvertColor(src_mat, src->color, &dst_mat, dst->color)) {
      LOGE(ScalerOpenCV) << "OpenCVProcess() convert color 2 failed";
      return false;
    }
    OpenCVMatToBuffer(dst_mat, dst);
    // OpenCVDumpBuffer("OpenCVConvert2.jpg", dst);
    return true;
  }
  return true;
}

}  // namespace cnstream
