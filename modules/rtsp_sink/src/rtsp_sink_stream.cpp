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

#include <libyuv.h>

#include "rtsp_sink_stream.hpp"

#include "cnstream_logging.hpp"
#include <memory>

#include "device/mlu_context.h"

#include "rtsp_sink.hpp"
#include "rtsp_stream_pipe.hpp"

#define MULTI_THREAD 1

namespace cnstream {

RtspSinkJoinStream::~RtspSinkJoinStream() { Close();}

bool RtspSinkJoinStream::Open(const RtspParam& rtsp_params) {
  if (rtsp_params.src_width < 1 || rtsp_params.src_height < 1 || rtsp_params.udp_port < 1 ||
      rtsp_params.http_port < 1) {
    return false;
  }
  if ("mosaic" == rtsp_params.view_mode) {
    if (!rtsp_params.resample) return false;
    is_mosaic_style_ = true;
    if (3 == rtsp_params.view_cols && 2 == rtsp_params.view_rows) {
      mosaic_win_width_ = rtsp_params.dst_width / rtsp_params.view_cols;
      mosaic_win_height_ = rtsp_params.dst_height / rtsp_params.view_cols;
    } else {
      mosaic_win_width_ = rtsp_params.dst_width / rtsp_params.view_cols;
      mosaic_win_height_ = rtsp_params.dst_height / rtsp_params.view_rows;
    }
  }

  rtsp_param_ = std::make_shared<RtspParam>();
  *rtsp_param_ = rtsp_params;

  running_ = true;
  LOGI(RTSP) << "==================================================================";
  if (rtsp_params.enc_type == FFMPEG)
    LOGI(RTSP) << "[Rtsp SINK] Use FFMPEG encoder";
  else if (rtsp_params.enc_type == MLU)
    LOGI(RTSP) << "[Rtsp SINK] Use MLU encoder";
  LOGI(RTSP) << "[Rtsp Sink] FrameRate: " << rtsp_params.frame_rate << "  GOP: " << rtsp_params.gop
            << "  KBPS: " << rtsp_params.kbps;
  LOGI(RTSP) << "==================================================================";

  ctx_ = StreamPipeCreate(rtsp_params);
  canvas_ = cv::Mat(rtsp_params.dst_height, rtsp_params.dst_width, CV_8UC3);           // for bgr24
  canvas_data_ = new uint8_t[rtsp_params.dst_height * rtsp_params.dst_width * 3 / 2];  // for nv21

  if (("cpu" == rtsp_params.preproc_type && MULTI_THREAD) || "bgr" == rtsp_params.color_mode) {
    if (rtsp_params.resample)
      refresh_thread_ = new std::thread(&RtspSinkJoinStream::RefreshLoop, this);
  }
  return true;
}

/**
 * nv21: true for nv21, false for nv12
 **/
static
void ResizeAndConvertByLibyuv(const cv::Mat& bgr, const int dst_width, const int dst_height, uint8_t* dst, bool nv21) {
  const int src_width = bgr.cols;
  const int src_height = bgr.rows;
  const int src_storage_height = bgr.rows & 1 ? bgr.rows + 1 : bgr.rows;
  const int src_nv12_size = src_width * src_storage_height * 3 / 2;

  // to I420
  uint8_t* i420_yplane = new uint8_t[src_width * src_storage_height];
  uint8_t* i420_uplane = new uint8_t[src_width * src_storage_height / 4];
  uint8_t* i420_vplane = new uint8_t[src_width * src_storage_height / 4];
  // RGB data in libyuv: BGRBGRBGRBGRBGRBGR.......
  libyuv::RGB24ToI420(bgr.data, src_width * 3,
                      i420_yplane, src_width,
                      i420_uplane, src_width >> 1,
                      i420_vplane, src_width >> 1,
                      src_width, src_height);

  bool should_resize = src_width != dst_width || src_height != dst_height;

  // to nv12
  uint8_t* src_nv12 = nullptr;
  if (should_resize) {
    src_nv12 = new uint8_t[src_nv12_size];
  } else {
    src_nv12 = dst;
  }
  uint8_t* src_yplane = src_nv12;
  uint8_t* src_uvplane = src_nv12 + src_width * src_storage_height;
  auto I420ToNV = &libyuv::I420ToNV12;
  if (nv21)
    I420ToNV = &libyuv::I420ToNV21;
  I420ToNV(i420_yplane, src_width,
           i420_uplane, src_width >> 1,
           i420_vplane, src_width >> 1,
           src_yplane, src_width,
           src_uvplane, src_width,
           src_width, src_height);

  delete[] i420_yplane;
  delete[] i420_uplane;
  delete[] i420_vplane;

  if (should_resize) {
    // split uv plane
    uint8_t* split_temp_uvplane = new uint8_t[src_width * src_storage_height / 2];
    uint8_t* split_temp_uplane = split_temp_uvplane;
    uint8_t* split_temp_vplane = split_temp_uvplane + src_width * src_storage_height / 4;
    libyuv::SplitUVPlane(src_uvplane, src_width,
                         split_temp_uplane, src_width >> 1,
                         split_temp_vplane, src_width >> 1,
                         src_width >> 1, src_height >> 1);

    // resize planes
    uint8_t* dst_nv_data = dst;
    uint8_t* dst_yplane = dst_nv_data;
    uint8_t* dst_uvplane = dst_nv_data + dst_width * dst_height;
    uint8_t* resize_temp_uvplane = new uint8_t[dst_width * dst_height / 2];
    uint8_t* resize_temp_uplane = resize_temp_uvplane;
    uint8_t* resize_temp_vplane = resize_temp_uvplane + dst_width * dst_height / 4;
    // libyuv::FilterMode::kFilterNone: fastest but lowest quality
    libyuv::ScalePlane(src_yplane, src_width, src_width, src_height,
                       dst_yplane, dst_width, dst_width, dst_height,
                       libyuv::FilterMode::kFilterNone);
    libyuv::ScalePlane(split_temp_uplane, src_width >> 1, src_width >> 1, src_height >> 1,
                       resize_temp_uplane, dst_width >> 1, dst_width >> 1, dst_height >> 1,
                       libyuv::FilterMode::kFilterNone);
    libyuv::ScalePlane(split_temp_vplane, src_width >> 1, src_width >> 1, src_height >> 1,
                       resize_temp_vplane, dst_width >> 1, dst_width >> 1, dst_height >> 1,
                       libyuv::FilterMode::kFilterNone);

    // merge u v planes
    libyuv::MergeUVPlane(resize_temp_uplane, dst_width >> 1,
                         resize_temp_vplane, dst_width >> 1,
                         dst_uvplane, dst_width, dst_width >> 1, dst_height >> 1);

    delete[] src_nv12;
    delete[] split_temp_uvplane;
    delete[] resize_temp_uvplane;
  }
}

void RtspSinkJoinStream::EncodeFrameBGR(const cv::Mat& bgr24, int64_t timestamp) {
  uint8_t* nv_data = new uint8_t[rtsp_param_->dst_width * rtsp_param_->dst_height * 3 / 2];
  bool nv21 = NV12 != rtsp_param_->color_format;
  ResizeAndConvertByLibyuv(bgr24, rtsp_param_->dst_width, rtsp_param_->dst_height, nv_data, nv21);
  StreamPipePutPacket(ctx_, nv_data, timestamp);
  delete[] nv_data;
}

void RtspSinkJoinStream::EncodeFrameYUV(uint8_t* s_data, int64_t timestamp) {
  StreamPipePutPacket(ctx_, s_data, timestamp);
}

/*
void RtspSinkJoinStream::EncodeFrameYUVs(void *y, void *uv, int64_t timestamp) {
  StreamPipePutPacketMlu(ctx_, y, uv, timestamp);
}
*/

void RtspSinkJoinStream::Close() {
  running_ = false;
  if (refresh_thread_ && refresh_thread_->joinable()) {
    refresh_thread_->join();
    delete refresh_thread_;
    refresh_thread_ = nullptr;
  }

  StreamPipeClose(ctx_);
  canvas_.release();
  delete[] canvas_data_;
  LOGI(RTSP) << "Release stream resources !!!" << std::endl;
}

bool RtspSinkJoinStream::UpdateBGR(cv::Mat image, int64_t timestamp, int channel_id) {
  canvas_lock_.lock();
  if (is_mosaic_style_ && channel_id >= 0) {
    if (3 == rtsp_param_->view_cols && 2 == rtsp_param_->view_rows) {
      int x = channel_id % rtsp_param_->view_cols * mosaic_win_width_;
      int y = channel_id / rtsp_param_->view_cols * mosaic_win_height_;
      switch (channel_id) {
        case 0:
          cv::resize(image, image, cv::Size(mosaic_win_width_ * 2, mosaic_win_height_ * 2), cv::INTER_CUBIC);
          if (rtsp_param_->resample)
            image.copyTo(canvas_(cv::Rect(x, y, mosaic_win_width_ * 2, mosaic_win_height_ * 2)));
          break;
        case 1:
          x += mosaic_win_width_;
          cv::resize(image, image, cv::Size(mosaic_win_width_, mosaic_win_height_), cv::INTER_CUBIC);
          if (rtsp_param_->resample)
            image.copyTo(canvas_(cv::Rect(x, y, mosaic_win_width_, mosaic_win_height_)));
          break;
        default:
          y += mosaic_win_height_;
          cv::resize(image, image, cv::Size(mosaic_win_width_, mosaic_win_height_), cv::INTER_CUBIC);
          if (rtsp_param_->resample)
            image.copyTo(canvas_(cv::Rect(x, y, mosaic_win_width_, mosaic_win_height_)));
          break;
      }
    } else {
      int x = channel_id % rtsp_param_->view_cols * mosaic_win_width_;
      int y = channel_id / rtsp_param_->view_cols * mosaic_win_height_;
      cv::resize(image, image, cv::Size(mosaic_win_width_, mosaic_win_height_), cv::INTER_CUBIC);
      image.copyTo(canvas_(cv::Rect(x, y, mosaic_win_width_, mosaic_win_height_)));
    }
  } else {
    if (rtsp_param_->resample)
      image.copyTo(canvas_);
  }

  if (!rtsp_param_->resample) {
    EncodeFrameBGR(image.clone(), timestamp);
  }

  canvas_lock_.unlock();
  return true;
}

bool RtspSinkJoinStream::UpdateYUV(uint8_t* image, int64_t timestamp) {
  canvas_lock_.lock();
  ResizeYuvNearest(image, canvas_data_);
  if (!MULTI_THREAD) {
    EncodeFrameYUV(canvas_data_, timestamp);
  }
  canvas_lock_.unlock();
  return true;
}

/*
bool RtspSinkJoinStream::UpdateYUVs(void *y, void *uv, int64_t timestamp) {
  canvas_lock_.lock();
  y_ptr_ = y;
  uv_ptr_ = uv;
  EncodeFrameYUVs(y_ptr_, uv_ptr_, timestamp);
  canvas_lock_.unlock();
  return true;
}
*/

void RtspSinkJoinStream::RefreshLoop() {
  edk::MluContext context;
  context.SetDeviceId(rtsp_param_->device_id);
  context.BindDevice();

  int64_t delay_us = 0;
  auto start = std::chrono::steady_clock::now();
  int64_t pts_us;
  uint32_t index = 0;

  while (running_) {
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::micro> dura = end - start;
    int64_t rt = delay_us - dura.count();
    if (rt > 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(rt));
      start = std::chrono::steady_clock::now();
    } else {
      start = end;
    }
    pts_us = index++ * 1e6 / rtsp_param_->frame_rate;

    if (ctx_) {
      canvas_lock_.lock();
      if ("cpu" == rtsp_param_->preproc_type) {
        if ("nv" == rtsp_param_->color_mode) {
          EncodeFrameYUV(canvas_data_, pts_us / 1000);
        } else if ("bgr" == rtsp_param_->color_mode) {
          EncodeFrameBGR(canvas_, pts_us / 1000);
        }
      }
      canvas_lock_.unlock();
      delay_us = index * 1e6 / rtsp_param_->frame_rate - pts_us;
    }
  }
}

void RtspSinkJoinStream::ResizeYuvNearest(uint8_t* src, uint8_t* dst) {
  if (rtsp_param_->src_width == rtsp_param_->dst_width && rtsp_param_->src_height == rtsp_param_->dst_height) {
    memcpy(dst, src, (rtsp_param_->dst_width * rtsp_param_->dst_height * 3 / 2) * sizeof(uint8_t));
    return;
  }
  int srcy, srcx, src_index;
  int xrIntFloat_16 = (rtsp_param_->src_width << 16) / rtsp_param_->dst_width + 1;
  int yrIntFloat_16 = (rtsp_param_->src_height << 16) / rtsp_param_->dst_height + 1;

  uint8_t* dst_uv = dst + rtsp_param_->dst_height * rtsp_param_->dst_width;
  uint8_t* src_uv = src + rtsp_param_->src_height * rtsp_param_->src_width;
  uint8_t* dst_uv_yScanline = nullptr;
  uint8_t* src_uv_yScanline = nullptr;
  uint8_t* dst_y_slice = dst;
  uint8_t* src_y_slice = nullptr;
  uint8_t* sp = nullptr;
  uint8_t* dp = nullptr;

  for (int y = 0; y < (rtsp_param_->dst_height & ~7); ++y) {
    srcy = (y * yrIntFloat_16) >> 16;
    src_y_slice = src + srcy * rtsp_param_->src_width;
    if (0 == (y & 1)) {
      dst_uv_yScanline = dst_uv + (y / 2) * rtsp_param_->dst_width;
      src_uv_yScanline = src_uv + (srcy / 2) * rtsp_param_->src_width;
    }
    for (int x = 0; x < (rtsp_param_->dst_width & ~7); ++x) {
      srcx = (x * xrIntFloat_16) >> 16;
      dst_y_slice[x] = src_y_slice[srcx];
      if ((y & 1) == 0) {    // y is even
        if ((x & 1) == 0) {  // x is even
          src_index = (srcx / 2) * 2;
          sp = dst_uv_yScanline + x;
          dp = src_uv_yScanline + src_index;
          *sp = *dp;
          ++sp;
          ++dp;
          *sp = *dp;
        }
      }
    }
    dst_y_slice += rtsp_param_->dst_width;
  }
}

}  // namespace cnstream
