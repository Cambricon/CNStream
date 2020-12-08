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

#include <gtest/gtest.h>

#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "cnencode.hpp"
#include "cnstream_frame_va.hpp"
#include "encode.hpp"

namespace cnstream {

TEST(CNEncodeTest, InitFailedCase) {
  CNEncode::CNEncodeParam cnencode_param;
  cnencode_param.dst_width = 1920;
  cnencode_param.dst_height = 1080;
  cnencode_param.dst_stride = 1920;
  cnencode_param.dst_pix_fmt = BGR24;
  cnencode_param.encoder_type = "cpu";
  cnencode_param.codec_type = JPEG;
  cnencode_param.frame_rate = 25;
  cnencode_param.bit_rate = 0x100000 * 1024;
  cnencode_param.gop = 30;
  cnencode_param.stream_id = "0";
  cnencode_param.device_id = 0;
  cnencode_param.output_dir = "./encode_output";

  cnencode_param.dst_pix_fmt = NV12;
  CNEncode cpu_encode(cnencode_param);
  EXPECT_FALSE(cpu_encode.Init());

  {
    cnencode_param.encoder_type = "mlu";
    CNEncode mlu_encode(cnencode_param);
    EXPECT_TRUE(mlu_encode.Init());
    EXPECT_FALSE(mlu_encode.Init());
  }
  {
    cnencode_param.dst_pix_fmt = BGR24;
    CNEncode mlu_encode(cnencode_param);
    EXPECT_FALSE(mlu_encode.Init());
  }
  {
    cnencode_param.dst_pix_fmt = NV12;
    cnencode_param.device_id = -1;
    CNEncode mlu_encode(cnencode_param);
    EXPECT_FALSE(mlu_encode.Init());
  }
}

TEST(CNEncodeTest, UpdateFailedCase) {
  CNEncode::CNEncodeParam cnencode_param;
  cnencode_param.codec_type = H264;
  CNEncode encode(cnencode_param);
  cv::Mat img(100, 100, CV_8UC3, cv::Scalar(0, 0, 0));
  EXPECT_FALSE(encode.Update(img, 0));
  EXPECT_FALSE(encode.Update(nullptr, nullptr, 0, false));

  cnencode_param.dst_width = 1920;
  cnencode_param.dst_height = 1080;
  cnencode_param.dst_pix_fmt = NV12;
  cnencode_param.encoder_type = "mlu";
  cnencode_param.device_id = 0;
  cnencode_param.output_dir = "./encode_output";
  CNEncode mlu_encode(cnencode_param);

  EXPECT_TRUE(mlu_encode.Init());
  EXPECT_FALSE(mlu_encode.Update(nullptr, nullptr, 0, false));
  EXPECT_TRUE(mlu_encode.Update(nullptr, nullptr, 0, true));
}
}  // namespace cnstream
