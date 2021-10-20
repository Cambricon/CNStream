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
#include <unordered_map>
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

class ScalerCncv {
 public:
  struct CncvContext {
    explicit CncvContext(int dev_id) {
      device_id = dev_id;
      edk::MluContext mlu_ctx;
      mlu_ctx.SetDeviceId(device_id);
      mlu_ctx.BindDevice();
      SCALER_CNRT_CHECK(cnrtMalloc(reinterpret_cast<void**>(&mlu_input), 2 * sizeof(void*)));
      SCALER_CNRT_CHECK(cnrtMalloc(reinterpret_cast<void**>(&mlu_output), 2 * sizeof(void*)));
      cpu_input = reinterpret_cast<void**>(malloc(2 * sizeof(void*)));
      cpu_output = reinterpret_cast<void**>(malloc(2 * sizeof(void*)));
      SCALER_CNRT_CHECK(cnrtCreateQueue(&queue));
      SCALER_CNCV_CHECK(cncvCreate(&handle));
      SCALER_CNCV_CHECK(cncvSetQueue(handle, queue));
    }
    ~CncvContext() {
      edk::MluContext mlu_ctx;
      mlu_ctx.SetDeviceId(device_id);
      mlu_ctx.BindDevice();
      if (mlu_input) SCALER_CNRT_CHECK(cnrtFree(mlu_input));
      if (mlu_output) SCALER_CNRT_CHECK(cnrtFree(mlu_output));
      if (cpu_input) free(cpu_input);
      if (cpu_output) free(cpu_output);
      if (handle) SCALER_CNCV_CHECK(cncvDestroy(handle));
      if (queue) SCALER_CNRT_CHECK(cnrtDestroyQueue(queue));
    }
    int device_id = 0;
    cncvImageDescriptor src_desc;
    cncvImageDescriptor dst_desc;
    cncvRect src_roi;
    cncvRect dst_roi;
    void** mlu_input = nullptr;
    void** mlu_output = nullptr;
    void** cpu_input = nullptr;
    void** cpu_output = nullptr;
    void* workspace = nullptr;
    size_t workspace_size = 0;

    cnrtQueue_t queue = nullptr;
    cncvHandle_t handle = nullptr;
  };
  struct Context {
    uint32_t hw_occupation = 0;
    std::mutex mtx;
    std::condition_variable cv;

    static cncvPixelFormat GetPixFormat(ColorFormat format) {
      static const cncvPixelFormat color_map[] = {
          CNCV_PIX_FMT_I420, CNCV_PIX_FMT_NV12, CNCV_PIX_FMT_NV21,
          CNCV_PIX_FMT_BGR, CNCV_PIX_FMT_RGB,
          CNCV_PIX_FMT_BGRA, CNCV_PIX_FMT_RGBA, CNCV_PIX_FMT_ABGR, CNCV_PIX_FMT_ARGB,
      };
      return color_map[format];
    }

    bool Process(const Buffer &src, Buffer *dst, const Rect &crop, int hw_instance_id);
    std::unordered_map<int, std::shared_ptr<CncvContext>> cncv_ctxs;
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
  ScalerCncv() { cnrtInit(0); }
  ScalerCncv(const ScalerCncv &) = delete;
  ScalerCncv &operator=(const ScalerCncv &) = delete;
  ScalerCncv(ScalerCncv &&) = delete;
  ScalerCncv &operator=(ScalerCncv &&) = delete;

  std::atomic<bool> exit_{false};
  static std::unique_ptr<ScalerCncv> instance_;
  std::mutex mtx_;
  std::unordered_map<int, Context> contexts_;
};  // ScalerCncv

std::unique_ptr<ScalerCncv> ScalerCncv::instance_ = nullptr;

bool ScalerCncv::Context::Process(const Buffer &src, Buffer *dst, const Rect &crop, int hw_instance_id) {
  std::shared_ptr<CncvContext> cncv_ctx = cncv_ctxs[hw_instance_id];
  edk::MluContext mlu_ctx;
  mlu_ctx.SetDeviceId(cncv_ctx->device_id);
  mlu_ctx.BindDevice();
  int batch_size = 1;
  cncv_ctx->src_desc.width = src.width;
  cncv_ctx->src_desc.height = src.height;
  cncv_ctx->src_desc.pixel_fmt = GetPixFormat(src.color);
  cncv_ctx->src_desc.stride[0] = src.stride[0];
  cncv_ctx->src_desc.stride[1] = src.stride[1];
  cncv_ctx->src_desc.depth = CNCV_DEPTH_8U;
  cncv_ctx->src_roi.x = unsigned(crop.x) >= src.width ? 0 : crop.x;
  cncv_ctx->src_roi.y = unsigned(crop.y) >= src.height ? 0 : crop.y;
  cncv_ctx->src_roi.w = crop.w <= 0 ? (src.width - cncv_ctx->src_roi.x) : crop.w;
  cncv_ctx->src_roi.h = crop.h <= 0 ? (src.height - cncv_ctx->src_roi.y) : crop.h;
  *(cncv_ctx->cpu_input) = reinterpret_cast<void*>(src.data[0]);
  *(cncv_ctx->cpu_input + 1) = reinterpret_cast<void*>(src.data[1]);
  SCALER_CNRT_CHECK(cnrtMemcpy(cncv_ctx->mlu_input, cncv_ctx->cpu_input, 2 * sizeof(void*),
                               CNRT_MEM_TRANS_DIR_HOST2DEV));

  cncv_ctx->dst_desc.width = dst->width;
  cncv_ctx->dst_desc.height = dst->height;
  cncv_ctx->dst_desc.pixel_fmt = GetPixFormat(dst->color);
  cncv_ctx->dst_desc.stride[0] = dst->stride[0];
  cncv_ctx->dst_desc.stride[1] = dst->stride[1];
  cncv_ctx->dst_desc.depth = CNCV_DEPTH_8U;
  cncv_ctx->dst_roi.x = 0;
  cncv_ctx->dst_roi.y = 0;
  cncv_ctx->dst_roi.w = dst->width;
  cncv_ctx->dst_roi.h = dst->height;
  *(cncv_ctx->cpu_output) = reinterpret_cast<void*>(dst->data[0]);
  *(cncv_ctx->cpu_output + 1) = reinterpret_cast<void*>(dst->data[1]);
  SCALER_CNRT_CHECK(cnrtMemcpy(cncv_ctx->mlu_output, cncv_ctx->cpu_output, 2 * sizeof(void*),
                               CNRT_MEM_TRANS_DIR_HOST2DEV));

  size_t required_workspace_size = 0;
  SCALER_CNCV_CHECK(
      cncvGetResizeYuvWorkspaceSize(batch_size, &cncv_ctx->src_desc, &cncv_ctx->src_roi,
                                   &cncv_ctx->dst_desc, &cncv_ctx->dst_roi, &required_workspace_size));
  if (required_workspace_size != cncv_ctx->workspace_size) {
    cncv_ctx->workspace_size = required_workspace_size;
    if (cncv_ctx->workspace) SCALER_CNRT_CHECK(cnrtFree(cncv_ctx->workspace));
    SCALER_CNRT_CHECK(cnrtMalloc(&(cncv_ctx->workspace), required_workspace_size));
  }

  SCALER_CNCV_CHECK(
      cncvResizeYuv(cncv_ctx->handle, batch_size, &(cncv_ctx->src_desc), &(cncv_ctx->src_roi), cncv_ctx->mlu_input,
                    &(cncv_ctx->dst_desc), cncv_ctx->mlu_output, &(cncv_ctx->dst_roi),
                    required_workspace_size, cncv_ctx->workspace, CNCV_INTER_BILINEAR));
  SCALER_CNRT_CHECK(cnrtSyncQueue(cncv_ctx->queue));

  return true;
}

bool ScalerCncv::Process(const Buffer &src, Buffer *dst, const Rect &src_crop, const Rect &dst_crop) {
  if (src.mlu_device_id < 0 || dst->mlu_device_id < 0 || src.mlu_device_id != dst->mlu_device_id) {
    LOGE(ScalerCncv) << "Process() invalid mlu device id";
    return false;
  }

  if (!(src.color == ColorFormat::YUV_NV12 || src.color == ColorFormat::YUV_NV21) ||
      dst->color != src.color || dst_crop != Scaler::NullRect) {
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
    ctx.cncv_ctxs[hw_instance_id] = std::make_shared<CncvContext>(src.mlu_device_id);
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
