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

#include "scaler.hpp"

namespace cnstream {


extern bool OpenCVProcess(const Scaler::Buffer *src, Scaler::Buffer *dst);
extern bool LibYUVProcess(const Scaler::Buffer *src, Scaler::Buffer *dst);
extern bool FFmpegProcess(const Scaler::Buffer *src, Scaler::Buffer *dst);


const Scaler::Rect Scaler::NullRect = {0, 0, 0, 0};

int Scaler::carrier_ = Scaler::AUTO;

uint32_t ScalerGetBufferStrideInBytes(const Scaler::Buffer *buffer) {
  if (!buffer) return 0;
  if (buffer->color <= Scaler::ColorFormat::YUV_NV21) {
    return (buffer->stride[0] < buffer->width ? buffer->width : buffer->stride[0]);
  } else if (buffer->color <= Scaler::ColorFormat::RGB) {
    return (buffer->stride[0] < buffer->width * 3 ? buffer->width * 3 : buffer->stride[0]);
  } else {
    return (buffer->stride[0] < buffer->width * 4 ? buffer->width * 4 : buffer->stride[0]);
  }
}

uint32_t ScalerGetBufferStrideInPixels(const Scaler::Buffer *buffer) {
  if (!buffer) return 0;
  if (buffer->color <= Scaler::ColorFormat::YUV_NV21) {
    return (buffer->stride[0] < buffer->width ? buffer->width : buffer->stride[0]);
  } else if (buffer->color <= Scaler::ColorFormat::RGB) {
    return (buffer->stride[0] < buffer->width * 3 ? buffer->width * 3 : buffer->stride[0]) / 3;
  } else {
    return (buffer->stride[0] < buffer->width * 4 ? buffer->width * 4 : buffer->stride[0]) / 4;
  }
}

void ScalerFillBufferStride(Scaler::Buffer *buffer) {
  if (!buffer) return;

  if (buffer->color <= Scaler::ColorFormat::YUV_NV21) {
    buffer->stride[0] = buffer->stride[0] < buffer->width ? buffer->width : buffer->stride[0];
    if (buffer->color != Scaler::ColorFormat::YUV_I420) {
      buffer->stride[1] = buffer->stride[1] < buffer->width ? buffer->width : buffer->stride[1];
    } else {
      buffer->stride[1] = buffer->stride[1] < (buffer->width / 2) ? (buffer->width / 2) : buffer->stride[1];
      buffer->stride[2] = buffer->stride[2] < (buffer->width / 2) ? (buffer->width / 2) : buffer->stride[2];
    }
  } else if (buffer->color <= Scaler::ColorFormat::RGB) {
    buffer->stride[0] = buffer->stride[0] < (buffer->width * 3) ? (buffer->width * 3) : buffer->stride[0];
  } else {
    buffer->stride[0] = buffer->stride[0] < (buffer->width * 4) ? (buffer->width * 4) : buffer->stride[0];
  }
}

void ScalerGetCropBuffer(const Scaler::Buffer *src, Scaler::Buffer *dst, const Scaler::Rect *crop) {
  if (!src || !dst) return;

  *dst = *src;
  ScalerFillBufferStride(dst);
  if (!crop) {
    if (dst->color <= Scaler::ColorFormat::YUV_NV21) {
      if (dst->width % 2) dst->width--;
      if (dst->height % 2) dst->height--;
    }
  } else {
    uint32_t crop_x = crop->x, crop_y = crop->y;
    if (dst->color <= Scaler::ColorFormat::YUV_NV21) {
      if (crop_x % 2) crop_x--;
      if (crop_y % 2) crop_y--;
      if (crop->w == 0) {
        dst->width = dst->width - crop_x;
      } else {
        dst->width = (crop_x + crop->w) > dst->width ? (dst->width - crop_x) : crop->w;
      }
      if (crop->h == 0) {
        dst->height = dst->height - crop_y;
      } else {
        dst->height = (crop_y + crop->h) > dst->height ? (dst->height - crop_y) : crop->h;
      }
      if (dst->width % 2) dst->width--;
      if (dst->height % 2) dst->height--;
      dst->data[0] += dst->stride[0] * crop_y + crop_x;
      if (dst->color != Scaler::ColorFormat::YUV_I420) {
        dst->data[1] += dst->stride[1] * crop_y / 2 + crop_x;
      } else {
        dst->data[1] += (dst->stride[1] * crop_y + crop_x) / 2;
        dst->data[2] += (dst->stride[2] * crop_y + crop_x) / 2;
      }
    } else {
      if (crop->w == 0) {
        dst->width = dst->width - crop_x;
      } else {
        dst->width = (crop_x + crop->w) > dst->width ? (dst->width - crop_x) : crop->w;
      }
      if (crop->h == 0) {
        dst->height = dst->height - crop_y;
      } else {
        dst->height = (crop_y + crop->h) > dst->height ? (dst->height - crop_y) : crop->h;
      }
      uint32_t bytes_in_pixel = 3;
      if (dst->color >= Scaler::ColorFormat::BGRA) bytes_in_pixel = 4;
      dst->data[0] += dst->stride[0] * crop_y + crop_x * bytes_in_pixel;
    }
  }
}

bool Scaler::Process(const Buffer *src, Buffer *dst, const Rect *src_crop, const Rect *dst_crop, int carrier) {
  // do some parameters check
  if (carrier == DEFAULT) carrier = carrier_;
  if (carrier == AUTO) carrier += 2;
  if (carrier == CARRIER_MAX) {
    LOGE(Scaler) << "no valid arithmetic operator found";
    return false;
  }

  Buffer src_buf, dst_buf;

  ScalerGetCropBuffer(src, &src_buf, src_crop);
  ScalerGetCropBuffer(dst, &dst_buf, dst_crop);
  if (carrier == OPENCV) {
    return OpenCVProcess(&src_buf, &dst_buf);
  } else if (carrier == LIBYUV) {
    return LibYUVProcess(&src_buf, &dst_buf);
  } else if (carrier == FFMPEG) {
    return FFmpegProcess(&src_buf, &dst_buf);
  } else {
    LOGE(Scaler) << "unsupported arithmetic operator";
    return false;
  }

  return true;
}

}  // namespace cnstream
