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

#include <cstring>

#include "cnstream_logging.hpp"

#include "libyuv.h"

#include "scaler.hpp"

namespace cnstream {

using Buffer = Scaler::Buffer;
using ColorFormat = Scaler::ColorFormat;
using Rect = Scaler::Rect;

typedef int (*Planes3To3)(const uint8_t *, int, const uint8_t *, int, const uint8_t *, int, uint8_t *, int, uint8_t *,
                          int, uint8_t *, int, int, int);
typedef int (*Planes3To2)(const uint8_t *, int, const uint8_t *, int, const uint8_t *, int, uint8_t *, int, uint8_t *,
                          int, int, int);
typedef int (*Planes3To1)(const uint8_t *, int, const uint8_t *, int, const uint8_t *, int, uint8_t *, int, int, int);
typedef int (*Planes2To3)(const uint8_t *, int, const uint8_t *, int, uint8_t *, int, uint8_t *, int, uint8_t *, int,
                          int, int);
typedef int (*Planes2To2)(const uint8_t *, int, const uint8_t *, int, uint8_t *, int, uint8_t *, int, int, int);
typedef int (*Planes2To1)(const uint8_t *, int, const uint8_t *, int, uint8_t *, int, int, int);
typedef int (*Planes1To3)(const uint8_t *, int, uint8_t *, int, uint8_t *, int, uint8_t *, int, int, int);
typedef int (*Planes1To2)(const uint8_t *, int, uint8_t *, int, uint8_t *, int, int, int);
typedef int (*Planes1To1)(const uint8_t *, int, uint8_t *, int, int, int);

extern void ScalerFillBufferStride(Buffer *buffer);

static int LibYUVConvertColor(const Buffer *src, Buffer *dst) {
  if (src->color == ColorFormat::YUV_I420) {
    static const Planes3To2 to_yuvsp_map[2] = {
        libyuv::I420ToNV12,
        libyuv::I420ToNV21,
    };
    static const Planes3To1 to_1plane_map[2] = {
        libyuv::I420ToRGB24,
        libyuv::I420ToRAW,
    };
    if (dst->color == ColorFormat::YUV_I420) {
      return libyuv::I420Copy(src->data[0], src->stride[0], src->data[1], src->stride[1], src->data[2], src->stride[2],
                              dst->data[0], dst->stride[0], dst->data[1], dst->stride[1], dst->data[2], dst->stride[2],
                              src->width, src->height);
    } else if (dst->color <= ColorFormat::YUV_NV21) {
      Planes3To2 to_yuvsp = to_yuvsp_map[dst->color - ColorFormat::YUV_NV12];
      return to_yuvsp(src->data[0], src->stride[0], src->data[1], src->stride[1], src->data[2], src->stride[2],
                      dst->data[0], dst->stride[0], dst->data[1], dst->stride[1], src->width, src->height);
    } else if (dst->color <= ColorFormat::RGB) {
      Planes3To1 to_1plane = to_1plane_map[dst->color - ColorFormat::BGR];
      return to_1plane(src->data[0], src->stride[0], src->data[1], src->stride[1], src->data[2], src->stride[2],
                       dst->data[0], dst->stride[0], src->width, src->height);
    } else if (dst->color == ColorFormat::ARGB) {
      return libyuv::I420ToARGB(src->data[0], src->stride[0], src->data[1], src->stride[1], src->data[2],
                                src->stride[2], dst->data[0], dst->stride[0], src->width, src->height);
    }
  } else if (src->color <= ColorFormat::YUV_NV21) {
    static const Planes2To3 to_i420_map[2] = {
        libyuv::NV12ToI420,
        libyuv::NV21ToI420,
    };
    static const Planes2To1 to_1plane_map[2][2] = {
        {
            libyuv::NV12ToRGB24,
            libyuv::NV12ToRAW,
        },
        {
            libyuv::NV21ToRGB24,
            libyuv::NV21ToRAW,
        },
    };
    if (dst->color == ColorFormat::YUV_I420) {
      Planes2To3 to_i420 = to_i420_map[src->color - ColorFormat::YUV_NV12];
      return to_i420(src->data[0], src->stride[0], src->data[1], src->stride[1], dst->data[0], dst->stride[0],
                     dst->data[1], dst->stride[1], dst->data[2], dst->stride[2], src->width, src->height);
    } else if (dst->color <= ColorFormat::YUV_NV21) {
      if (src->color == dst->color) {
        return libyuv::NV12Copy(src->data[0], src->stride[0], src->data[1], src->stride[1], dst->data[0],
                                dst->stride[0], dst->data[1], dst->stride[1], src->width, src->height);
      } else {
        return libyuv::NV21ToNV12(src->data[0], src->stride[0], src->data[1], src->stride[1], dst->data[0],
                                  dst->stride[0], dst->data[1], dst->stride[1], src->width, src->height);
      }
    } else if (dst->color <= ColorFormat::RGB) {
      int src_color_index = src->color - ColorFormat::YUV_NV12;
      int dst_color_index = dst->color - ColorFormat::BGR;
      Planes2To1 to_1plane = to_1plane_map[src_color_index][dst_color_index];
      return to_1plane(src->data[0], src->stride[0], src->data[1], src->stride[1], dst->data[0], dst->stride[0],
                       src->width, src->height);
    } else if (dst->color == ColorFormat::ARGB) {
      if (src->color == ColorFormat::YUV_NV12) {
        return libyuv::NV12ToARGB(src->data[0], src->stride[0], src->data[1], src->stride[1], dst->data[0],
                                  dst->stride[0], src->width, src->height);
      } else if (src->color == ColorFormat::YUV_NV21) {
        return libyuv::NV21ToARGB(src->data[0], src->stride[0], src->data[1], src->stride[1], dst->data[0],
                                  dst->stride[0], src->width, src->height);
      }
    }
  } else if (src->color <= ColorFormat::RGB) {
    static const Planes1To3 to_i420_map[2] = {
        libyuv::RGB24ToI420,
        libyuv::RAWToI420,
    };
    static const Planes1To2 to_yuvsp_map[2][2] = {
        {
            libyuv::RGB24ToNV12,
            libyuv::RGB24ToNV21,
        },
        {
            libyuv::RAWToNV12,
            libyuv::RAWToNV21,
        },
    };
    static const Planes1To1 to_1plane_map[2][2] = {
        {
            libyuv::RGB24Copy,
            libyuv::RGB24ToRAW,
        },
        {
            libyuv::RAWToRGB24,
            libyuv::RAWCopy,
        },
    };
    if (dst->color == ColorFormat::YUV_I420) {
      Planes1To3 to_i420 = to_i420_map[src->color - ColorFormat::BGR];
      return to_i420(src->data[0], src->stride[0], dst->data[0], dst->stride[0], dst->data[1], dst->stride[1],
                     dst->data[2], dst->stride[2], src->width, src->height);
    } else if (dst->color <= ColorFormat::YUV_NV21) {
      int src_color_index = src->color - ColorFormat::BGR;
      int dst_color_index = dst->color - ColorFormat::YUV_NV12;
      Planes1To2 to_yuvsp = to_yuvsp_map[src_color_index][dst_color_index];
      return to_yuvsp(src->data[0], src->stride[0], dst->data[0], dst->stride[0], dst->data[1], dst->stride[1],
                      src->width, src->height);
    } else if (dst->color <= ColorFormat::RGB) {
      int src_color_index = src->color - ColorFormat::BGR;
      int dst_color_index = dst->color - ColorFormat::BGR;
      Planes1To1 to_1plane = to_1plane_map[src_color_index][dst_color_index];
      return to_1plane(src->data[0], src->stride[0], dst->data[0], dst->stride[0], src->width, src->height);
    } else if (dst->color == ColorFormat::ARGB) {
      if (src->color <= ColorFormat::BGR) {
        return libyuv::RGB24ToARGB(src->data[0], src->stride[0], dst->data[0], dst->stride[0], src->width, src->height);
      } else {
        return libyuv::RAWToARGB(src->data[0], src->stride[0], dst->data[0], dst->stride[0], src->width, src->height);
      }
    }
  } else if (src->color == ColorFormat::ARGB) {
    if (dst->color == ColorFormat::YUV_I420) {
      return libyuv::ARGBToI420(src->data[0], src->stride[0], dst->data[0], dst->stride[0], dst->data[1],
                                dst->stride[1], dst->data[2], dst->stride[2], src->width, src->height);
    } else if (dst->color == ColorFormat::YUV_NV12) {
      return libyuv::ARGBToNV12(src->data[0], src->stride[0], dst->data[0], dst->stride[0], dst->data[1],
                                dst->stride[1], src->width, src->height);
    } else if (dst->color == ColorFormat::YUV_NV21) {
      return libyuv::ARGBToNV21(src->data[0], src->stride[0], dst->data[0], dst->stride[0], dst->data[1],
                                dst->stride[1], src->width, src->height);
    } else if (dst->color <= ColorFormat::BGR) {
      return libyuv::ARGBToRGB24(src->data[0], src->stride[0], dst->data[0], dst->stride[0], src->width, src->height);
    } else if (dst->color <= ColorFormat::RGB) {
      return libyuv::ARGBToRAW(src->data[0], src->stride[0], dst->data[0], dst->stride[0], src->width, src->height);
    }
  }
  return -1;
}

static int LibYUVProcessI420(const Buffer *src, Buffer *dst) {
  int ret = 0;
  Buffer buffer;
  memset(&buffer, 0, sizeof(Buffer));
  // convert to argb first
  if (dst->color >= ColorFormat::BGR &&
      (dst->width % 2 == 1 || dst->height % 2 == 1 || dst->width > src->width || dst->height > src->height)) {
    uint8_t *argb_src_data = nullptr, *argb_dst_data = nullptr, *dst_data = nullptr;
    uint32_t dst_stride;
    argb_src_data = new uint8_t[src->width * src->height * 4];
    if (dst->color <= ColorFormat::RGB) {
      argb_dst_data = new uint8_t[dst->width * dst->height * 4];
      dst_data = argb_dst_data;
      dst_stride = dst->width * 4;
    } else {
      dst_data = dst->data[0];
      dst_stride = dst->stride[0];
    }
    ret = libyuv::I420ToARGB(src->data[0], src->stride[0], src->data[1], src->stride[1], src->data[2], src->stride[2],
                             argb_src_data, src->width * 4, src->width, src->height);
    if (ret != 0) {
      if (argb_src_data) delete[] argb_src_data;
      if (argb_dst_data) delete[] argb_dst_data;
      return ret;
    }
    ret = libyuv::ARGBScale(argb_src_data, src->width * 4, src->width, src->height, dst_data, dst_stride, dst->width,
                            dst->height, libyuv::kFilterBilinear);
    if (ret != 0) {
      if (argb_src_data) delete[] argb_src_data;
      if (argb_dst_data) delete[] argb_dst_data;
      return ret;
    }
    buffer.data[0] = dst_data;
    buffer.stride[0] = dst_stride;
    buffer.width = dst->width;
    buffer.height = dst->height;
    buffer.color = ColorFormat::ARGB;
    ret = LibYUVConvertColor(&buffer, dst);
    if (argb_src_data) delete[] argb_src_data;
    if (argb_dst_data) delete[] argb_dst_data;
  } else {
    uint8_t *data = nullptr;
    uint8_t *data_y, *data_u, *data_v;
    uint32_t stride_y, stride_u, stride_v;
    if (dst->color == ColorFormat::YUV_I420) {
      data_y = dst->data[0];
      stride_y = dst->stride[0];
      data_u = dst->data[1];
      stride_u = dst->stride[1];
      data_v = dst->data[2];
      stride_v = dst->stride[2];
    } else if (dst->color <= ColorFormat::YUV_NV21) {
      data_y = dst->data[0];
      stride_y = dst->stride[0];
      data = new uint8_t[dst->stride[1] * dst->height / 2];
      data_u = data;
      stride_u = dst->stride[1] / 2;
      data_v = data + dst->stride[1] * dst->height / 4;
      stride_v = dst->stride[1] / 2;
    } else {
      data = new uint8_t[dst->width * dst->height * 3 / 2];
      data_y = data;
      stride_y = dst->width;
      data_u = data + dst->width * dst->height;
      stride_u = dst->width / 2;
      data_v = data + dst->width * dst->height * 5 / 4;
      stride_v = dst->width / 2;
    }
    ret = libyuv::I420Scale(src->data[0], src->stride[0], src->data[1], src->stride[1], src->data[2], src->stride[2],
                            src->width, src->height, data_y, stride_y, data_u, stride_u, data_v, stride_v, dst->width,
                            dst->height, libyuv::kFilterBilinear);
    if (ret != 0) {
      if (data) delete[] data;
      return ret;
    }
    buffer.color = src->color;
    buffer.data[0] = data_y;
    buffer.stride[0] = stride_y;
    buffer.data[1] = data_u;
    buffer.stride[1] = stride_u;
    buffer.data[2] = data_v;
    buffer.stride[2] = stride_v;
    buffer.width = dst->width;
    buffer.height = dst->height;
    ret = LibYUVConvertColor(&buffer, dst);
    if (data) delete[] data;
  }
  return ret;
}

static int LibYUVProcessYUVsp(const Buffer *src, Buffer *dst) {
  int ret = 0;
  Buffer buffer;
  memset(&buffer, 0, sizeof(Buffer));
  // convert to argb first
  if (dst->color >= ColorFormat::BGR &&
      (dst->width % 2 == 1 || dst->height % 2 == 1 || dst->width > src->width || dst->height > src->height)) {
    uint8_t *argb_src_data = nullptr, *argb_dst_data = nullptr, *dst_data = nullptr;
    uint32_t dst_stride;
    argb_src_data = new uint8_t[src->width * src->height * 4];
    if (dst->color <= ColorFormat::RGB) {
      argb_dst_data = new uint8_t[dst->width * dst->height * 4];
      dst_data = argb_dst_data;
      dst_stride = dst->width * 4;
    } else {
      dst_data = dst->data[0];
      dst_stride = dst->stride[0];
    }
    if (src->color == ColorFormat::YUV_NV12) {
      ret = libyuv::NV12ToARGB(src->data[0], src->stride[0], src->data[1], src->stride[1], argb_src_data,
                               src->width * 4, src->width, src->height);
    } else {
      ret = libyuv::NV21ToARGB(src->data[0], src->stride[0], src->data[1], src->stride[1], argb_src_data,
                               src->width * 4, src->width, src->height);
    }
    if (ret != 0) {
      if (argb_src_data) delete[] argb_src_data;
      if (argb_dst_data) delete[] argb_dst_data;
      return ret;
    }
    ret = libyuv::ARGBScale(argb_src_data, src->width * 4, src->width, src->height, dst_data, dst_stride, dst->width,
                            dst->height, libyuv::kFilterBilinear);
    if (ret != 0) {
      if (argb_src_data) delete[] argb_src_data;
      if (argb_dst_data) delete[] argb_dst_data;
      return ret;
    }
    buffer.data[0] = dst_data;
    buffer.stride[0] = dst_stride;
    buffer.width = dst->width;
    buffer.height = dst->height;
    buffer.color = ColorFormat::ARGB;
    ret = LibYUVConvertColor(&buffer, dst);
    if (argb_src_data) delete[] argb_src_data;
    if (argb_dst_data) delete[] argb_dst_data;
  } else {
    uint8_t *data = nullptr;
    uint8_t *data_y, *data_uv;
    uint32_t stride_y, stride_uv;
    if (dst->color == ColorFormat::YUV_I420) {
      data_y = dst->data[0];
      stride_y = dst->stride[0];
      data = new uint8_t[dst->stride[1] * dst->height];
      data_uv = data;
      stride_uv = dst->stride[1] * 2;
    } else if (dst->color <= ColorFormat::YUV_NV21) {
      data_y = dst->data[0];
      stride_y = dst->stride[0];
      data_uv = dst->data[1];
      stride_uv = dst->stride[1];
    } else {
      data = new uint8_t[dst->width * dst->height * 3 / 2];
      data_y = data;
      stride_y = dst->width;
      data_uv = data + dst->width * dst->height;
      stride_uv = dst->width;
    }
    ret = libyuv::NV12Scale(src->data[0], src->stride[0], src->data[1], src->stride[1], src->width, src->height, data_y,
                            stride_y, data_uv, stride_uv, dst->width, dst->height, libyuv::kFilterBilinear);
    if (ret != 0) {
      if (data) delete[] data;
      return ret;
    }
    buffer.color = src->color;
    buffer.data[0] = data_y;
    buffer.stride[0] = stride_y;
    buffer.data[1] = data_uv;
    buffer.stride[1] = stride_uv;
    buffer.width = dst->width;
    buffer.height = dst->height;
    ret = LibYUVConvertColor(&buffer, dst);
    if (data) delete[] data;
  }
  return ret;
}

static int LibYUVProcessBGRRGB(const Buffer *src, Buffer *dst) {
  int ret = 0;
  Buffer buffer;
  memset(&buffer, 0, sizeof(Buffer));
  // convert to argb first
  uint8_t *argb_src_data = nullptr, *argb_dst_data = nullptr, *dst_data = nullptr;
  uint32_t dst_stride;
  if (src->color <= ColorFormat::RGB) {
    argb_src_data = new uint8_t[src->width * src->height * 4];
    buffer.data[0] = argb_src_data;
    buffer.stride[0] = src->width * 4;
    buffer.width = src->width;
    buffer.height = src->height;
    buffer.color = ColorFormat::ARGB;
    ret = LibYUVConvertColor(src, &buffer);
    if (ret != 0) {
      if (argb_src_data) delete[] argb_src_data;
      if (argb_dst_data) delete[] argb_dst_data;
      return ret;
    }
  } else {
    buffer.data[0] = src->data[0];
    buffer.stride[0] = src->stride[0];
  }
  if (dst->color <= ColorFormat::RGB) {
    argb_dst_data = new uint8_t[dst->width * dst->height * 4];
    dst_data = argb_dst_data;
    dst_stride = dst->width * 4;
  } else {
    dst_data = dst->data[0];
    dst_stride = dst->stride[0];
  }
  ret = libyuv::ARGBScale(buffer.data[0], buffer.stride[0], src->width, src->height, dst_data, dst_stride, dst->width,
                          dst->height, libyuv::kFilterBilinear);
  if (ret != 0) {
    if (argb_src_data) delete[] argb_src_data;
    if (argb_dst_data) delete[] argb_dst_data;
    return ret;
  }
  buffer.data[0] = dst_data;
  buffer.stride[0] = dst_stride;
  buffer.width = dst->width;
  buffer.height = dst->height;
  if (src->color <= ColorFormat::RGB) {
    buffer.color = ColorFormat::ARGB;
  } else {
    buffer.color = src->color;
  }
  ret = LibYUVConvertColor(&buffer, dst);
  if (argb_src_data) delete[] argb_src_data;
  if (argb_dst_data) delete[] argb_dst_data;
  return ret;
}

bool LibYUVProcess(const Buffer *src, Buffer *dst) {
  if (!src || !dst) return false;

  if (src->color > ColorFormat::RGB || dst->color > ColorFormat::RGB) {
    LOGE(ScalerLibYUV) << "LibYUVProcess() unsupport color";
    return false;
  }

  if (src->width == dst->width && src->height == dst->height) {
    if (0 != LibYUVConvertColor(src, dst)) {
      LOGE(ScalerLibYUV) << "LibYUVProcess() convert color failed";
      return false;
    }
  } else if (src->color == ColorFormat::YUV_I420) {
    if (0 != LibYUVProcessI420(src, dst)) {
      LOGE(ScalerLibYUV) << "LibYUVProcess() process I420 failed";
      return false;
    }
  } else if (src->color <= ColorFormat::YUV_NV21) {
    if (0 != LibYUVProcessYUVsp(src, dst)) {
      LOGE(ScalerLibYUV) << "LibYUVProcess() process YUVsp failed";
      return false;
    }
  } else {
    if (0 != LibYUVProcessBGRRGB(src, dst)) {
      LOGE(ScalerLibYUV) << "LibYUVProcess() process BGRRGB failed";
      return false;
    }
  }
  return true;
}

}  // namespace cnstream
