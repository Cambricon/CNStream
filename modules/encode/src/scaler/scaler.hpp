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

#ifndef __SCALER_H__
#define __SCALER_H__

#include <cstdint>

#include "opencv2/core/core.hpp"

namespace cnstream {

class Scaler {
 public:
  enum ColorFormat {
    YUV_I420 = 0,
    YUV_NV12,
    YUV_NV21,
    BGR,
    RGB,
    BGRA,
    RGBA,
    ABGR,
    ARGB,
    COLOR_MAX,
  };

  struct Buffer {
    uint32_t width, height;
    uint8_t *data[3];
    uint32_t stride[3];
    ColorFormat color;
  };

  struct Rect {
    int x, y, w, h;

    Rect() { x = 0; y = 0; w = 0; h = 0; }
    Rect(int rx, int ry, int rw, int rh) { x = rx; y = ry; w = rw; h = rh; }
    Rect(const Rect &r) { x = r.x; y = r.y; w = r.w; h = r.h; }
    Rect & operator=(const Rect &r) {
      x = r.x; y = r.y; w = r.w; h = r.h;
      return *this;
    }
    bool operator==(const Rect &r) const {
      return (x == r.x && y == r.y && w == r.w && h == r.h);
    }
    bool operator!=(const Rect &r) const {
      return !(x == r.x && y == r.y && w == r.w && h == r.h);
    }
  };

  static const Rect NullRect;

  enum Carrier {
    DEFAULT = -2,
    AUTO    = -1,
    OPENCV  = 0,
    LIBYUV,
    FFMPEG,
    CARRIER_MAX,
  };

  static void SetCarrier(int carrier) { carrier_ = carrier; }
  static int GetCarrier() { return carrier_; }
  static bool Process(const Buffer *src, Buffer *dst, const Rect *src_crop = nullptr, const Rect *dst_crop = nullptr,
                      int carrier = DEFAULT);

  static void MatToBuffer(const cv::Mat &mat, Scaler::ColorFormat color, Scaler::Buffer *buffer) {
    if (!buffer) return;
    buffer->width = mat.cols;
    buffer->height = mat.rows;
    buffer->color = color;
    if (color <=  Scaler::ColorFormat::YUV_NV21) {
      buffer->height = mat.rows * 2 / 3;
      buffer->data[0] = mat.data;
      buffer->stride[0] = mat.step;
      buffer->data[1] = mat.data + mat.step * buffer->height;
      buffer->stride[1] = mat.step;
      if (color ==  Scaler::ColorFormat::YUV_I420) {
        buffer->data[2] = mat.data + mat.step * buffer->height * 5 / 4;
        buffer->stride[1] = buffer->stride[2] = mat.step / 2;
      }
    } else {
      buffer->data[0] = mat.data;
      buffer->stride[0] = mat.step;
    }
  }

 private:
  static int carrier_;
};  // Scaler

}  // namespace cnstream

#endif  // __SCALER_H__
