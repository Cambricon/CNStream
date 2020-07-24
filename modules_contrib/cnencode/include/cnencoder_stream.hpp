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

#ifndef MODULES_CNENCODER_STREAM_HPP_
#define MODULES_CNENCODER_STREAM_HPP_

#include <glog/logging.h>
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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "easycodec/easy_encode.h"
#include "easycodec/vformat.h"
#include "perf_manager.hpp"

class CNEncoderStream {
 public:
  enum PictureFormat {
    YUV420P = 0,
    RGB24,
    BGR24,
    NV21,
    NV12,
  };
  enum CodecType { H264 = 0, HEVC, MPEG4, JPEG };

  CNEncoderStream(int src_width, int src_height, int width, int height, float frame_rate, PictureFormat format,
                  int bit_rate, int gop_size, CodecType type, uint8_t channelIdx, uint32_t device_id,
                  std::string pre_type);
  virtual ~CNEncoderStream();

  bool Update(const cv::Mat &image, int64_t timestamp, bool eos);
  bool Update(uint8_t *image, int64_t timestamp, bool eos);
  void ResizeYuvNearest(uint8_t *src, uint8_t *dst);
  void Bgr2YUV420NV(const cv::Mat &bgr, PictureFormat ToFormat, uint8_t *nv_data);
  void Convert(const uint8_t *src_buffer, const size_t src_buffer_size, uint8_t *dst_buffer,
               const size_t dst_buffer_size);

  void EosCallback();
  void PacketCallback(const edk::CnPacket &packet);
  void SetPerfManager(std::shared_ptr<cnstream::PerfManager> manager) { perf_manager_ = manager; }
  void SetModuleName(std::string name) { module_name_ = name; }

 private:
  void RecordEndTime(int64_t pts);
  bool copy_frame_buffer_ = false;

  std::string pre_type_;
  std::mutex update_lock_;
  uint8_t *output_data = nullptr;

  uint32_t src_width_ = 0;
  uint32_t src_height_ = 0;
  uint32_t dst_width_ = 0;
  uint32_t dst_height_ = 0;
  uint32_t output_frame_size_;
  uint32_t frame_rate_num_;
  uint32_t frame_rate_den_;
  uint32_t gop_size_;
  uint32_t bit_rate_;
  uint32_t device_id_;

  uint8_t channelIdx_;
  uint32_t frame_count_ = 0;
  char output_file[256] = {0};
  size_t written;
  FILE *p_file = nullptr;

  CodecType type_;
  PictureFormat format_;

  cv::Mat canvas_;
  edk::PixelFmt picture_format_;
  edk::CodecType codec_type_;
  edk::EasyEncode *encoder_ = nullptr;

  SwsContext *swsctx_ = nullptr;
  AVFrame *src_pic_ = nullptr;
  AVFrame *dst_pic_ = nullptr;
  AVPixelFormat src_pix_fmt_ = AV_PIX_FMT_NONE;  // AV_PIX_FMT_BGR24
  AVPixelFormat dst_pix_fmt_ = AV_PIX_FMT_NONE;
  std::shared_ptr<cnstream::PerfManager> perf_manager_ = nullptr;
  std::string module_name_ = "";
};

#endif  // CNEncoder_STREAM_HPP_
