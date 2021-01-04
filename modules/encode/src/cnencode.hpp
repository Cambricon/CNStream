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

#ifndef MODULES_CNENCODE_HPP_
#define MODULES_CNENCODE_HPP_

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

#include <fstream>
#include <memory>
#include <string>

#include "common.hpp"
#include "encode.hpp"
#include "easycodec/easy_encode.h"
#include "easycodec/vformat.h"

namespace cnstream {

class CNEncode {
 public:
  struct CNEncodeParam {
    uint32_t dst_height = 0;
    uint32_t dst_width = 0;
    uint32_t dst_stride = 0;
    CNPixelFormat dst_pix_fmt = BGR24;
    std::string encoder_type = "cpu";
    CNCodecType codec_type = H264;
    int frame_rate = 25;
    int bit_rate = 0x100000;
    int gop = 30;
    int device_id = -1;
    std::string stream_id = "";
    std::string output_dir = "";
  };
  explicit CNEncode(const CNEncodeParam &param);
  virtual ~CNEncode();

  bool Init();
  bool CreateMluEncoder();
  bool CreateCpuEncoder();
  bool Update(uint8_t* src_y, uint8_t* src_uv, int64_t timestamp, bool eos);
  bool Update(const cv::Mat src, int64_t timestamp);

  void EosCallback();
  void PacketCallback(const edk::CnPacket &packet);

  void SetModuleName(std::string name) { module_name_ = name; }

 private:
  bool CreateDir(std::string dir);
  // bool copy_frame_buffer_ = false;

  CNEncodeParam cnencode_param_;
  bool is_init_ = false;

  uint32_t output_frame_size_ = 0;

  uint32_t frame_count_ = 0;

  std::string output_file_name_ = "";
  size_t written_ = 0;
  std::ofstream file_;

  edk::PixelFmt picture_format_;
  std::unique_ptr<edk::EasyEncode> mlu_encoder_ = nullptr;

  cv::VideoWriter writer_;
  cv::Size size_;

  std::string module_name_ = "";
};  // class CNEncode

}  // namespace cnstream

#endif  // MODULES_CNENCODE_HPP_
