/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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

#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <utility>

#include "cnedk_platform.h"
#include "cnedk_buf_surface_util.hpp"
#include "cnrt.h"
#include "cnstream_logging.hpp"
#include "data_handler_image_frame.hpp"
#include "data_handler_util.hpp"
#include "data_source.hpp"
#include "platform_utils.hpp"
#include "profiler/module_profiler.hpp"
#include "profiler/pipeline_profiler.hpp"
#include "video_decoder.hpp"

namespace cnstream {

class ImageFrameHandlerImpl : public SourceRender, public IUserPool {
 public:
  explicit ImageFrameHandlerImpl(DataSource *module, const ImageFrameSourceParam &param, ImageFrameHandler *handler)
      : SourceRender(handler),
        module_(module),
        handle_param_(param),
        handler_(*handler),
        stream_id_(handler->GetStreamId()) {}
  ~ImageFrameHandlerImpl() {}

  bool Open();
  void Close();
  int Write(ImageFrame* frame);

 private:
  DataSource *module_ = nullptr;
  DataSourceParam param_;
  ImageFrameSourceParam handle_param_;
  ImageFrameHandler &handler_;
  std::string stream_id_;
  CnedkPlatformInfo platform_info_;
  CnedkBufSurfaceCreateParams create_params_;

 private:
  bool CheckParams(ImageFrame *frame);
  bool CreatePool(ImageFrame *frame);
  bool ProcessImage(ImageFrame *frame);
  bool ConvertImage(cnedk::BufSurfWrapperPtr input, cnedk::BufSurfWrapperPtr output);


  // IUserPool
  int CreatePool(CnedkBufSurfaceCreateParams *params, uint32_t block_count) override;
  void DestroyPool() override;
  void OnBufInfo(int width, int height, CnedkBufSurfaceColorFormat fmt) override;
  cnedk::BufSurfWrapperPtr GetBufSurface(int timeout_ms) override;

 private:
  std::mutex mutex_;

  cnedk::BufPool pool_;
  bool pool_created_ = false;
  std::mutex pool_mutex_;

  std::atomic<bool> first_write_{true};
  std::atomic<bool> eos_reached_{false};

  std::atomic<bool> generate_pts_{false};
  uint64_t fake_pts_ = 0;
  uint64_t pts_gap_ = 1;  // FIXME

  CnedkBufSurfaceColorFormat out_color_format_;

  ModuleProfiler *module_profiler_ = nullptr;
  PipelineProfiler *pipeline_profiler_ = nullptr;
};  // class ImageFrameHandlerImpl

std::shared_ptr<SourceHandler> CreateSource(DataSource *module, const std::string &stream_id,
                                            const ImageFrameSourceParam &param) {
  if (!module || stream_id.empty()) {
    LOGE(SOURCE) << "CreateSource(): Create ImageFrameHandler failed. source module and stream id must not be empty";
    return nullptr;
  }
  return std::make_shared<ImageFrameHandler>(module, stream_id, param);
}

int Write(std::shared_ptr<SourceHandler>handler, ImageFrame* frame) {
  auto handle = std::dynamic_pointer_cast<ImageFrameHandler>(handler);
  return handle->Write(frame);
}

ImageFrameHandler::ImageFrameHandler(DataSource *module, const std::string &stream_id,
                                     const ImageFrameSourceParam &param) : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) ImageFrameHandlerImpl(module, param, this);
}

ImageFrameHandler::~ImageFrameHandler() {
  if (impl_) {
    delete impl_, impl_ = nullptr;
  }
}

bool ImageFrameHandler::Open() {
  if (!this->module_) {
    LOGE(SOURCE) << "[ImageFrameHandler] Open(): [" << stream_id_ << "]: module_ is null";
    return false;
  }
  if (!impl_) {
    LOGE(SOURCE) << "[ImageFrameHandler] Open(): [" << stream_id_ << "]: no memory left";
    return false;
  }

  if (stream_index_ == kInvalidStreamIdx) {
    LOGE(SOURCE) << "[ImageFrameHandler] Open(): [" << stream_id_ << "]: invalid stream_idx";
    return false;
  }

  return impl_->Open();
}

void ImageFrameHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

int ImageFrameHandler::Write(ImageFrame *frame) {
  if (impl_) {
    return impl_->Write(frame);
  }
  return -1;
}

bool ImageFrameHandlerImpl::Open() {
  DataSource *source = dynamic_cast<DataSource *>(module_);
  if (nullptr == source) {
    LOGE(SOURCE) << "[ImageFrameHandlerImpl] Open(): [" << stream_id_ << "]: source module is null";
    return false;
  }
  param_ = source->GetSourceParam();
  cnrtSetDevice(param_.device_id);

  if (CnedkPlatformGetInfo(param_.device_id, &platform_info_) < 0) {
    LOGE(SOURCE) << "[ImageFrameHandlerImpl] Open(): Get platform information failed";
    return false;
  }

  CnedkTransformConfigParams config;
  memset(&config, 0, sizeof(config));
  config.compute_mode = CNEDK_TRANSFORM_COMPUTE_MLU;
  CnedkTransformSetSessionParams(&config);

  if (!module_profiler_) {
    if (module_) module_profiler_ = module_->GetProfiler();
    if (!pipeline_profiler_) {
      if (module_->GetContainer()) pipeline_profiler_ = module_->GetContainer()->GetProfiler();
    }
  }

  eos_reached_ = false;
  return true;
}

void ImageFrameHandlerImpl::Close() {
  LOGI(SOURCE) << "[ImageFrameHandlerImpl] Close(): this(" << this << ") Destroy pool";
  DestroyPool();
  first_write_ = true;
  eos_reached_ = false;
  fake_pts_ = 0;
}

int ImageFrameHandlerImpl::Write(ImageFrame* frame) {
  if (!frame || eos_reached_) {
    LOGW(SOURCE) << "[ImageFrameHandlerImpl] Write(): [" << stream_id_ << "]: write failed, eos got or frame is null.";
    return -1;
  }

  std::unique_lock<std::mutex> lk(mutex_);
  if (!frame->data) {
    LOGI(SOURCE) << "[ImageFrameHandlerImpl] Write(): Eos reached";
    this->SendFlowEos();
    eos_reached_ = true;
    return 0;
  }
  lk.unlock();

  if (!CheckParams(frame)) return -1;

  if (first_write_) {
    lk.lock();
    if (frame->data->GetColorFormat() == CNEDK_BUF_COLOR_FORMAT_NV12) {
      out_color_format_ = CNEDK_BUF_COLOR_FORMAT_NV12;
    } else if (frame->data->GetColorFormat() == CNEDK_BUF_COLOR_FORMAT_NV21) {
      out_color_format_ = CNEDK_BUF_COLOR_FORMAT_NV21;
    } else {
      out_color_format_ = CNEDK_BUF_COLOR_FORMAT_NV12;
    }
    lk.unlock();
    std::string platform(platform_info_.name);
    if (IsEdgePlatform(platform) ||
        (IsCloudPlatform(platform) && handle_param_.out_res.width > 0 && handle_param_.out_res.height > 0)) {
      CreatePool(frame);
    }
    lk.lock();
    first_write_ = false;
    lk.unlock();
  }
  lk.lock();
  if (!frame->has_pts) {
    generate_pts_ = true;
  }
  lk.unlock();

  if (ProcessImage(frame)) return 0;
  return -1;
}

bool ImageFrameHandlerImpl::ProcessImage(ImageFrame *frame) {
  if (frame_count_++ % param_.interval != 0) {
    return true;  // discard frames
  }

  cnedk::BufSurfWrapperPtr input_wrapper = frame->data;
  int64_t pts = generate_pts_ ? (fake_pts_ += pts_gap_) : input_wrapper->GetPts();

  if (module_profiler_) {
    auto record_key = std::make_pair(stream_id_, pts);
    module_profiler_->RecordProcessStart(kPROCESS_PROFILER_NAME, record_key);
    if (pipeline_profiler_) {
      pipeline_profiler_->RecordInput(record_key);
    }
  }

  OnBufInfo(input_wrapper->GetWidth(), input_wrapper->GetHeight(), out_color_format_);
  cnedk::BufSurfWrapperPtr output_wrapper = GetBufSurface(5000);  // FIXME

  ConvertImage(input_wrapper, output_wrapper);

  output_wrapper->SetPts(pts);

  std::shared_ptr<CNFrameInfo> data = this->CreateFrameInfo();
  if (!data) {
    LOGW(SOURCE) << "[ImageFrameHandlerImpl] ProcessImage(): failed to create FrameInfo.";
    return false;
  }

  data->timestamp = output_wrapper->GetPts();
  if (!output_wrapper->GetBufSurface()) {
    data->flags = static_cast<size_t>(CNFrameFlag::CN_FRAME_FLAG_INVALID);
    this->SendFrameInfo(data);
    return false;
  }
  int ret = SourceRender::Process(data, std::move(output_wrapper), frame_id_++, param_);
  if (ret < 0) {
    LOGE(SOURCE) << "[ImageFrameHandlerImpl] ProcessImage(): [" << stream_id_ << "]: Render frame failed";
    return false;
  }
  this->SendFrameInfo(data);

  return true;
}

bool ImageFrameHandlerImpl::CheckParams(ImageFrame *frame) {
  cnedk::BufSurfWrapperPtr buf = frame->data;
  if (!buf || !buf->GetBufSurface()) {
    LOGE(SOURCE) << "[ImageFrameHandlerImpl] CheckParams(): [" << stream_id_ << "]: Image BufSurface does not exist";
    return false;
  }

  if (buf->GetWidth() < 0 || buf->GetHeight() < 0) {
    LOGE(SOURCE) << "[ImageFrameHandlerImpl] CheckParams(): [" << stream_id_ << "]: The width or height is negative";
    return false;
  }

  switch (buf->GetColorFormat()) {
    case CNEDK_BUF_COLOR_FORMAT_NV12:
    case CNEDK_BUF_COLOR_FORMAT_NV21:
    case CNEDK_BUF_COLOR_FORMAT_RGB:
    case CNEDK_BUF_COLOR_FORMAT_BGR:
    case CNEDK_BUF_COLOR_FORMAT_ARGB:
    case CNEDK_BUF_COLOR_FORMAT_ABGR:
      break;
    default:
      LOGE(SOURCE) << "[ImageFrameHandlerImpl] CheckParams(): [" << stream_id_ << "]: Unsupported color format";
      return false;
  }
  return true;
}

bool ImageFrameHandlerImpl::CreatePool(ImageFrame *frame) {
  std::string platform(platform_info_.name);
  LOGI(SOURCE) << "[ImageFrameHandlerImpl] CreatePool()";
  CnedkBufSurfaceCreateParams create_params;
  memset(&create_params, 0, sizeof(create_params));
  create_params.device_id = param_.device_id;
  create_params.batch_size = 1;
  create_params.color_format = out_color_format_;

  if (handle_param_.out_res.width > 0 && handle_param_.out_res.height > 0) {
    create_params.width = handle_param_.out_res.width;
    create_params.height = handle_param_.out_res.height;
  } else {
    create_params.width = frame->data->GetWidth();
    create_params.height = frame->data->GetHeight();
  }

  if (IsEdgePlatform(platform)) {
    create_params.mem_type = CNEDK_BUF_MEM_VB_CACHED;
  } else {
    create_params.mem_type = CNEDK_BUF_MEM_DEVICE;
  }
  if (CreatePool(&create_params, param_.bufpool_size) < 0) {
    LOGE(SOURCE) << "[ImageFrameHandlerImpl] CreatePool(): Create pool failed";
    return false;
  }
  return true;
}

bool ImageFrameHandlerImpl::ConvertImage(cnedk::BufSurfWrapperPtr input, cnedk::BufSurfWrapperPtr output) {
  CnedkBufSurfaceColorFormat in_fmt = input->GetColorFormat();
  CnedkBufSurfaceColorFormat out_fmt = output->GetColorFormat();
  if ((in_fmt == CNEDK_BUF_COLOR_FORMAT_NV12 || in_fmt == CNEDK_BUF_COLOR_FORMAT_NV21) &&
      in_fmt != out_fmt) {
    LOGE(SOURCE) << "[ImageFrameHandlerImpl] ConvertImage(): YUV420sp nv12 <-> nv21 is not supported.";
    return false;
  }

  CnedkBufSurface* in_buf = input->GetBufSurface();
  CnedkBufSurface* out_buf = output->GetBufSurface();

  CnedkBufSurfaceMemType mem_type = in_buf->mem_type;
  if (mem_type == CNEDK_BUF_MEM_DEFAULT || mem_type == CNEDK_BUF_MEM_DEVICE ||
      mem_type == CNEDK_BUF_MEM_UNIFIED || mem_type == CNEDK_BUF_MEM_UNIFIED_CACHED ||
      mem_type == CNEDK_BUF_MEM_VB || mem_type == CNEDK_BUF_MEM_VB_CACHED) {
    if (input->GetWidth() == output->GetWidth() && input->GetHeight() == output->GetHeight() && in_fmt == out_fmt) {
      CnedkBufSurfaceCopy(input->GetBufSurface(), output->GetBufSurface());
    } else {
      CnedkBufSurfaceMemSet(out_buf, -1, -1, 0);
      CnedkTransformParams params;
      memset(&params, 0, sizeof(params));
      if (CnedkTransform(in_buf, out_buf, &params) < 0) {
        LOGE(SOURCE) << "[ImageFrameHandlerImpl] ConvertImage(): CnedkTransform failed";
        return false;
      }
    }
  } else {
    if (input->GetWidth() == output->GetWidth() && input->GetHeight() == output->GetHeight() && in_fmt == out_fmt) {
      CnedkBufSurfaceCopy(input->GetBufSurface(), output->GetBufSurface());
    } else {
      std::string platform(platform_info_.name);
      CnedkBufSurfaceCreateParams create_params;
      memset(&create_params, 0, sizeof(create_params));
      create_params.device_id = param_.device_id;
      create_params.batch_size = 1;
      create_params.width = input->GetWidth();
      create_params.height = input->GetHeight();
      create_params.color_format = in_fmt;
      if (IsCloudPlatform(platform)) {
        create_params.mem_type = CNEDK_BUF_MEM_DEVICE;
      } else if (IsEdgePlatform(platform)) {
        create_params.mem_type = CNEDK_BUF_MEM_UNIFIED;
      }

      CnedkBufSurface* tmp_surf;
      if (CnedkBufSurfaceCreate(&tmp_surf, &create_params) < 0) {
        LOGE(SOURCE) << "[ImageFrameHandlerImpl] ConvertImage(): CnedkBufSurfaceCreate failed";
        return false;
      }
      CnedkBufSurfaceCopy(input->GetBufSurface(), tmp_surf);

      CnedkBufSurfaceMemSet(out_buf, -1, -1, 0);
      CnedkTransformParams params;
      memset(&params, 0, sizeof(params));
      if (CnedkTransform(tmp_surf, out_buf, &params) < 0) {
        LOGE(SOURCE) << "[ImageFrameHandlerImpl] ConvertImage(): CnedkTransform failed";
        return false;
      }
      CnedkBufSurfaceDestroy(tmp_surf);
    }
  }
  return true;
}

// IUserPool
int ImageFrameHandlerImpl::CreatePool(CnedkBufSurfaceCreateParams *params, uint32_t block_count) {
  std::unique_lock<std::mutex> lk(mutex_);
  if (!pool_.CreatePool(params, block_count)) {
    pool_created_ = true;
    return 0;
  }
  LOGE(SOURCE) << "[ImageFrameHandlerImpl] CreatePool(): Create pool failed.";
  return -1;
}

void ImageFrameHandlerImpl::DestroyPool() {
  std::unique_lock<std::mutex> lk(mutex_);
  if (pool_created_) {
    pool_.DestroyPool(5000);
  }
}

void ImageFrameHandlerImpl::OnBufInfo(int width, int height, CnedkBufSurfaceColorFormat fmt) {
  if (!pool_created_) {
    memset(&create_params_, 0, sizeof(CnedkBufSurfaceCreateParams));
    create_params_.width = width;
    create_params_.height = height;
    create_params_.device_id = param_.device_id;
    create_params_.batch_size = 1;
    create_params_.color_format = fmt;
    create_params_.mem_type = CNEDK_BUF_MEM_DEVICE;
  }
}

cnedk::BufSurfWrapperPtr ImageFrameHandlerImpl::GetBufSurface(int timeout_ms) {
  std::string platform(platform_info_.name);
  if (IsEdgePlatform(platform)) {
    std::unique_lock<std::mutex> lk(mutex_);
    return pool_.GetBufSurfaceWrapper(timeout_ms);
  } else if (IsCloudPlatform(platform)) {
    if (pool_created_) {
      std::unique_lock<std::mutex> lk(mutex_);
      return pool_.GetBufSurfaceWrapper(timeout_ms);
    }

    CnedkBufSurface *surf = nullptr;
    if (CnedkBufSurfaceCreate(&surf, &create_params_) < 0) {
        LOGE(SOURCE) << "[ImageFrameHandlerImpl] GetBufSurface() Create BufSurface failed.";
      return nullptr;
    }
    return std::make_shared<cnedk::BufSurfaceWrapper>(surf);
  }
  return nullptr;
}

}  // namespace cnstream
