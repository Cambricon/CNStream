/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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

#include "video_preprocess_common.hpp"

bool ConvertColorSpace(size_t width, size_t height, size_t stride, VideoPixelFmt src_fmt, VideoPixelFmt dst_fmt,
                       uint8_t* src_img_data, cv::Mat* dst_img) {
  cv::Mat src_img;
  cv::Mat dst_img_tmp;
  bool cvt_ret = true;
  switch (src_fmt) {
    case VideoPixelFmt::NV12: {  /*src nv12*/
      src_img = cv::Mat(height * 3 / 2, stride, CV_8UC1, src_img_data);
      switch (dst_fmt) {
        case VideoPixelFmt::RGB24: cv::cvtColor(src_img, dst_img_tmp, cv::COLOR_YUV2RGB_NV12); break;
        case VideoPixelFmt::BGR24: cv::cvtColor(src_img, dst_img_tmp, cv::COLOR_YUV2BGR_NV12); break;
        case VideoPixelFmt::RGBA:
        case VideoPixelFmt::BGRA:
        case VideoPixelFmt::ARGB:
        case VideoPixelFmt::ABGR: cv::cvtColor(src_img, dst_img_tmp, cv::COLOR_YUV2BGRA_NV12); break;
        default: cvt_ret = false; break;
      }
      break;
    }
    case VideoPixelFmt::NV21: {  /*src nv21*/
      src_img = cv::Mat(height * 3 / 2, stride, CV_8UC1, src_img_data);
      switch (dst_fmt) {
        case VideoPixelFmt::RGB24: cv::cvtColor(src_img, dst_img_tmp, cv::COLOR_YUV2RGB_NV21); break;
        case VideoPixelFmt::BGR24: cv::cvtColor(src_img, dst_img_tmp, cv::COLOR_YUV2BGR_NV21); break;
        case VideoPixelFmt::RGBA:
        case VideoPixelFmt::BGRA:
        case VideoPixelFmt::ARGB:
        case VideoPixelFmt::ABGR: cv::cvtColor(src_img, dst_img_tmp, cv::COLOR_YUV2BGRA_NV21); break;
        default: cvt_ret = false; break;
      }
      break;
    }
    case VideoPixelFmt::RGB24: {  /*src rgb*/
      src_img = cv::Mat(height, stride, CV_8UC3, src_img_data);
      switch (dst_fmt) {
        case VideoPixelFmt::RGB24: dst_img_tmp = src_img; break;
        case VideoPixelFmt::BGR24: cv::cvtColor(src_img, dst_img_tmp, cv::COLOR_RGB2BGR); break;
        case VideoPixelFmt::RGBA:
        case VideoPixelFmt::BGRA:
        case VideoPixelFmt::ARGB:
        case VideoPixelFmt::ABGR: cv::cvtColor(src_img, dst_img_tmp, cv::COLOR_RGB2BGRA); break;
        default: cvt_ret = false; break;
      }
      break;
    }
    case VideoPixelFmt::BGR24: {  /*src bgr*/
      src_img = cv::Mat(height, stride, CV_8UC3, src_img_data);
      switch (dst_fmt) {
        case VideoPixelFmt::RGB24: cv::cvtColor(src_img, dst_img_tmp, cv::COLOR_BGR2RGB); break;
        case VideoPixelFmt::BGR24: dst_img_tmp = src_img; break;
        case VideoPixelFmt::RGBA:
        case VideoPixelFmt::BGRA:
        case VideoPixelFmt::ARGB:
        case VideoPixelFmt::ABGR: cv::cvtColor(src_img, dst_img_tmp, cv::COLOR_BGR2BGRA); break;
        default: cvt_ret = false; break;
      }
      break;
    }
    default: cvt_ret = false; break;
  }
  if (!cvt_ret) {
    return false;
  }
  switch (dst_fmt) {
    case VideoPixelFmt::RGBA: {
      cv::Mat rgba(dst_img_tmp.size(), dst_img_tmp.type());
      // bgra->rgba b:0->2 g:1->1 r:2->0 a:3->3
      int from_to[] = {0, 2, 1, 1, 2, 0, 3, 3};
      cv::mixChannels(&dst_img_tmp, 1, &rgba, 1, from_to, 4);
      dst_img_tmp = rgba;
      break;
    }
    case VideoPixelFmt::ARGB: {
      cv::Mat argb(dst_img_tmp.size(), dst_img_tmp.type());
      // bgra->argb b:0->3 g:1->2 r:2->1 a:3->0
      int from_to[] = {0, 3, 1, 2, 2, 1, 3, 0};
      cv::mixChannels(&dst_img_tmp, 1, &argb, 1, from_to, 4);
      dst_img_tmp = argb;
      break;
    }
    case VideoPixelFmt::ABGR: {
      cv::Mat abgr(dst_img_tmp.size(), dst_img_tmp.type());
      // bgra->abgr b:0->1 g:1->2 r:2->3 a:3->0
      int from_to[] = {0, 1, 1, 2, 2, 3, 3, 0};
      cv::mixChannels(&dst_img_tmp, 1, &abgr, 1, from_to, 4);
      dst_img_tmp = abgr;
      break;
    }
    default: break;
  }
  *dst_img = dst_img_tmp(cv::Rect(0, 0, width, height)).clone();
  return true;
}
