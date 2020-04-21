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

#include "ffmpeg_sws.hpp"

#include <glog/logging.h>

#define LOCK() std::lock_guard<std::recursive_mutex> _lock(this->mutex_)
#define UNLOCK()\
{\
  this->mutex_.unlock();\
}
#define CHECKSTOP() \
if (this->status_ != STOP)\
{\
  return AVERROR(EINVAL);\
}
#define CHECKNOTSTOP() \
if (this->status_ == STOP)\
{\
  return AVERROR(EINVAL);\
}

namespace cnstream {

FFSws::~FFSws() {
  UnlockOpt();
}

int FFSws::SetSrcOpt(AVPixelFormat pix_fmt, int w, int h) {
  LOCK();
  CHECKSTOP();
  src_pix_fmt_ = pix_fmt;
  src_w_ = w;
  src_h_ = h;
  return 0;
}

int FFSws::SetDstOpt(AVPixelFormat pix_fmt, int w, int h) {
  LOCK();
  CHECKSTOP();
  dst_pix_fmt_ = pix_fmt;
  dst_w_ = w;
  dst_h_ = h;
  return 0;
}

int FFSws::LockOpt() {
  LOCK();
  CHECKSTOP();
  int ret = 0;

  src_pic_ = av_frame_alloc();
  dst_pic_ = av_frame_alloc();
  if (src_pic_ == nullptr || dst_pic_ == nullptr) {
    LOG(ERROR) << "Failed allocating AVFrame for the src_pic/dst_pic";
    return ret;
  }
  sws_ctx_ = sws_getContext(src_w_, src_h_, src_pix_fmt_, dst_w_, dst_h_,
      dst_pix_fmt_, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
  if (sws_ctx_ == nullptr) {
    ret = AVERROR_BUG2;
  } else {
    status_ = LOCKED;
  }
  return ret;
}

int FFSws::UnlockOpt() {
  LOCK();
  /* Free up everything */
  if (src_pic_ != nullptr) {
    av_frame_free(&src_pic_);
    src_pic_ = nullptr;
  }
  if (dst_pic_ != nullptr) {
    av_frame_free(&dst_pic_);
    dst_pic_ = nullptr;
  }

  if (sws_ctx_ != nullptr) {
    sws_freeContext(sws_ctx_);
    sws_ctx_ = nullptr;
  }
  sws_ctx_ = nullptr;
  status_ = STOP;
  src_w_ = 0;
  src_h_ = 0;
  dst_w_ = 0;
  dst_h_ = 0;
  return 0;
}

int FFSws::Convert(const uint8_t* const src_slice[], const int src_stride[],
    int src_slice_y, int src_slice_h, uint8_t* const dst[],
    const int dst_stride[]) {
  int ret = -1;
  LOCK();
  CHECKNOTSTOP();
  ret = sws_scale(sws_ctx_, src_slice, src_stride, src_slice_y, src_slice_h,
      dst, dst_stride);
  return ret;
}
int FFSws::Convert(const uint8_t* src_buffer, const size_t src_buffer_size,
    uint8_t* dst_buffer, const size_t dst_buffer_size) {
  int ret = -1;
  LOCK();
  CHECKNOTSTOP();

  /* Check the buffer sizes */

  size_t insize = av_image_get_buffer_size(src_pix_fmt_, src_w_, src_h_, 1);
  if (insize != src_buffer_size) {
    LOG(ERROR) <<"The input buffer size does not match the expected size.  Required:"
      << insize << " Available: " << src_buffer_size;
    return ret;
  }

  size_t outsize = av_image_get_buffer_size(dst_pix_fmt_, dst_w_, dst_h_, 1);
  if (outsize < dst_buffer_size) {
    LOG(ERROR) << "The input buffer size does not match the expected size.Required:"
      << outsize << " Available: " << dst_buffer_size;
    return ret;
  }

  /* Fill in the buffers */
  if (av_image_fill_arrays(src_pic_->data, src_pic_->linesize,
        src_buffer, src_pix_fmt_, src_w_, src_h_, 1) <= 0) {
    LOG(ERROR) <<"Failed filling input frame with input buffer";
    return ret;
  }

  if (av_image_fill_arrays(dst_pic_->data, dst_pic_->linesize,
        dst_buffer, dst_pix_fmt_, dst_w_, dst_h_, 1) <= 0) {
    LOG(ERROR) <<"Failed filling output frame with output buffer";
    return ret;
  }

  /* Do the conversion */
  ret = sws_scale(sws_ctx_, src_pic_->data, src_pic_->linesize, 0, src_h_, dst_pic_->data, dst_pic_->linesize);
  return ret;
}

}  // namespace cnstream

