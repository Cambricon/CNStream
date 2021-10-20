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
#include "profiler/module_profiler.hpp"
#include "profiler/pipeline_profiler.hpp"

#define STRIDE_ALIGN_FOR_SCALER_NV12 128
#define STRIDE_ALIGN 64

namespace cnstream {

static bool CvtI420ToNV12(const uint8_t *src_I420, uint8_t *dst_nv12, const int width,
    const int height, const int dst_stride) {
  if (!src_I420 || !dst_nv12 || width <= 0 || height <= 0 || dst_stride <= 0) {
    LOGW(SOURCE) << "CvtI420ToNV12 function, invalid paramters.";
    return false;
  }

  int pad_height = height % 2 ? height + 1 : height;
  int pad_width = width % 2 ? width + 1 : width;

  // memcpy y plane
  const uint8_t *src_y = src_I420;
  uint8_t *dst_y = dst_nv12;
  if (dst_stride == pad_width) {
    memcpy(dst_y, src_y, pad_width * height);
  } else {
    for (int row = 0; row < height; ++row) {
      const uint8_t *psrc_yt = src_y + row * pad_width;
      uint8_t *pdst_yt = dst_y + row * dst_stride;
      memcpy(pdst_yt, psrc_yt, pad_width);
    }
  }

  // memcpy u and v
  int src_u_stride = pad_width / 2;
  int src_v_stride = pad_width / 2;
  const uint8_t *src_u = src_I420 + pad_width * pad_height;
  const uint8_t *src_v = src_u + pad_width * pad_height / 4;
  uint8_t *dst_uv = dst_nv12 + dst_stride * height;

  for (int row = 0; row < height / 2; ++row) {
    const uint8_t *psrc_u = src_u + src_u_stride * row;
    const uint8_t *psrc_v = src_v + src_v_stride * row;
    uint8_t *pdst_uvt = dst_uv + dst_stride * row;

    for (int col = 0; col < src_u_stride; ++col) {
      pdst_uvt[col * 2] = psrc_u[col];
      pdst_uvt[col * 2 + 1] = psrc_v[col];
    }
  }

  return true;
}

static bool CvtNV21ToNV12(const uint8_t *src_nv21, uint8_t *dst_nv12, const int width,
    const int height, const int stride) {
  if (!src_nv21 || !dst_nv12 || width <= 0 || height <= 0 || stride <= 0) {
    LOGW(SOURCE) << "CvtNV21ToNV12 function, invalid paramters.";
    return false;
  }

  if (width % 2 != 0) {
    LOGW(SOURCE) << "CvtNV21ToNV12 do not support image with width%2 != 0";
    return false;
  }

  // memcpy y plane
  int src_y_size = width * height;
  const uint8_t *src_y = src_nv21;
  uint8_t *dst_y = dst_nv12;
  memcpy(dst_y, src_y, src_y_size);

  // swap u and v
  const uint8_t *src_vu = src_nv21 + width * height;
  uint8_t *dst_uv = dst_nv12 + stride * height;
  for (int i = 0; i < width * height / 2; i += 2) {
    dst_uv[i] = src_vu[i + 1];
    dst_uv[i + 1] = src_vu[i];
  }

  return true;
}

static bool CvtNV12ToNV12WithStride(const uint8_t *src_nv12, uint8_t *dst_nv12, const int width,
    const int height, const int stride) {
  if (!src_nv12 || !dst_nv12 || width <= 0 || height <= 0 || stride <= 0) {
    LOGW(SOURCE) << "CvtNV12ToNV12WithStride function, invalid paramters.";
    return false;
  }

  if (width % 2 != 0) {
    LOGW(SOURCE) << "CvtNV12ToNV12WithStride do not support image with width%2 != 0";
    return false;
  }

  // memcpy y plane
  int src_y_size = width * height;
  const uint8_t *src_y = src_nv12;
  uint8_t *dst_y = dst_nv12;
  memcpy(dst_y, src_y, src_y_size);

  // memcpy uv plane
  const uint8_t *src_uv = src_nv12 + width * height;
  uint8_t *dst_uv = dst_nv12 + stride * height;
  memcpy(dst_uv, src_uv, width * height / 2);
  return true;
}

std::shared_ptr<SourceHandler> RawImgMemHandler::Create(DataSource *module, const std::string &stream_id) {
  if (!module || stream_id.empty()) {
    LOGW(SOURCE) << "[RawImgMemHandler] create function, invalid paramters.";
    return nullptr;
  }
  std::shared_ptr<RawImgMemHandler> handler(new (std::nothrow) RawImgMemHandler(module, stream_id));
  return handler;
}

RawImgMemHandler::RawImgMemHandler(DataSource *module, const std::string &stream_id)
    : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) RawImgMemHandlerImpl(module, this);
}

RawImgMemHandler::~RawImgMemHandler() {
  if (impl_) {
    delete impl_, impl_ = nullptr;
  }
}

bool RawImgMemHandler::Open() {
  if (!this->module_) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "module_ null";
    return false;
  }
  if (!impl_) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "ESJpegMemHandler open failed, no memory left";
    return false;
  }

  if (stream_index_ == cnstream::INVALID_STREAM_IDX) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "invalid stream_idx";
    return false;
  }

  return impl_->Open();
}

void RawImgMemHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

int RawImgMemHandler::Write(const cv::Mat *mat, const uint64_t pts) {
  if (impl_) {
    return impl_->Write(mat, pts);
  }
  return -1;
}

int RawImgMemHandler::Write(const uint8_t *data, const int size, const uint64_t pts,
    const int w, const int h, const CNDataFormat pixel_fmt) {
  if (impl_) {
    return impl_->Write(data, size, pts, w, h, pixel_fmt);
  }
  return -1;
}

bool RawImgMemHandlerImpl::Open() {
  // updated with paramSet
  DataSource *source = dynamic_cast<DataSource *>(module_);
  param_ = source->GetSourceParam();
  return true;
}

void RawImgMemHandlerImpl::Close() {
  if (src_mat_) {
    delete src_mat_;
    src_mat_ = nullptr;
  }

  if (dst_mat_) {
    delete dst_mat_;
    dst_mat_ = nullptr;
  }
}

int RawImgMemHandlerImpl::Write(const cv::Mat *mat_data, const uint64_t pts) {
  if (eos_got_.load()) {
    LOGW(SOURCE) << "eos got, can not feed data any more.";
    return -1;
  }

  if (nullptr == mat_data) {
    LOGI(SOURCE) << "[" << stream_id_ << "]: " << "Got eos image data";
    SendFlowEos();
    eos_got_.store(true);
    return 0;
  } else if (mat_data && mat_data->data && (3 == mat_data->channels())
      && (CV_8UC3 == mat_data->type()) && mat_data->isContinuous()) {
    int width = mat_data->cols;
    int height = mat_data->rows;
    int size = mat_data->step * height;
    if (module_ && module_->GetProfiler()) {
      auto record_key = std::make_pair(stream_id_, pts);
      module_->GetProfiler()->RecordProcessStart(kPROCESS_PROFILER_NAME, record_key);
      if (module_->GetContainer() && module_->GetContainer()->GetProfiler()) {
        module_->GetContainer()->GetProfiler()->RecordInput(record_key);
      }
    }
    if (ProcessImage(mat_data->data, size, width, height, CNDataFormat::CN_PIXEL_FORMAT_BGR24, pts)) {
      return 0;
    } else {
      return -1;
    }
  }
  return -2;
}

int RawImgMemHandlerImpl::Write(const uint8_t *img_data, const int size, const uint64_t pts,
    const int w, const int h, const CNDataFormat pixel_fmt) {
  if (eos_got_.load()) {
    LOGW(SOURCE) << "[" << stream_id_ << "]: " << "eos got, can not feed data any more.";
    return -1;
  }

  if (nullptr == img_data && 0 == size) {
    LOGI(SOURCE) << "[" << stream_id_ << "]: " << "EOS reached in RawImgMemHandler";
    SendFlowEos();
    eos_got_.store(true);
    return 0;
  } else if (CheckRawImageParams(img_data, size, w, h, pixel_fmt)) {
    if (module_ && module_->GetProfiler()) {
      auto record_key = std::make_pair(stream_id_, pts);
      module_->GetProfiler()->RecordProcessStart(kPROCESS_PROFILER_NAME, record_key);
      if (module_->GetContainer() && module_->GetContainer()->GetProfiler()) {
        module_->GetContainer()->GetProfiler()->RecordInput(record_key);
      }
    }
    if (ProcessImage(img_data, size, w, h, pixel_fmt, pts)) {
      return 0;
    } else {
      return -1;
    }
  }

  return -2;
}

bool RawImgMemHandlerImpl::CheckRawImageParams(const uint8_t *data, const int size,
    const int width, const int height, const CNDataFormat pixel_fmt) {
  if (data && size > 0 && width > 0 && height > 0) {
    switch (pixel_fmt) {
      case CNDataFormat::CN_PIXEL_FORMAT_BGR24:
      case CNDataFormat::CN_PIXEL_FORMAT_RGB24:
        return size == width * height * 3;
      case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
      case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
        return size == width * height * 3 / 2;
      case CNDataFormat::CN_INVALID:
      default:
        LOGE(SOURCE) << "[RawImgMemHandlerImpl] CheckRawImageParams function, unsupported format.";
        return false;
    }
  }

  return false;
}

bool RawImgMemHandlerImpl::ProcessImage(const uint8_t *img_data, const int size, const int width,
      const int height, const CNDataFormat pixel_fmt, const uint64_t pts) {
  if (!img_data) {
    LOGE(SOURCE) << "[RawImgMemHandlerImpl] ProcessImage function img_data is nullptr.";
    return false;
  }
  if (frame_count_++ % param_.interval_ != 0) {
    return true;  // discard frames
  }
  int dst_stride = width;
  dst_stride = std::ceil(1.0 * dst_stride / STRIDE_ALIGN) * STRIDE_ALIGN;  // align stride to 64 by default
  if (param_.apply_stride_align_for_scaler_) {
    dst_stride = std::ceil(1.0 * dst_stride / STRIDE_ALIGN_FOR_SCALER_NV12) * STRIDE_ALIGN_FOR_SCALER_NV12;
  }

  size_t frame_size = dst_stride * height * 3 / 2;
  std::shared_ptr<void> sp_data = cnCpuMemAlloc(frame_size);
  if (!sp_data) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "Malloc dst nv12 data buffer failed, size:" << frame_size;
    return false;
  }

  // convert raw image data to NV12 data with stride
  uint8_t *sp_data_ = static_cast<uint8_t*>(sp_data.get());
  if (!CvtColorWithStride(img_data, size, width, height, pixel_fmt, sp_data_, dst_stride)) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: "
                 << "convert raw image to NV12 format with stride failed.";
    return false;
  }

  // create cnframedata and fill it
  std::shared_ptr<CNFrameInfo> data;
  while (true) {
    data = CreateFrameInfo();
    if (data != nullptr) break;
    std::this_thread::sleep_for(std::chrono::microseconds(5));
  }
  std::shared_ptr<CNDataFrame> dataframe(new (std::nothrow) CNDataFrame());
  if (!dataframe) {
    LOGE(SOURCE) << "[RawImgMemHandlerImpl] ProcessImage function, failed to create dataframe.";
    return false;
  }

  if (param_.output_type_ == OutputType::OUTPUT_MLU) {
    dataframe->ctx.dev_type = DevContext::DevType::MLU;
    dataframe->ctx.dev_id = param_.device_id_;
    dataframe->ctx.ddr_channel = -1;
  } else {
    dataframe->ctx.dev_type = DevContext::DevType::CPU;
    dataframe->ctx.dev_id = -1;
    dataframe->ctx.ddr_channel = -1;
  }

  dataframe->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12;
  dataframe->width = width;
  dataframe->height = height;
  dataframe->stride[0] = dst_stride;
  dataframe->stride[1] = dst_stride;

  if (param_.output_type_ == OutputType::OUTPUT_MLU) {
    dataframe->mlu_data = cnMluMemAlloc(frame_size, dataframe->ctx.dev_id);
    if (nullptr == dataframe->mlu_data) {
      LOGE(SOURCE) << "[" << stream_id_ << "]: "
                   << "RawImgMemHandlerImpl failed to alloc mlu memory, size: " << frame_size;
      return false;
    }

    cnrtRet_t ret = cnrtMemcpy(dataframe->mlu_data.get(), sp_data.get(), frame_size, CNRT_MEM_TRANS_DIR_HOST2DEV);
    if (ret != CNRT_RET_SUCCESS) {
      LOGE(SOURCE) << "[" << stream_id_ << "]: "
                   << "RawImgMemHandlerImpl failed to cnrtMemcpy";
      return false;
    }

    auto t = reinterpret_cast<uint8_t *>(dataframe->mlu_data.get());
    for (int i = 0; i < dataframe->GetPlanes(); ++i) {
      size_t plane_size = dataframe->GetPlaneBytes(i);
      CNSyncedMemory *CNSyncedMemory_ptr =
          new (std::nothrow) CNSyncedMemory(plane_size, dataframe->ctx.dev_id, dataframe->ctx.ddr_channel);
      LOGF_IF(SOURCE, nullptr == CNSyncedMemory_ptr) << "[" << stream_id_ << "]: "
                                                     << "RawImgMemHandlerImpl new CNSyncedMemory failed";
      dataframe->data[i].reset(CNSyncedMemory_ptr);
      dataframe->data[i]->SetMluData(t);
      t += plane_size;
    }
  } else if (param_.output_type_ == OutputType::OUTPUT_CPU) {
    dataframe->cpu_data = sp_data;
    auto t = reinterpret_cast<uint8_t *>(dataframe->cpu_data.get());
    for (int i = 0; i < dataframe->GetPlanes(); ++i) {
      size_t plane_size = dataframe->GetPlaneBytes(i);
      CNSyncedMemory *CNSyncedMemory_ptr = new (std::nothrow) CNSyncedMemory(plane_size);
      LOGF_IF(SOURCE, nullptr == CNSyncedMemory_ptr) << "[" << stream_id_ << "]: "
                                                     << "RawImgMemHandlerImpl new CNSyncedMemory failed";
      dataframe->data[i].reset(CNSyncedMemory_ptr);
      dataframe->data[i]->SetCpuData(t);
      t += plane_size;
    }
  } else {
    LOGE(SOURCE) << "[" << stream_id_ << "]: " << "DevContex::INVALID";
    return false;
  }

  dataframe->frame_id = frame_id_++;
  data->timestamp = pts;
  data->collection.Get<CNDataFramePtr>(kCNDataFrameTag) = dataframe;
  SendFrameInfo(data);
  return true;
}

bool RawImgMemHandlerImpl::PrepareConvertCtx(const uint8_t* data, const int size,
    const int width, const int height, const CNDataFormat pixel_fmt) {
  if (!data) {
    LOGW(SOURCE) << "[RawImgMemHandlerImpl] PrepareConvertCtx data is nullptr.";
    return false;
  }
  if (CNDataFormat::CN_PIXEL_FORMAT_BGR24 == pixel_fmt ||
      CNDataFormat::CN_PIXEL_FORMAT_RGB24 == pixel_fmt) {
    int pad_height = height % 2 ? height + 1 : height;
    int pad_width = width % 2 ? width + 1 : width;
    if (!(src_mat_ && dst_mat_ && src_mat_->cols == width && src_width_ == width &&
          src_height_ == height && src_fmt_ == pixel_fmt)) {
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
      src_fmt_ = pixel_fmt;
      src_width_ = width;
      src_height_ = height;
    }

    if (!src_mat_ || !dst_mat_) {
      LOGW(SOURCE) << "[RawImgMemHandlerImpl] PrepareConvertCtx, failed to create Mat.";
      return false;
    }

    if ((width % 2) || (height % 2)) {
      pad_height = height % 2 ? height + 1 : height;
      pad_width = width % 2 ? width + 1 : width;
      if (!(width % 2)) {
        memcpy(src_mat_->data, data, height * pad_width * 3);
      } else {
        for (int i = 0; i < height; ++i) {
          const uint8_t *psrc = data + i * width * 3;
          uint8_t *pdst = src_mat_->data + i * pad_width * 3;
          memcpy(pdst, psrc, width * 3);
        }
      }
    } else {
      memcpy(src_mat_->data, data, pad_height * pad_width * 3);
    }

    return true;
  }
  return false;
}

bool RawImgMemHandlerImpl::CvtColorWithStride(const uint8_t *data, const int size,
    const int width, const int height, const CNDataFormat pixel_fmt,
    uint8_t *dst_nv12_data, const int dst_stride) {
  if (!data || !dst_nv12_data || dst_stride <= 0) {
    return false;
  }

  switch (pixel_fmt) {
    case CNDataFormat::CN_PIXEL_FORMAT_BGR24:
      if (!PrepareConvertCtx(data, size, width, height, pixel_fmt)) return false;
      cv::cvtColor(*src_mat_, *dst_mat_, cv::COLOR_BGR2YUV_I420);
      return CvtI420ToNV12(dst_mat_->data, dst_nv12_data, width, height, dst_stride);
    case CNDataFormat::CN_PIXEL_FORMAT_RGB24:
      if (!PrepareConvertCtx(data, size, width, height, pixel_fmt)) return false;
      cv::cvtColor(*src_mat_, *dst_mat_, cv::COLOR_RGB2YUV_I420);
      return CvtI420ToNV12(dst_mat_->data, dst_nv12_data, width, height, dst_stride);
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
      return CvtNV21ToNV12(data, dst_nv12_data, width, height, dst_stride);
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
      return CvtNV12ToNV12WithStride(data, dst_nv12_data, width, height, dst_stride);
    case CNDataFormat::CN_INVALID:
    default:
      LOGE(SOURCE) << "raw image data with invalid pixel_fmt, not support.";
      break;
  }

  return false;
}

}  // namespace cnstream

#undef STRIDE_ALIGN_FOR_SCALER_NV12
#undef STRIDE_ALIGN
