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

#include "data_handler_rawimg.hpp"
#include <cnrt.h>

#include <condition_variable>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "cnstream_frame_va.hpp"
#include "device/mlu_context.h"
#include "perf_manager.hpp"

#define STRIDE_ALIGN_FOR_SCALER_NV12 128
#define STRIDE_ALIGN 64

namespace cnstream {

#if HAVE_OPENCV
static bool CvtI420ToNV12(uint8_t *src_I420, uint8_t *dst_nv12, int width, int height, int dst_stride) {
  if (!src_I420 || !dst_nv12 || width <= 0 || height <= 0 || dst_stride <= 0) {
    return false;
  }

  int pad_height = height % 2 ? height + 1 : height;
  int pad_width = width % 2 ? width + 1 : width;

  // memcpy y plane
  uint8_t *src_y = src_I420;
  uint8_t *dst_y = dst_nv12;
  if (dst_stride == pad_width) {
    memcpy(dst_y, src_y, pad_width * height);
  } else {
    for (int row = 0; row < height; ++row) {
      uint8_t *psrc_yt = src_y + row * pad_width;
      uint8_t *pdst_yt = dst_y + row * dst_stride;
      memcpy(pdst_yt, psrc_yt, pad_width);
    }
  }

  // memcpy u and v
  int src_u_stride = pad_width / 2;
  int src_v_stride = pad_width / 2;
  uint8_t *src_u = src_I420 + pad_width * pad_height;
  uint8_t *src_v = src_u + pad_width * pad_height / 4;
  uint8_t *dst_uv = dst_nv12 + dst_stride * height;

  for (int row = 0; row < height / 2; ++row) {
    uint8_t *psrc_u = src_u + src_u_stride * row;
    uint8_t *psrc_v = src_v + src_v_stride * row;
    uint8_t *pdst_uvt = dst_uv + dst_stride * row;

    for (int col = 0; col < src_u_stride; ++col) {
      pdst_uvt[col * 2] = psrc_u[col];
      pdst_uvt[col * 2 + 1] = psrc_v[col];
    }
  }

  return true;
}
#endif

static bool CvtNV21ToNV12(uint8_t *src_nv21, uint8_t *dst_nv12, int width, int height, int stride) {
  if (!src_nv21 || !dst_nv12 || width <= 0 || height <= 0 || stride <= 0) {
    return false;
  }

  if (width % 2 != 0) {
    LOG(WARNING) << "CvtNV21ToNV12 do not support image with width%2 != 0";
    return false;
  }

  // memcpy y plane
  int src_y_size = width * height;
  uint8_t *src_y = src_nv21;
  uint8_t *dst_y = dst_nv12;
  memcpy(dst_y, src_y, src_y_size);

  // swap u and v
  uint8_t *src_vu = src_nv21 + width * height;
  uint8_t *dst_uv = dst_nv12 + stride * height;
  for (int i = 0; i < width * height / 2; i += 2) {
    dst_uv[i] = src_vu[i + 1];
    dst_uv[i + 1] = src_vu[i];
  }

  return true;
}

static bool CvtNV12ToNV12WithStride(uint8_t *src_nv12, uint8_t *dst_nv12, int width, int height, int stride) {
  if (!src_nv12 || !dst_nv12 || width <= 0 || height <= 0 || stride <= 0) {
    return false;
  }

  if (width % 2 != 0) {
    LOG(WARNING) << "CvtNV12ToNV12WithStride do not support image with width%2 != 0";
    return false;
  }

  // memcpy y plane
  int src_y_size = width * height;
  uint8_t *src_y = src_nv12;
  uint8_t *dst_y = dst_nv12;
  memcpy(dst_y, src_y, src_y_size);

  // memcpy uv plane
  uint8_t *src_uv = src_nv12 + width * height;
  uint8_t *dst_uv = dst_nv12 + stride * height;
  memcpy(dst_uv, src_uv, width * height / 2);
  return true;
}

std::shared_ptr<SourceHandler> RawImgMemHandler::Create(DataSource *module, const std::string &stream_id) {
  if (!module || stream_id.empty()) {
    return nullptr;
  }
  std::shared_ptr<RawImgMemHandler> handler(new (std::nothrow) RawImgMemHandler(module, stream_id));
  return handler;
}

RawImgMemHandler::RawImgMemHandler(DataSource *module, const std::string &stream_id)
    : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) RawImgMemHandlerImpl(module, *this);
}

RawImgMemHandler::~RawImgMemHandler() {
  if (impl_) {
    delete impl_, impl_ = nullptr;
  }
}

bool RawImgMemHandler::Open() {
  if (!this->module_) {
    LOG(ERROR) << "module_ null";
    return false;
  }
  if (!impl_) {
    LOG(ERROR) << "impl_ null";
    return false;
  }

  if (stream_index_ == cnstream::INVALID_STREAM_IDX) {
    LOG(ERROR) << "invalid stream_idx";
    return false;
  }

  return impl_->Open();
}

void RawImgMemHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

#ifdef HAVE_OPENCV
int RawImgMemHandler::Write(cv::Mat *mat) {
  if (impl_) {
    return impl_->Write(mat);
  }
  return -1;
}
#endif

int RawImgMemHandler::Write(unsigned char *data, int size, int w, int h, CNDataFormat pixel_fmt) {
  if (impl_) {
    return impl_->Write(data, size, w, h, pixel_fmt);
  }
  return -1;
}

bool RawImgMemHandlerImpl::Open() {
  // updated with paramSet
  DataSource *source = dynamic_cast<DataSource *>(module_);
  param_ = source->GetSourceParam();
  this->interval_ = param_.interval_;

  SetPerfManager(source->GetPerfManager(stream_id_));
  SetThreadName(module_->GetName(), handler_.GetStreamUniqueIdx());

  size_t MaxSize = 60;  // FIXME
  img_pktq_ = new (std::nothrow) cnstream::BoundedQueue<ImagePacket>(MaxSize);
  if (!img_pktq_) {
    return false;
  }

  running_.store(1);
  thread_ = std::thread(&RawImgMemHandlerImpl::ProcessLoop, this);
  return true;
}

void RawImgMemHandlerImpl::Close() {
  if (running_.load()) {
    running_.store(0);
  }
  if (thread_.joinable()) {
    thread_.join();
  }

  std::lock_guard<std::mutex> lk(img_pktq_mutex_);
  if (img_pktq_) {
    ImagePacket in;
    while (img_pktq_->Size() > 0) {
      img_pktq_->Pop(1, in);
      if (in.data) delete[] in.data;
    }
    delete img_pktq_;
    img_pktq_ = nullptr;
  }

#ifdef HAVE_OPENCV
  if (src_mat_) {
    delete src_mat_;
    src_mat_ = nullptr;
  }

  if (dst_mat_) {
    delete dst_mat_;
    dst_mat_ = nullptr;
  }
#endif
}

#ifdef HAVE_OPENCV
int RawImgMemHandlerImpl::Write(cv::Mat *mat_data) {
  if (eos_got_.load()) {
    LOG(WARNING) << "eos got, can not feed data any more.";
    return -1;
  }

  std::lock_guard<std::mutex> lk(img_pktq_mutex_);
  if (img_pktq_) {
    ImagePacket img_pkt;
    if (nullptr == mat_data) {
      img_pkt.flags = ImagePacket::FLAG_EOS;
      img_pkt.data = nullptr;
      img_pkt.pts = pts_++;
    } else if (mat_data && mat_data->data && (3 == mat_data->channels())
        && (CV_8UC3 == mat_data->type()) && mat_data->isContinuous()) {
      img_pkt.pixel_fmt = CN_PIXEL_FORMAT_BGR24;
      img_pkt.width = mat_data->cols;
      img_pkt.height = mat_data->rows;
      img_pkt.size = mat_data->step * img_pkt.height;
      img_pkt.data = new (std::nothrow) uint8_t[img_pkt.size];
      memcpy(img_pkt.data, mat_data->data, img_pkt.size);
      img_pkt.pts = pts_++;
    } else {
      return -2;
    }

    int timeoutMs = 1000;
    while (running_.load()) {
      if (img_pktq_->Push(timeoutMs, img_pkt)) {
        return 0;
      }
    }
  }

  return -1;
}
#endif

int RawImgMemHandlerImpl::Write(unsigned char *img_data, int size, int w, int h, CNDataFormat pixel_fmt) {
  if (eos_got_.load()) {
    LOG(WARNING) << "eos got, can not feed data any more.";
    return -1;
  }

  std::lock_guard<std::mutex> lk(img_pktq_mutex_);
  if (img_pktq_) {
    ImagePacket img_pkt;
    if (nullptr == img_data && 0 == size) {
      img_pkt.flags = ImagePacket::FLAG_EOS;
      img_pkt.data = nullptr;
      img_pkt.pts = pts_++;
    } else if (CheckRawImageParams(img_data, size, w, h, pixel_fmt)) {
      img_pkt.pixel_fmt = pixel_fmt;
      img_pkt.width = w;
      img_pkt.height = h;
      img_pkt.size = size;
      img_pkt.data = new (std::nothrow) uint8_t[img_pkt.size];
      memcpy(img_pkt.data, img_data, img_pkt.size);
      img_pkt.pts = pts_++;
    } else {
      return -2;
    }

    int timeoutMs = 1000;
    while (running_.load()) {
      if (img_pktq_->Push(timeoutMs, img_pkt)) {
        return 0;
      }
    }
  }

  return -1;
}

bool RawImgMemHandlerImpl::CheckRawImageParams(unsigned char *data, int size, int w, int h, CNDataFormat pixel_fmt) {
  if (data && size > 0 && w > 0 && h > 0) {
    switch (pixel_fmt) {
      case CN_PIXEL_FORMAT_BGR24:
      case CN_PIXEL_FORMAT_RGB24:
        return size == w * h * 3;
      case CN_PIXEL_FORMAT_YUV420_NV21:
      case CN_PIXEL_FORMAT_YUV420_NV12:
        return size == w * h * 3 / 2;
      case CN_INVALID:
      default:
        return false;
    }
  }

  return false;
}

void RawImgMemHandlerImpl::ProcessLoop() {
  /*meet cnrt requirement*/
  if (param_.device_id_ >= 0) {
    try {
      edk::MluContext mlu_ctx;
      mlu_ctx.SetDeviceId(param_.device_id_);
      mlu_ctx.ConfigureForThisThread();
    } catch (edk::Exception &e) {
      if (nullptr != module_)
        module_->PostEvent(EVENT_ERROR, "stream_id " + stream_id_ + " failed to setup dev/channel.");
      return;
    }
  }

  while (running_.load()) {
    if (!Process()) {
      break;
    }
  }

  LOG(INFO) << "ProcessLoop Exit";
}

bool RawImgMemHandlerImpl::Process() {
  ImagePacket img_pkt;
  int timeoutMs = 1000;
  bool ret = this->img_pktq_->Pop(timeoutMs, img_pkt);
  if (!ret) {
    // continue.. not exit
    return true;
  }

  if (img_pkt.flags & ImagePacket::FLAG_EOS) {
    this->SendFlowEos();
    eos_got_.store(true);
    return false;
  }

  RecordStartTime(module_->GetName(), img_pkt.pts);

  return ProcessOneFrame(&img_pkt);
}

bool RawImgMemHandlerImpl::PrepareConvertCtx(ImagePacket *img_pkt) {
  if (!img_pkt) return false;
#ifdef HAVE_OPENCV
  if (CNDataFormat::CN_PIXEL_FORMAT_BGR24 == img_pkt->pixel_fmt ||
      CNDataFormat::CN_PIXEL_FORMAT_RGB24 == img_pkt->pixel_fmt) {
    int pad_height = img_pkt->height % 2 ? img_pkt->height + 1 : img_pkt->height;
    int pad_width = img_pkt->width % 2 ? img_pkt->width + 1 : img_pkt->width;
    if (!(src_mat_ && dst_mat_ && src_mat_->cols == img_pkt->width && src_width_ == img_pkt->width &&
          src_height_ == img_pkt->height && src_fmt_ == img_pkt->pixel_fmt)) {
      if (dst_mat_) {
        delete dst_mat_;
        dst_mat_ = nullptr;
      }

      if (src_mat_) {
        delete src_mat_;
        src_mat_ = nullptr;
      }

      src_mat_ = new (std::nothrow) cv::Mat(pad_height, pad_width, CV_8UC3);
      dst_mat_ = new (std::nothrow) cv::Mat(pad_height * 1.5, pad_width, CV_8UC1);
      src_fmt_ = img_pkt->pixel_fmt;
      src_width_ = img_pkt->width;
      src_height_ = img_pkt->height;
    }

    if (!src_mat_ || !dst_mat_) return false;

    if ((img_pkt->width % 2) || (img_pkt->height % 2)) {
      pad_height = img_pkt->height % 2 ? img_pkt->height + 1 : img_pkt->height;
      pad_width = img_pkt->width % 2 ? img_pkt->width + 1 : img_pkt->width;
      if (!(img_pkt->width % 2)) {
        memcpy(src_mat_->data, img_pkt->data, img_pkt->height * pad_width * 3);
      } else {
        for (int i = 0; i < img_pkt->height; ++i) {
          uint8_t *psrc = img_pkt->data + i * img_pkt->width * 3;
          uint8_t *pdst = src_mat_->data + i * pad_width * 3;
          memcpy(pdst, psrc, img_pkt->width * 3);
        }
      }
    } else {
      memcpy(src_mat_->data, img_pkt->data, pad_height * pad_width * 3);
    }

    return true;
  }

  return false;
#else
  return false;
#endif
}

bool RawImgMemHandlerImpl::CvtColorWithStride(ImagePacket *img_pkt, uint8_t *dst_nv12_data, int dst_stride) {
  if (!img_pkt || !dst_nv12_data || dst_stride <= 0) {
    return false;
  }

  int width = img_pkt->width;
  int height = img_pkt->height;

  switch (img_pkt->pixel_fmt) {
#ifdef HAVE_OPENCV
    case CNDataFormat::CN_PIXEL_FORMAT_BGR24:
      if (!PrepareConvertCtx(img_pkt)) return false;
      cv::cvtColor(*src_mat_, *dst_mat_, cv::COLOR_BGR2YUV_I420);
      return CvtI420ToNV12(dst_mat_->data, dst_nv12_data, width, height, dst_stride);
    case CNDataFormat::CN_PIXEL_FORMAT_RGB24:
      if (!PrepareConvertCtx(img_pkt)) return false;
      cv::cvtColor(*src_mat_, *dst_mat_, cv::COLOR_RGB2YUV_I420);
      return CvtI420ToNV12(dst_mat_->data, dst_nv12_data, width, height, dst_stride);
#else
    case CNDataFormat::CN_PIXEL_FORMAT_BGR24:
    case CNDataFormat::CN_PIXEL_FORMAT_RGB24:
      LOG(ERROR) << "opencv is not linked, can not support CN_PIXEL_FORMAT_BGR24 and CN_PIXEL_FORMAT_RGB24 format.";
      return false;
#endif
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
      return CvtNV21ToNV12(img_pkt->data, dst_nv12_data, width, height, dst_stride);
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
      return CvtNV12ToNV12WithStride(img_pkt->data, dst_nv12_data, width, height, dst_stride);
    case CNDataFormat::CN_INVALID:
    default:
      LOG(ERROR) << "raw image data with invalid pixel_fmt, not support.";
      break;
  }

  return false;
}

bool RawImgMemHandlerImpl::ProcessOneFrame(ImagePacket *img_pkt) {
  if (nullptr == img_pkt || nullptr == img_pkt->data) {
    return false;
  }

  int dst_stride = img_pkt->width;
  dst_stride = std::ceil(1.0 * dst_stride / STRIDE_ALIGN) * STRIDE_ALIGN;  // align stride to 64 by default
  if (param_.apply_stride_align_for_scaler_) {
    dst_stride = std::ceil(1.0 * dst_stride / STRIDE_ALIGN_FOR_SCALER_NV12) * STRIDE_ALIGN_FOR_SCALER_NV12;
  }

  size_t frame_size = dst_stride * img_pkt->height * 3 / 2;
  uint8_t *sp_data = new (std::nothrow) uint8_t[frame_size];
  if (!sp_data) {
    LOG(ERROR) << "Malloc dst nv12 data buffer failed, size:" << frame_size;
    return false;
  }

  // convert raw image data to NV12 data with stride
  if (!CvtColorWithStride(img_pkt, sp_data, dst_stride)) {
    LOG(ERROR) << "convert raw image to NV12 format with stride failed.";
    if (img_pkt->data) delete[] img_pkt->data;
    return false;
  }

  // create cnframedata and fill it
  std::shared_ptr<CNFrameInfo> data;
  while (1) {
    data = CreateFrameInfo();
    if (data != nullptr) break;
    std::this_thread::sleep_for(std::chrono::microseconds(5));
  }
  std::shared_ptr<CNDataFrame> dataframe(new (std::nothrow) CNDataFrame());
  if (!dataframe) return false;

  if (param_.output_type_ == OUTPUT_MLU) {
    dataframe->ctx.dev_type = DevContext::MLU;
    dataframe->ctx.dev_id = param_.device_id_;
    dataframe->ctx.ddr_channel = data->GetStreamIndex() % 4;  // FIXME
  } else {
    dataframe->ctx.dev_type = DevContext::CPU;
    dataframe->ctx.dev_id = -1;
    dataframe->ctx.ddr_channel = 0;
  }

  dataframe->fmt = CN_PIXEL_FORMAT_YUV420_NV12;
  dataframe->width = img_pkt->width;
  dataframe->height = img_pkt->height;
  dataframe->stride[0] = dst_stride;
  dataframe->stride[1] = dst_stride;

  if (param_.output_type_ == OUTPUT_MLU) {
    CALL_CNRT_BY_CONTEXT(cnrtMalloc(&dataframe->mlu_data, frame_size), dataframe->ctx.dev_id,
                         dataframe->ctx.ddr_channel);
    if (nullptr == dataframe->mlu_data) {
      LOG(ERROR) << "RawImgMemHandlerImpl failed to alloc mlu memory, size: " << frame_size;
      return false;
    }

    CALL_CNRT_BY_CONTEXT(cnrtMemcpy(dataframe->mlu_data, sp_data, frame_size, CNRT_MEM_TRANS_DIR_HOST2DEV),
                         dataframe->ctx.dev_id, dataframe->ctx.ddr_channel);

    auto t = reinterpret_cast<uint8_t *>(dataframe->mlu_data);
    for (int i = 0; i < dataframe->GetPlanes(); ++i) {
      size_t plane_size = dataframe->GetPlaneBytes(i);
      CNSyncedMemory *CNSyncedMemory_ptr =
          new (std::nothrow) CNSyncedMemory(plane_size, dataframe->ctx.dev_id, dataframe->ctx.ddr_channel);
      LOG_IF(FATAL, nullptr == CNSyncedMemory_ptr) << "RawImgMemHandlerImpl new CNSyncedMemory failed";
      dataframe->data[i].reset(CNSyncedMemory_ptr);
      dataframe->data[i]->SetMluData(t);
      t += plane_size;
    }
  } else if (param_.output_type_ == OUTPUT_CPU) {
    dataframe->cpu_data = sp_data;
    sp_data = nullptr;
    auto t = reinterpret_cast<uint8_t *>(dataframe->cpu_data);
    for (int i = 0; i < dataframe->GetPlanes(); ++i) {
      size_t plane_size = dataframe->GetPlaneBytes(i);
      CNSyncedMemory *CNSyncedMemory_ptr = new (std::nothrow) CNSyncedMemory(plane_size);
      LOG_IF(FATAL, nullptr == CNSyncedMemory_ptr) << "RawImgMemHandlerImpl new CNSyncedMemory failed";
      dataframe->data[i].reset(CNSyncedMemory_ptr);
      dataframe->data[i]->SetCpuData(t);
      t += plane_size;
    }
  } else {
    LOG(ERROR) << "DevContex::INVALID";
    return false;
  }

  dataframe->frame_id = frame_id_++;
  data->timestamp = img_pkt->pts;
  data->datas[CNDataFramePtrKey] = dataframe;
  if (sp_data) delete[] sp_data;
  if (img_pkt->data) delete[] img_pkt->data;
  SendFrameInfo(data);
  return true;
}

}  // namespace cnstream
