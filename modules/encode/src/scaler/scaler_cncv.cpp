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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <map>
#include <vector>

#ifdef HAVE_CNCV
#include <cncv.h>
#endif
#include <cnrt.h>
#include <device/mlu_context.h>

#include "cnstream_logging.hpp"

#include "scaler.hpp"

namespace cnstream {

using Buffer = Scaler::Buffer;
using ColorFormat = Scaler::ColorFormat;
using Rect = Scaler::Rect;

#ifdef HAVE_CNCV
#define SCALER_CNRT_CHECK(__EXPRESSION__)                         \
    do {                                                          \
      cnrtRet_t ret = (__EXPRESSION__);                           \
      LOGF_IF(ScalerCncv, CNRT_RET_SUCCESS != ret) << "Call [" <<   \
          #__EXPRESSION__ << "] failed, error code: " << ret;     \
    } while (0)

#define SCALER_CNCV_CHECK(__EXPRESSION__)                         \
  do {                                                            \
    cncvStatus_t ret = (__EXPRESSION__);                          \
    LOGF_IF(ScalerCncv, CNCV_STATUS_SUCCESS != ret) << "Call [" <<  \
        #__EXPRESSION__ << "] failed, error code: " << ret;       \
  } while (0)

class CncvContext {
 public:
  explicit CncvContext(int dev_id, ColorFormat src_fmt) {
    device_id_ = dev_id;
    src_fmt_ = src_fmt;
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(device_id_);
    mlu_ctx.BindDevice();
    SCALER_CNRT_CHECK(cnrtCreateQueue(&queue_));
    SCALER_CNCV_CHECK(cncvCreate(&handle_));
    SCALER_CNCV_CHECK(cncvSetQueue(handle_, queue_));
  }
  virtual bool Process(const Buffer &src, Buffer *dst, const Rect &crop) = 0;
  int GetDeviceId() { return device_id_; }
  ColorFormat GetSrcFmt() { return src_fmt_; }
  cnrtQueue_t GetQueue() { return queue_; }
  cncvHandle_t GetHandle() { return handle_; }
  cncvImageDescriptor& GetSrcImageDesc() { return src_desc_; }
  cncvImageDescriptor& GetDstImageDesc() { return dst_desc_; }
  virtual ~CncvContext() {
    if (handle_) SCALER_CNCV_CHECK(cncvDestroy(handle_));
    if (queue_) SCALER_CNRT_CHECK(cnrtDestroyQueue(queue_));
  }
  static cncvPixelFormat GetPixFormat(ColorFormat format) {
    static const cncvPixelFormat color_map[] = {
        CNCV_PIX_FMT_I420, CNCV_PIX_FMT_NV12, CNCV_PIX_FMT_NV21,
        CNCV_PIX_FMT_BGR, CNCV_PIX_FMT_RGB,
        CNCV_PIX_FMT_BGRA, CNCV_PIX_FMT_RGBA, CNCV_PIX_FMT_ABGR, CNCV_PIX_FMT_ARGB,
    };
    return color_map[format];
  }

 protected:
  int device_id_ = 0;
  cncvImageDescriptor src_desc_;
  cncvImageDescriptor dst_desc_;
  cnrtQueue_t queue_ = nullptr;
  cncvHandle_t handle_ = nullptr;
  ColorFormat src_fmt_;
};  // class CncvContext

class CncvResizeYuvContext : public CncvContext {
 public:
  CncvResizeYuvContext(int dev_id, ColorFormat src_fmt) : CncvContext(dev_id, src_fmt) {
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(device_id_);
    mlu_ctx.BindDevice();
    SCALER_CNRT_CHECK(cnrtMalloc(reinterpret_cast<void**>(&mlu_input_), 2 * sizeof(void*)));
    SCALER_CNRT_CHECK(cnrtMalloc(reinterpret_cast<void**>(&mlu_output_), 2 * sizeof(void*)));
    cpu_input_ = reinterpret_cast<void**>(malloc(2 * sizeof(void*)));
    cpu_output_ = reinterpret_cast<void**>(malloc(2 * sizeof(void*)));
  }
  bool Process(const Buffer &src, Buffer *dst, const Rect &crop) override;
  ~CncvResizeYuvContext() override {
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(device_id_);
    mlu_ctx.BindDevice();
    if (mlu_input_) SCALER_CNRT_CHECK(cnrtFree(mlu_input_));
    if (mlu_output_) SCALER_CNRT_CHECK(cnrtFree(mlu_output_));
    if (cpu_input_) free(cpu_input_);
    if (cpu_output_) free(cpu_output_);
    if (workspace_) SCALER_CNRT_CHECK(cnrtFree(workspace_));
  };

 private:
  cncvRect src_roi_;
  cncvRect dst_roi_;
  void** mlu_input_ = nullptr;
  void** mlu_output_ = nullptr;
  void** cpu_input_ = nullptr;
  void** cpu_output_ = nullptr;
  void* workspace_ = nullptr;
  size_t workspace_size_ = 0;
};  // class CncvResizeYuvContext

bool CncvResizeYuvContext::Process(const Buffer &src, Buffer *dst, const Rect &crop) {
  // LOGI(CncvResizeYuvContext) << "CncvResizeYuvContext::Process()";
  edk::MluContext mlu_ctx;
  mlu_ctx.SetDeviceId(device_id_);
  mlu_ctx.BindDevice();
  int batch_size = 1;
  src_desc_.width = src.width;
  src_desc_.height = src.height;
  src_desc_.pixel_fmt = GetPixFormat(src.color);
  src_desc_.stride[0] = src.stride[0];
  src_desc_.stride[1] = src.stride[1];
  src_desc_.depth = CNCV_DEPTH_8U;
  src_roi_.x = unsigned(crop.x) >= src.width ? 0 : crop.x;
  src_roi_.y = unsigned(crop.y) >= src.height ? 0 : crop.y;
  src_roi_.w = crop.w <= 0 ? (src.width - src_roi_.x) : crop.w;
  src_roi_.h = crop.h <= 0 ? (src.height - src_roi_.y) : crop.h;
  *(cpu_input_) = reinterpret_cast<void*>(src.data[0]);
  *(cpu_input_ + 1) = reinterpret_cast<void*>(src.data[1]);
  SCALER_CNRT_CHECK(cnrtMemcpy(mlu_input_, cpu_input_, 2 * sizeof(void*), CNRT_MEM_TRANS_DIR_HOST2DEV));

  dst_desc_.width = dst->width;
  dst_desc_.height = dst->height;
  dst_desc_.pixel_fmt = GetPixFormat(dst->color);
  dst_desc_.stride[0] = dst->stride[0];
  dst_desc_.stride[1] = dst->stride[1];
  dst_desc_.depth = CNCV_DEPTH_8U;
  dst_roi_.x = 0;
  dst_roi_.y = 0;
  dst_roi_.w = dst->width;
  dst_roi_.h = dst->height;
  *(cpu_output_) = reinterpret_cast<void*>(dst->data[0]);
  *(cpu_output_ + 1) = reinterpret_cast<void*>(dst->data[1]);
  SCALER_CNRT_CHECK(cnrtMemcpy(mlu_output_, cpu_output_, 2 * sizeof(void*), CNRT_MEM_TRANS_DIR_HOST2DEV));

  size_t required_workspace_size = 0;
  SCALER_CNCV_CHECK(cncvGetResizeYuvWorkspaceSize(batch_size, &src_desc_, &src_roi_, &dst_desc_, &dst_roi_,
                                                  &required_workspace_size));
  if (required_workspace_size != workspace_size_) {
    workspace_size_ = required_workspace_size;
    if (workspace_) SCALER_CNRT_CHECK(cnrtFree(workspace_));
    SCALER_CNRT_CHECK(cnrtMalloc(&(workspace_), required_workspace_size));
  }

  SCALER_CNCV_CHECK(cncvResizeYuv(handle_, batch_size, &(src_desc_), &(src_roi_), mlu_input_, &(dst_desc_), mlu_output_,
                                  &(dst_roi_), required_workspace_size, workspace_, CNCV_INTER_BILINEAR));
  SCALER_CNRT_CHECK(cnrtSyncQueue(queue_));
  return true;
}

class CncvResizeRgbxContext : public CncvContext {
 public:
  CncvResizeRgbxContext(int dev_id, ColorFormat src_fmt) : CncvContext(dev_id, src_fmt) {
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(device_id_);
    mlu_ctx.BindDevice();
    SCALER_CNRT_CHECK(cnrtMalloc(reinterpret_cast<void**>(&mlu_input_), sizeof(void*)));
    SCALER_CNRT_CHECK(cnrtMalloc(reinterpret_cast<void**>(&mlu_output_), sizeof(void*)));
    cpu_input_ = reinterpret_cast<void**>(malloc(sizeof(void*)));
    cpu_output_ = reinterpret_cast<void**>(malloc(sizeof(void*)));
  }
  bool Process(const Buffer &src, Buffer *dst, const Rect &crop) override;
  ~CncvResizeRgbxContext() override {
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(device_id_);
    mlu_ctx.BindDevice();
    if (mlu_input_) SCALER_CNRT_CHECK(cnrtFree(mlu_input_));
    if (mlu_output_) SCALER_CNRT_CHECK(cnrtFree(mlu_output_));
    if (cpu_input_) free(cpu_input_);
    if (cpu_output_) free(cpu_output_);
    if (workspace_) SCALER_CNRT_CHECK(cnrtFree(workspace_));
  };

 private:
  cncvRect src_roi_;
  cncvRect dst_roi_;
  void** mlu_input_ = nullptr;
  void** mlu_output_ = nullptr;
  void** cpu_input_ = nullptr;
  void** cpu_output_ = nullptr;
  void* workspace_ = nullptr;
  size_t workspace_size_ = 0;
};  // class CncvResizeRgbxContext

bool CncvResizeRgbxContext::Process(const Buffer &src, Buffer *dst, const Rect &crop) {
  // LOGI(CncvResizeRgbxContext) << "CncvResizeRgbxContext::Process()";
  edk::MluContext mlu_ctx;
  mlu_ctx.SetDeviceId(device_id_);
  mlu_ctx.BindDevice();
  int batch_size = 1;
  src_desc_.width = src.width;
  src_desc_.height = src.height;
  src_desc_.pixel_fmt = GetPixFormat(src.color);
  src_desc_.stride[0] = src.stride[0];
  src_desc_.depth = CNCV_DEPTH_8U;
  src_roi_.x = unsigned(crop.x) >= src.width ? 0 : crop.x;
  src_roi_.y = unsigned(crop.y) >= src.height ? 0 : crop.y;
  src_roi_.w = crop.w <= 0 ? (src.width - src_roi_.x) : crop.w;
  src_roi_.h = crop.h <= 0 ? (src.height - src_roi_.y) : crop.h;
  *(cpu_input_) = reinterpret_cast<void*>(src.data[0]);
  SCALER_CNRT_CHECK(cnrtMemcpy(mlu_input_, cpu_input_, sizeof(void*), CNRT_MEM_TRANS_DIR_HOST2DEV));

  dst_desc_.width = dst->width;
  dst_desc_.height = dst->height;
  dst_desc_.pixel_fmt = GetPixFormat(dst->color);
  dst_desc_.stride[0] = dst->stride[0];
  dst_desc_.depth = CNCV_DEPTH_8U;
  dst_roi_.x = 0;
  dst_roi_.y = 0;
  dst_roi_.w = dst->width;
  dst_roi_.h = dst->height;
  *(cpu_output_) = reinterpret_cast<void*>(dst->data[0]);
  SCALER_CNRT_CHECK(cnrtMemcpy(mlu_output_, cpu_output_, sizeof(void*), CNRT_MEM_TRANS_DIR_HOST2DEV));

  size_t required_workspace_size = 0;
  SCALER_CNCV_CHECK(cncvGetResizeRgbxWorkspaceSize(batch_size, &required_workspace_size));
  if (required_workspace_size != workspace_size_) {
    workspace_size_ = required_workspace_size;
    if (workspace_) SCALER_CNRT_CHECK(cnrtFree(workspace_));
    SCALER_CNRT_CHECK(cnrtMalloc(&(workspace_), required_workspace_size));
  }

  SCALER_CNCV_CHECK(cncvResizeRgbx(handle_, batch_size, src_desc_, &(src_roi_), mlu_input_, dst_desc_,
                                   &(dst_roi_), mlu_output_, required_workspace_size, workspace_, CNCV_INTER_BILINEAR));
  SCALER_CNRT_CHECK(cnrtSyncQueue(queue_));
  return true;
}

class CncvRgbxToYuvContext : public CncvContext {
 public:
  CncvRgbxToYuvContext(int dev_id, ColorFormat src_fmt) : CncvContext(dev_id, src_fmt) {
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(device_id_);
    mlu_ctx.BindDevice();
    SCALER_CNRT_CHECK(cnrtMalloc(reinterpret_cast<void**>(&mlu_output_), 2 * sizeof(void*)));
    cpu_output_ = reinterpret_cast<void**>(malloc(2 * sizeof(void*)));
  }
  bool Process(const Buffer &src, Buffer *dst, const Rect &crop) override;
  ~CncvRgbxToYuvContext() override {
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(device_id_);
    mlu_ctx.BindDevice();
    if (mlu_output_) SCALER_CNRT_CHECK(cnrtFree(mlu_output_));
    if (cpu_output_) free(cpu_output_);
  }

 private:
  cncvRect src_roi_;
  void** mlu_output_ = nullptr;
  void** cpu_output_ = nullptr;
};  // class CncvRgbxToYuvContext

bool CncvRgbxToYuvContext::Process(const Buffer &src, Buffer *dst, const Rect &crop) {
  // LOGI(CncvRgbxToYuvContext) << "CncvRgbxToYuvContext::Process()";
  edk::MluContext mlu_ctx;
  mlu_ctx.SetDeviceId(device_id_);
  mlu_ctx.BindDevice();
  src_desc_.width = src.width;
  src_desc_.height = src.height;
  src_desc_.pixel_fmt = GetPixFormat(src.color);
  src_desc_.stride[0] = src.stride[0];
  src_desc_.depth = CNCV_DEPTH_8U;
  src_roi_.x = unsigned(crop.x) >= src.width ? 0 : crop.x;
  src_roi_.y = unsigned(crop.y) >= src.height ? 0 : crop.y;
  src_roi_.w = crop.w <= 0 ? (src.width - src_roi_.x) : crop.w;
  src_roi_.h = crop.h <= 0 ? (src.height - src_roi_.y) : crop.h;
  void* mlu_input = reinterpret_cast<void*>(src.data[0]);

  dst_desc_.width = dst->width;
  dst_desc_.height = dst->height;
  dst_desc_.pixel_fmt = GetPixFormat(dst->color);
  dst_desc_.stride[0] = dst->stride[0];
  dst_desc_.stride[1] = dst->stride[1];
  dst_desc_.depth = CNCV_DEPTH_8U;
  *(cpu_output_) = reinterpret_cast<void*>(dst->data[0]);
  *(cpu_output_ + 1) = reinterpret_cast<void*>(dst->data[1]);
  SCALER_CNRT_CHECK(cnrtMemcpy(mlu_output_, cpu_output_, 2 * sizeof(void*), CNRT_MEM_TRANS_DIR_HOST2DEV));

  SCALER_CNCV_CHECK(cncvRgbxToYuv(handle_, src_desc_, src_roi_, mlu_input, dst_desc_, mlu_output_));
  SCALER_CNRT_CHECK(cnrtSyncQueue(queue_));
  return true;
}

class CncvResizeRgbxToYuvContext : public CncvContext {
 public:
  CncvResizeRgbxToYuvContext(int dev_id, ColorFormat src_fmt) : CncvContext(dev_id, src_fmt) {
    rgbx_to_yuv_ctx_ = std::make_shared<CncvRgbxToYuvContext>(dev_id, src_fmt);
    resize_rgbx_ctx_ = std::make_shared<CncvResizeRgbxContext>(dev_id, src_fmt);
  }
  bool Process(const Buffer &src, Buffer *dst, const Rect &crop) override;
  ~CncvResizeRgbxToYuvContext() override {
    if (rgbx_to_yuv_ctx_) {
      rgbx_to_yuv_ctx_.reset();
      rgbx_to_yuv_ctx_ = nullptr;
    }
    if (resize_rgbx_ctx_) {
      resize_rgbx_ctx_.reset();
      resize_rgbx_ctx_ = nullptr;
    }
  }

 private:
  std::shared_ptr<CncvRgbxToYuvContext> rgbx_to_yuv_ctx_ = nullptr;
  std::shared_ptr<CncvResizeRgbxContext> resize_rgbx_ctx_ = nullptr;
};  // class CncvResizeRgbxToYuvContext

bool CncvResizeRgbxToYuvContext::Process(const Buffer &src, Buffer *dst, const Rect &crop) {
  edk::MluContext mlu_ctx;
  mlu_ctx.SetDeviceId(device_id_);
  mlu_ctx.BindDevice();
  if (src.width != dst->width || src.height != dst->height) {
    Buffer resize_output;
    memset(&resize_output, 0, sizeof(Buffer));
    resize_output.color = src.color;
    resize_output.width = dst->width;
    resize_output.height = dst->height;
    if (src.color >= ColorFormat::BGRA) {
      resize_output.stride[0] = dst->stride[0] * 4;
    } else {
      resize_output.stride[0] = dst->stride[0] * 3;
    }
    resize_output.mlu_device_id = device_id_;

    SCALER_CNRT_CHECK(cnrtMalloc(reinterpret_cast<void**>(&resize_output.data[0]), resize_output.stride[0] *
                                resize_output.height));
    if (!resize_rgbx_ctx_->Process(src, &resize_output, crop)) {
      LOGE(CncvResizeRgbxToYuvContext) << "CncvResizeRgbxToYuvContext ResizeRgbx Failed";
    }
    if (!rgbx_to_yuv_ctx_->Process(resize_output, dst, crop)) {
      LOGE(CncvResizeRgbxToYuvContext) << "CncvResizeRgbxToYuvContext RgbxToYuv Failed";
    }
    SCALER_CNRT_CHECK(cnrtFree(resize_output.data[0]));
  } else {
    if (!rgbx_to_yuv_ctx_->Process(src, dst, crop)) {
      LOGE(CncvResizeRgbxToYuvContext) << "CncvResizeRgbxToYuvContext RgbxToYuv Failed";
    }
  }

  return true;
}
class ScalerCncv {
 public:
  struct Context {
    uint32_t hw_occupation = 0;
    std::mutex mtx;
    std::condition_variable cv;

    bool Process(const Buffer &src, Buffer *dst, const Rect &crop, int hw_instance_id);
    std::map<int, std::shared_ptr<CncvContext>> cncv_ctxs;
  };

  ~ScalerCncv() {
    LOGI(ScalerCncv) << "~ScalerCncv()";
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto &c : contexts_) {
      auto &ctx = c.second;
      std::unique_lock<std::mutex> lk(ctx.mtx);
      exit_ = true;
      lk.unlock();
      ctx.cv.notify_all();
      while (ctx.hw_occupation > 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));
      ctx.cncv_ctxs.clear();
    }
    contexts_.clear();
  }

  static ScalerCncv * GetInstance() {
    static std::once_flag of;
    std::call_once(of, [&]() { ScalerCncv::instance_.reset(new (std::nothrow) ScalerCncv()); });
    return instance_.get();
  }

  bool Process(const Buffer &src, Buffer *dst, const Rect &src_crop, const Rect &dst_crop);

 private:
  ScalerCncv() { }
  ScalerCncv(const ScalerCncv &) = delete;
  ScalerCncv &operator=(const ScalerCncv &) = delete;
  ScalerCncv(ScalerCncv &&) = delete;
  ScalerCncv &operator=(ScalerCncv &&) = delete;

  std::atomic<bool> exit_{false};
  static std::unique_ptr<ScalerCncv> instance_;
  std::mutex mtx_;
  std::map<int, Context> contexts_;
};  // ScalerCncv

std::unique_ptr<ScalerCncv> ScalerCncv::instance_ = nullptr;

bool ScalerCncv::Context::Process(const Buffer &src, Buffer *dst, const Rect &crop, int hw_instance_id) {
  std::shared_ptr<CncvContext> cncv_ctx = cncv_ctxs[hw_instance_id];
  return cncv_ctx->Process(src, dst, crop);
}

bool ScalerCncv::Process(const Buffer &src, Buffer *dst, const Rect &src_crop, const Rect &dst_crop) {
  if (src.mlu_device_id < 0 || dst->mlu_device_id < 0 || src.mlu_device_id != dst->mlu_device_id) {
    LOGE(ScalerCncv) << "Process() invalid mlu device id";
    return false;
  }

  if (src.color < ColorFormat::YUV_NV12 || dst->color < ColorFormat::YUV_NV12 ||
      (src.color < ColorFormat::BGR && src.color != dst->color) || dst_crop != Scaler::NullRect) {
    LOGE(ScalerCncv) << "Process() unsupported format";
    return false;
  }

  std::unique_lock<std::mutex> lk(mtx_);
  auto &ctx = contexts_[src.mlu_device_id];
  lk.unlock();

  int hw_instance_id;
  std::unique_lock<std::mutex> ctx_lk(ctx.mtx);
  ctx.cv.wait(ctx_lk, [&ctx, this] () { return exit_ || ctx.hw_occupation < 3; });
  if (exit_) return false;
  hw_instance_id = ((ctx.hw_occupation & 1) == 0) ? 0 : 1;
  ctx.hw_occupation |= 1 << hw_instance_id;
  if (ctx.cncv_ctxs.find(hw_instance_id) == ctx.cncv_ctxs.end()) {
    if (src.color <= ColorFormat::YUV_NV21) {
      ctx.cncv_ctxs[hw_instance_id] = std::make_shared<CncvResizeYuvContext>(src.mlu_device_id, src.color);
    } else {
      ctx.cncv_ctxs[hw_instance_id] = std::make_shared<CncvResizeRgbxToYuvContext>(src.mlu_device_id, src.color);
    }
  } else {
    ColorFormat org_src_fmt = ctx.cncv_ctxs[hw_instance_id]->GetSrcFmt();
    if (src.color <= ColorFormat::YUV_NV21 && !(org_src_fmt <= ColorFormat::YUV_NV21)) {
      ctx.cncv_ctxs[hw_instance_id] = std::make_shared<CncvResizeYuvContext>(src.mlu_device_id, src.color);
    } else if (src.color >= ColorFormat::BGR && !(org_src_fmt >= ColorFormat::BGR)) {
      ctx.cncv_ctxs[hw_instance_id] = std::make_shared<CncvResizeRgbxToYuvContext>(src.mlu_device_id, src.color);
    }
  }
  ctx_lk.unlock();

  bool ret = ctx.Process(src, dst, src_crop, hw_instance_id);

  ctx_lk.lock();
  ctx.hw_occupation &= ~(1 << hw_instance_id);
  ctx_lk.unlock();
  ctx.cv.notify_one();

  return ret;
}

bool CncvProcess(const Buffer *src, Buffer *dst, const Rect *src_crop, const Rect *dst_crop) {
  const Rect &sc = (src_crop) ? *src_crop : Scaler::NullRect;
  const Rect &dc = (dst_crop) ? *dst_crop : Scaler::NullRect;
  return ScalerCncv::GetInstance()->Process(*src, dst, sc, dc);
}
#else
bool CncvProcess(const Buffer *src, Buffer *dst, const Rect *src_crop, const Rect *dst_crop) {
  LOGE(ScalerCncv) << "CncvProcess() Please install CNCV.";
  return false;
}
#endif

}  // namespace cnstream
