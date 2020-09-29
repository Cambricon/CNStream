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

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "encode.hpp"
#include "image_preproc.hpp"
#include "test_base.hpp"

namespace cnstream {

TEST(EncodePreprocTest, InitFailedCase) {
  ImagePreproc::ImagePreprocParam params;
  // MLU preproc is not supported yet
  {
    params.src_pix_fmt = NV12;
    params.preproc_type = "mlu";
    ImagePreproc preproc(params);
    EXPECT_FALSE(preproc.Init());
    params.device_id = 0;
    EXPECT_FALSE(preproc.Init());
    params.src_pix_fmt = BGR24;
    EXPECT_FALSE(preproc.Init());
  }
  // src w, src h, dst w, and dst h is 0
  {
    params.preproc_type = "cpu";
    params.use_ffmpeg = true;
    ImagePreproc preproc(params);
    EXPECT_FALSE(preproc.Init());
  }
  // init should only be called once
  {
    params.src_height = 720;
    params.src_width = 1280;
    params.dst_height = 720;
    params.dst_width = 1280;
    ImagePreproc preproc(params);
    EXPECT_TRUE(preproc.Init());
    EXPECT_FALSE(preproc.Init());
  }
  // YUV420P is not supported
  {
    params.use_ffmpeg = true;
    params.src_pix_fmt = YUV420P;
    ImagePreproc preproc(params);
    EXPECT_FALSE(preproc.Init());
  }
  {
    params.src_pix_fmt = BGR24;
    params.dst_pix_fmt = YUV420P;
    ImagePreproc preproc(params);
    EXPECT_FALSE(preproc.Init());
  }
}
TEST(EncodePreprocTest, SetSrcWidthHeightFailedCase) {
  ImagePreproc::ImagePreprocParam params;
  params.use_ffmpeg = true;
  {
    ImagePreproc preproc(params);
    EXPECT_FALSE(preproc.SetSrcWidthHeight(1280, 720));
  }
  {
    ImagePreproc preproc(params);
    params.dst_height = 720;
    params.dst_width = 1280;
    EXPECT_FALSE(preproc.SetSrcWidthHeight(0, 1080));
    EXPECT_FALSE(preproc.SetSrcWidthHeight(1920, 0));
    EXPECT_FALSE(preproc.SetSrcWidthHeight(0, 0));
  }
}

TEST(EncodePreprocTest, Bgr2BgrFailedCase) {
  ImagePreproc::ImagePreprocParam params;
  {
    cv::Mat src(0, 0, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Mat dst(0, 0, CV_8UC3, cv::Scalar(0, 0, 0));
    ImagePreproc preproc(params);
    preproc.Init();
    EXPECT_FALSE(preproc.Bgr2Bgr(src, dst));
  }

  {
    params.use_ffmpeg = true;
    params.dst_height = 720;
    params.dst_width = 680;
    params.src_height = 720;
    params.src_width = 680;
    cv::Mat src(720, 680, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Mat dst(720, 680, CV_8UC3, cv::Scalar(0, 0, 0));
    ImagePreproc preproc(params);
    // Init first
    EXPECT_FALSE(preproc.Bgr2Bgr(src, dst));
  }
}

TEST(EncodePreprocTest, Bgr2YuvFailedCase) {
  ImagePreproc::ImagePreprocParam params;
  params.use_ffmpeg = false;
  {
    cv::Mat src(720, 680, CV_8UC3, cv::Scalar(0, 0, 0));
    ImagePreproc preproc(params);
    EXPECT_FALSE(preproc.Bgr2Yuv(src, nullptr, nullptr));
    EXPECT_FALSE(preproc.Bgr2Yuv(src, nullptr));
  }

  params.dst_width = 1920;
  params.dst_height = 1080;
  uint8_t *dst = new uint8_t[params.dst_width * params.dst_height * 3 / 2];
  uint8_t *dst_y = dst;
  uint8_t *dst_uv = dst + params.dst_width * params.dst_height;

  {
    params.dst_pix_fmt = NV21;
    cv::Mat src(0, 0, CV_8UC3, cv::Scalar(0, 0, 0));
    ImagePreproc preproc(params);
    preproc.Init();
    EXPECT_FALSE(preproc.Bgr2Yuv(src, dst_y, dst_uv));
    EXPECT_FALSE(preproc.Bgr2Yuv(src, dst));
  }

  {
    cv::Mat src(720, 680, CV_8UC3, cv::Scalar(0, 0, 0));
    params.dst_width = 0;
    params.dst_height = 0;
    ImagePreproc preproc(params);
    preproc.Init();
    EXPECT_FALSE(preproc.Bgr2Yuv(src, dst_y, dst_uv));
    EXPECT_FALSE(preproc.Bgr2Yuv(src, dst));
  }
  {
    cv::Mat src(720, 680, CV_8UC3, cv::Scalar(0, 0, 0));
    params.dst_width = 1919;
    params.dst_height = 1080;
    ImagePreproc preproc(params);
    preproc.Init();
    EXPECT_FALSE(preproc.Bgr2Yuv(src, dst_y, dst_uv));
    EXPECT_FALSE(preproc.Bgr2Yuv(src, dst));
  }

  {
    cv::Mat src(721, 681, CV_8UC3, cv::Scalar(0, 0, 0));
    params.dst_width = 1920;
    params.dst_height = 1080;
    ImagePreproc preproc(params);
    preproc.Init();
    EXPECT_TRUE(preproc.Bgr2Yuv(src, dst_y, dst_uv));
    EXPECT_TRUE(preproc.Bgr2Yuv(src, dst));
  }

  params.use_ffmpeg = true;
  {
    cv::Mat src(720, 680, CV_8UC3, cv::Scalar(0, 0, 0));
    ImagePreproc preproc(params);
    EXPECT_FALSE(preproc.Bgr2Yuv(src, dst_y, dst_uv));
  }
  {
    cv::Mat src(720, 680, CV_8UC3, cv::Scalar(0, 0, 0));
    ImagePreproc preproc(params);
    preproc.Init();
    EXPECT_FALSE(preproc.Bgr2Yuv(src, dst_y, dst_uv));
  }
  // param src_width does not match mat src w
  {
    params.src_width = 681;
    params.src_height = 720;
    cv::Mat src(720, 680, CV_8UC3, cv::Scalar(0, 0, 0));
    ImagePreproc preproc(params);
    preproc.Init();
    EXPECT_FALSE(preproc.Bgr2Yuv(src, dst_y, dst_uv));
  }
  delete[] dst;
}

TEST(EncodePreprocTest, Yuv2YuvFailedCase) {
  ImagePreproc::ImagePreprocParam params;
  params.src_pix_fmt = NV12;
  params.dst_pix_fmt = NV12;
  params.src_height = 720;
  params.src_width = 640;
  params.dst_height = 1080;
  params.dst_width = 1920;
  uint8_t *dst = new uint8_t[params.dst_width * params.dst_height * 3 / 2];
  uint8_t *dst_y = dst;
  uint8_t *dst_uv = dst + params.dst_width * params.dst_height;
  uint8_t *src = new uint8_t[params.src_width * params.src_height * 3 / 2];
  uint8_t *src_y = src;
  uint8_t *src_uv = src + params.src_width * params.src_height;
  {
    ImagePreproc preproc(params);
    preproc.Init();
    EXPECT_FALSE(preproc.Yuv2Yuv(nullptr, nullptr, nullptr, nullptr));
    EXPECT_FALSE(preproc.Yuv2Yuv(nullptr, nullptr, nullptr));
  }

  // mlu preproc is not supported yet
  {
    params.preproc_type = "mlu";
    ImagePreproc preproc(params);
    preproc.Init();
    EXPECT_FALSE(preproc.Yuv2Yuv(src_y, src_uv, dst_y, dst_uv));
    EXPECT_FALSE(preproc.Yuv2Yuv(src_y, src_uv, dst));
  }

  // ffmpeg resize, height or width should not be odd.
  {
    params.use_ffmpeg = true;
    params.src_stride = 601;
    ImagePreproc preproc(params);
    preproc.Init();
    EXPECT_FALSE(preproc.Yuv2Yuv(src_y, src_uv, dst_y, dst_uv));
  }
  {
    params.use_ffmpeg = true;
    params.dst_stride = 1919;
    ImagePreproc preproc(params);
    preproc.Init();
    EXPECT_FALSE(preproc.Yuv2Yuv(src_y, src_uv, dst_y, dst_uv));
  }
  delete[] src;
  delete[] dst;
}

TEST(EncodePreprocTest, Yuv2Yuv) {
  ImagePreproc::ImagePreprocParam params;
  params.src_pix_fmt = NV12;
  params.dst_pix_fmt = NV12;
  params.src_height = 720;
  params.src_width = 640;
  params.dst_height = 1080;
  params.dst_width = 1920;
  uint8_t *dst = new uint8_t[params.dst_width * params.dst_height * 3 / 2];
  uint8_t *dst_y = dst;
  uint8_t *dst_uv = dst + params.dst_width * params.dst_height;
  uint8_t *src = new uint8_t[params.src_width * params.src_height * 3 / 2];
  uint8_t *src_y = src;
  uint8_t *src_uv = src + params.src_width * params.src_height;
  {
    params.preproc_type = "cpu";
    // stride != width
    params.dst_width = 1900;
    params.dst_stride = 1920;
    params.src_width = 600;
    params.src_stride = 640;

    ImagePreproc preproc(params);
    preproc.Init();
    EXPECT_TRUE(preproc.Yuv2Yuv(src_y, src_uv, dst_y, dst_uv));
  }
  {
    // src size == dst size
    params.dst_width = 600;
    params.dst_stride = 640;
    params.dst_height = 720;
    ImagePreproc preproc(params);
    preproc.Init();
    EXPECT_TRUE(preproc.Yuv2Yuv(src_y, src_uv, dst_y, dst_uv));
  }
  {
    // src size == dst size, but src stride != dst stride
    params.src_stride = 0;
    ImagePreproc preproc(params);
    preproc.Init();
    EXPECT_TRUE(preproc.Yuv2Yuv(src_y, src_uv, dst_y, dst_uv));
  }
  delete[] src;
  delete[] dst;
}
}  // namespace cnstream
