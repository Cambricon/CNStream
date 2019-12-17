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

#include <cnrt.h>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "easybang/resize_and_colorcvt.h"
#include "easyinfer/easy_infer.h"
#include "easyinfer/mlu_context.h"
#include "easyinfer/mlu_memory_op.h"
#include "easyinfer/model_loader.h"

#include "../../modules/inference/src/infer_thread_pool.hpp"
#include "cnstream_eventbus.hpp"
#include "cnstream_frame.hpp"
#include "cnstream_pipeline.hpp"
#include "cnstream_timer.hpp"
#include "inference_ex.hpp"
#include "mtcnn_process.hpp"
#include "postprocess/postproc.hpp"
#include "preprocess/preproc.hpp"

#define ROUND_UP(addr, boundary) (((u32_t)(addr) + (boundary)-1) & ~((boundary)-1))
#define TIMEOUT_PRINT_INTERVAL 100

namespace cnstream {

static bool IsYAndUVSplit(const std::shared_ptr<edk::ModelLoader>& model) {
  auto shapes = model->InputShapes();
  return shapes.size() == 2 && shapes[0].c == 1 && shapes[0].c == shapes[1].c &&
         1.0 * shapes[0].hw() / shapes[1].hw() == 2.0;
}

class TimeoutOperator {
 public:
  TimeoutOperator() {
    plk_ = std::make_shared<std::unique_lock<std::mutex>>(mtx_);
    plk_->unlock();
    handle_th_ = std::thread(&TimeoutOperator::HandleFunc, this);
  }

  ~TimeoutOperator() {
    std::unique_lock<std::mutex> lk(mtx_);
    state_ = STATE_EXIT;
    cond_.notify_all();
    lk.unlock();
    if (handle_th_.joinable()) handle_th_.join();
  }

  /* lock before do this */
  void SetTimeout(float timeout) { timeout_ = timeout; }

  void LockOperator() { plk_->lock(); }

  void UnlockOperator() { plk_->unlock(); }

  /* lock before do this */
  void Reset(const std::function<void()>& func) {
    if (STATE_EXIT == state_) {
      LOG(WARNING) << "Timeout Operator has been exit.";
      return;
    }
    func_ = func;
    if (func) {
      if (STATE_NO_FUNC == state_) {
        state_ = STATE_DO;
      } else if (STATE_DO == state_ || STATE_RESET == state_) {
        state_ = STATE_RESET;
      } else {
        LOG(FATAL) << "Unexpected logic.";
      }
    } else {
      state_ = STATE_NO_FUNC;
    }
    cond_.notify_one();
  }

 private:
  enum { STATE_NO_FUNC = 0, STATE_RESET, STATE_DO, STATE_EXIT } state_ = STATE_NO_FUNC;
  void HandleFunc() {
    std::unique_lock<std::mutex> lk(mtx_);
    while (state_ != STATE_EXIT) {
      cond_.wait(lk, [this]() -> bool { return state_ == STATE_EXIT || state_ != STATE_NO_FUNC; });

      auto wait_time = std::chrono::nanoseconds(static_cast<uint64_t>(timeout_ * 1e6));
      cond_.wait_for(lk, wait_time, [this]() -> bool {
        return state_ == STATE_EXIT || state_ == STATE_NO_FUNC || state_ == STATE_RESET;
      });

      if (STATE_RESET == state_) {
        state_ = STATE_DO;
        continue;
      } else if (STATE_NO_FUNC == state_) {
        continue;
      } else if (STATE_EXIT == state_) {
        break;
      } else if (STATE_DO == state_) {
        CHECK_NE(static_cast<bool>(func_), false) << "Bad logic: state_ is STATE_DO, but function is NULL.";
        func_();
        timeout_print_cnt_++;
        if (timeout_print_cnt_ == TIMEOUT_PRINT_INTERVAL) {
          timeout_print_cnt_ = 0;
          LOG(INFO) << "Batching timeout. The trigger frequency of timeout processing can be reduced by"
                       " increasing the timeout time(see batching_timeout parameter of the inferencer module). If the"
                       " decoder memory is reused, the trigger frequency of timeout processing can also be reduced by"
                       " increasing the number of cache blocks output by the decoder(see output_buf_number parameter of"
                       " the source module). ";
        }
        func_ = NULL;  // unbind resources.
        state_ = STATE_NO_FUNC;
      } else {
        LOG(FATAL) << "Unexpected logic.";
        break;
      }
    }
  }

  std::mutex mtx_;
  std::condition_variable cond_;
  std::shared_ptr<std::unique_lock<std::mutex>> plk_;
  std::function<void()> func_;
  std::thread handle_th_;
  float timeout_ = 0;
  uint32_t timeout_print_cnt_ = 0;
};  // class TimeoutOperator

using TimeoutOperatorSptr = std::shared_ptr<TimeoutOperator>;

/*********************************************************************************
 * @brief Inferencer thread context
 *********************************************************************************/
struct InferContext {
  TimeoutOperatorSptr timeout_handler = NULL;
  std::vector<InferTaskSptr> preproc_tasks;
  InferTaskSptr H2D_task = NULL;
  InferTaskSptr invoke_task = NULL;
  InferTaskSptr D2H_task = NULL;
  std::vector<InferTaskSptr> postproc_tasks;
  InferTaskSptr transmit_task = NULL;
  edk::MluMemoryOp mem_op;
  edk::EasyInfer infer;
  edk::MluContext env;
  edk::MluResizeConvertOp rc_op;
  std::mutex rc_op_mtx;
  struct {
    void* y_plane_data = nullptr;
    void* uv_plane_data = nullptr;
  } rc_input_fake_data;
  int drop_count = 0;
  void** cpu_input = nullptr;
  void** mlu_output = nullptr;
  void** cpu_output = nullptr;
  // resize and color convert operator output
  void** mlu_input = nullptr;
  /* batch */
  std::vector<CNFrameInfoPtr> vec_data;
  volatile bool initialized = false;
  ~InferContext() {
    if (initialized) {
      env.ConfigureForThisThread();
      if (nullptr != mlu_output) mem_op.FreeArrayMlu(mlu_output, mem_op.Loader()->OutputNum());
      if (nullptr != cpu_output) mem_op.FreeCpuOutput(cpu_output);
      if (nullptr != cpu_input) mem_op.FreeCpuInput(cpu_input);
      if (nullptr != mlu_input) mem_op.FreeArrayMlu(mlu_input, mem_op.Loader()->InputNum());
      if (nullptr != rc_input_fake_data.y_plane_data) cnrtFree(rc_input_fake_data.y_plane_data);
      if (nullptr != rc_input_fake_data.uv_plane_data) cnrtFree(rc_input_fake_data.uv_plane_data);
      rc_op.Destroy();
    }
  }
};  // struct InferContext

static thread_local InferContext* g_tl_ctx;

static edk::MluResizeConvertOp::ColorMode FMTCONVERT2CMODE(CNDataFormat fmt) {
  switch (fmt) {
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
      return edk::MluResizeConvertOp::ColorMode::YUV2BGRA_NV12;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
      return edk::MluResizeConvertOp::ColorMode::YUV2BGRA_NV21;
    default:
      LOG(FATAL) << "Can not deal with this format.";
  }
  return edk::MluResizeConvertOp::ColorMode::YUV2BGRA_NV21;
}

class InferencerExPrivate {
 public:
  explicit InferencerExPrivate(InferencerEx* q) : q_ptr_(q) {}
  InferThreadPool tpool_;
  std::shared_ptr<edk::ModelLoader> model_loader_;
  std::shared_ptr<Preproc> cpu_preproc_;
  std::shared_ptr<Postproc> post_proc_;
  int device_id_ = 0;
  int interval_ = 0;
  uint32_t bsize_ = 1;
  float batching_timeout_ = 3000.0;
  std::vector<InferContext*> infer_ctxs_;
  std::mutex ctx_mtx_;
  void CheckAndUpdateRCOp(InferContext* pctx, CNFrameInfoPtr data) {
    const edk::MluResizeConvertOp::Attr& attr = pctx->rc_op.GetAttr();
    if (static_cast<int>(attr.src_w) != data->frame.width || static_cast<int>(attr.src_h) != data->frame.height ||
        static_cast<int>(attr.src_stride) != data->frame.stride[0] ||
        FMTCONVERT2CMODE(data->frame.fmt) != attr.color_mode) {
      pctx->rc_op.Destroy();
      edk::Shape model_input_shape = model_loader_->InputShapes()[0];
      edk::MluResizeConvertOp::Attr new_attr;
      new_attr.src_h = data->frame.height;
      new_attr.src_w = data->frame.width;
      new_attr.dst_h = model_input_shape.h;
      new_attr.dst_w = model_input_shape.w;
      new_attr.src_stride = data->frame.stride[0];
      new_attr.color_mode = FMTCONVERT2CMODE(data->frame.fmt);
      if (nullptr != pctx->rc_input_fake_data.y_plane_data) cnrtFree(pctx->rc_input_fake_data.y_plane_data);
      cnrtRet_t cnret = cnrtMalloc(&(pctx->rc_input_fake_data.y_plane_data), bsize_ * data->frame.GetPlaneBytes(0));
      CHECK_EQ(cnret, CNRT_RET_SUCCESS) << "Malloc resize convert fake data(for y plane) failed.";
      if (nullptr != pctx->rc_input_fake_data.uv_plane_data) cnrtFree(pctx->rc_input_fake_data.uv_plane_data);
      cnret = cnrtMalloc(&(pctx->rc_input_fake_data.uv_plane_data), bsize_ * data->frame.GetPlaneBytes(1));
      CHECK_EQ(cnret, CNRT_RET_SUCCESS) << "Malloc resize convert fake data(for uv plane) failed.";
      bool ret = pctx->rc_op.Init(new_attr);
      LOG_IF(FATAL, !ret);
    }
  }
  InferContext* GetInferContext() {
    /* control thread context by inferencer itself */
    if (nullptr == g_tl_ctx) {
      g_tl_ctx = new InferContext();
      std::lock_guard<std::mutex> lk(ctx_mtx_);
      infer_ctxs_.push_back(g_tl_ctx);
    }

    InferContext* pctx = g_tl_ctx;
#ifdef CNS_MLU100
    const uint32_t mem_bsize = bsize_;
#elif CNS_MLU270
    const uint32_t mem_bsize = 1;
#endif

    if (!g_tl_ctx->initialized) {
      // process for first time

      // input information flags
      bool model_yuv_input = model_loader_->WithYUVInput();
      bool model_rgb0_output = model_loader_->WithRGB0Output(nullptr);
      if (!model_yuv_input && model_rgb0_output) {
        throw InferencerExError("Model has wrong IO shape");
      }

      pctx->drop_count = 0;
      pctx->mem_op.SetLoader(model_loader_);

      // Configure mlu context
      pctx->env.SetDeviceId(device_id_);
      pctx->env.ConfigureForThisThread();

      // Create inference tool
      pctx->infer.Init(model_loader_, mem_bsize, device_id_);

      // prepare memory
      pctx->cpu_input = pctx->mem_op.AllocCpuInput(mem_bsize);
      pctx->cpu_output = pctx->mem_op.AllocCpuOutput(mem_bsize);
      pctx->mlu_input = pctx->mem_op.AllocMluInput(mem_bsize);
      pctx->mlu_output = pctx->mem_op.AllocMluOutput(mem_bsize);

      // resize and convert op
      uint32_t src_w = 0;
      uint32_t src_h = 0;
      edk::Shape model_input_shape = model_loader_->InputShapes()[0];
      uint32_t dst_w = model_input_shape.w;
      uint32_t dst_h = model_input_shape.h;

      edk::MluResizeConvertOp::Attr new_attr;
      new_attr.src_h = src_h;
      new_attr.src_w = src_w;
      new_attr.dst_h = dst_h;
      new_attr.dst_w = dst_w;
      new_attr.color_mode = edk::MluResizeConvertOp::ColorMode::YUV2BGRA_NV21;
      pctx->rc_op.SetMluQueue(pctx->infer.GetMluQueue());
      // batching timeout operator
      pctx->timeout_handler = std::make_shared<TimeoutOperator>();
      pctx->timeout_handler->SetTimeout(batching_timeout_);

      pctx->initialized = true;
      LOG(INFO) << "[InferencerEx] Create new context";
    }
    return pctx;
  }

  size_t GetBatchIndex(InferContext* pctx) {
    assert(pctx);
    return pctx->vec_data.size();
  }

  InferTaskSptr CreatePreprocTask(InferContext* pctx, size_t bidx, const std::shared_ptr<CNFrameInfo>& data,
                                  const std::shared_ptr<CNInferObject>& obj) {
    CHECK_GT(bidx, 0);
    auto tfunc = [=]() -> int {
      pctx->env.ConfigureForThisThread();
      auto shapes = model_loader_->InputShapes();

      if (cpu_preproc_.get()) {
        /* case A: preprocessing use cpu */
        std::vector<float*> net_inputs;  // neuron network inputs

        for (size_t input_i = 0; input_i < shapes.size(); ++input_i) {
          uint64_t offset = shapes[input_i].hwc();
          net_inputs.push_back(reinterpret_cast<float*>(pctx->cpu_input[input_i]) + (bidx - 1) * offset);
        }

        cpu_preproc_->Execute(net_inputs, model_loader_, data, obj);
      } else {
        /* case B: preprocessing use mlu */
        if (!model_loader_->WithYUVInput()) {
          /* case B.a: model with rgb input, use resize convert */
          if (data->frame.fmt != CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12 &&
              data->frame.fmt != CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21) {
            q_ptr_->PostEvent(EVENT_ERROR, "Unsupport data format: " + std::to_string(data->frame.fmt));
            return -1;
          }
          void* y_plane = data->frame.data[0]->GetMutableMluData();
          void* uv_plane = data->frame.data[1]->GetMutableMluData();
          try {
            std::lock_guard<std::mutex> lk(pctx->rc_op_mtx);
            /*
              debug info:
                pctx->rc_op.InvokeOp will be stuck when pctx->rc_op.src_resolution() is (0, 0)
            */
            pctx->rc_op.BatchingUp(y_plane, uv_plane);
          } catch (edk::Exception& e) {
            q_ptr_->PostEvent(EVENT_ERROR, e.what());
            return -1;
          }
          return 0;
        } else {
          /* case B.b: model with yuv input, check frame size and use d2d */
          if (data->frame.fmt != CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12 &&
              data->frame.fmt != CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21) {
            q_ptr_->PostEvent(EVENT_ERROR, "Unsupport data format: " + std::to_string(data->frame.fmt));
            return -1;
          }

          void* y_plane_src = data->frame.data[0]->GetMutableMluData();
          void* uv_plane_src = data->frame.data[1]->GetMutableMluData();
          void *y_plane_dst = nullptr, *uv_plane_dst = nullptr;

          if (IsYAndUVSplit(model_loader_)) {
            /* case B.b.1: model with y and uv split input */
            if (data->frame.width != static_cast<int>(shapes[0].w) ||
                data->frame.height != static_cast<int>(shapes[0].h)) {
              q_ptr_->PostEvent(EVENT_ERROR,
                                "Can not deal with this frame, wrong size: " + std::to_string(data->frame.width) + "x" +
                                    std::to_string(data->frame.height));
              return -1;
            }

            uint64_t y_plane_offset = 0, uv_plane_offset = 0;
            {
#ifdef CNS_MLU100
              unsigned int align_size = model_loader_->GetOutputDataBatchAlignSize(0);

              y_plane_offset = (bidx - 1) * align_size;
              align_size = model_loader_->GetOutputDataBatchAlignSize(1);
              uv_plane_offset = (bidx - 1) * align_size;
#elif CNS_MLU270
              y_plane_offset = (bidx - 1) * shapes[0].hw();
              uv_plane_offset = (bidx - 1) * shapes[1].hw();
#endif
            }
            y_plane_dst = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(pctx->mlu_input[0]) + y_plane_offset);
            uv_plane_dst = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(pctx->mlu_input[1]) + uv_plane_offset);

          } else {
            /* case B.b.2: model with y and uv packed input */
            if (data->frame.width != static_cast<int>(shapes[0].w) ||
                data->frame.height * 3 / 2 != static_cast<int>(shapes[0].h)) {
              q_ptr_->PostEvent(EVENT_ERROR,
                                "Can not deal with this frame, wrong size: " + std::to_string(data->frame.width) + "x" +
                                    std::to_string(data->frame.height));
              return -1;
            }
            uint64_t offset = 0;
            {
#ifdef CNS_MLU100
              unsigned int align_size = model_loader_->GetOutputDataBatchAlignSize(0);

              offset = (bidx - 1) * align_size;
#elif CNS_MLU270
              offset = (bidx - 1) * shapes[0].hw();
#endif
            }
            void* yuv_dst = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(pctx->mlu_input[0]) + offset);
            y_plane_dst = yuv_dst;
            uv_plane_dst = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(yuv_dst) + data->frame.GetPlaneBytes(0));
          }  // if (IsYAndUVSplit(model_loader_))

          // copy frame to invoke input buffer
          cnrtRet_t ret =
              cnrtMemcpy(y_plane_dst, y_plane_src, data->frame.GetPlaneBytes(0), CNRT_MEM_TRANS_DIR_DEV2DEV);
          CHECK_EQ(CNRT_RET_SUCCESS, ret) << "y plane dst:" << y_plane_dst << " y plane src:" << y_plane_src
                                          << " bytes:" << data->frame.GetPlaneBytes(0);
          ret = cnrtMemcpy(uv_plane_dst, uv_plane_src, data->frame.GetPlaneBytes(1), CNRT_MEM_TRANS_DIR_DEV2DEV);
          CHECK_EQ(CNRT_RET_SUCCESS, ret) << "uv plane dst:" << uv_plane_dst << " uv plane src:" << uv_plane_src
                                          << " bytes:" << data->frame.GetPlaneBytes(1);
        }  // if (!model_loader_->WithYUVInput())
      }    // if (cpu_preproc_.get())

      return 0;
    };

    auto task = std::make_shared<InferTask>(tfunc);
    task->task_msg = "Preprocess task";

    return task;
  }

  InferTaskSptr CreateH2DTask(InferContext* pctx) {
    auto tfunc = [=]() -> int {
      pctx->env.ConfigureForThisThread();
      CHECK_EQ(cpu_preproc_.get() != nullptr, true);
#ifdef CNS_MLU100
      const uint32_t mem_bsize = bsize_;
#elif CNS_MLU270
      const uint32_t mem_bsize = 1;
#endif
      try {
        if (cpu_preproc_.get() != nullptr) {
          pctx->mem_op.MemcpyInputH2D(pctx->mlu_input, pctx->cpu_input, mem_bsize);
        }
      } catch (edk::Exception& e) {
        q_ptr_->PostEvent(EVENT_ERROR, e.what());
        return -1;
      }
      return 0;
    };

    auto task = std::make_shared<InferTask>(tfunc);
    task->task_msg = "H2D task";

    return task;
  }

  InferTaskSptr CreateInvokeTask(InferContext* pctx) {
    auto vec_data = pctx->vec_data;
    auto tfunc = [=]() -> int {
      pctx->env.ConfigureForThisThread();
      const uint32_t bsize = vec_data.size();
      if (bsize == 0) return 0;
      try {
        if (cpu_preproc_.get() == nullptr) {
          // prepare fake data
          char* y_plane_fake_data = reinterpret_cast<char*>(pctx->rc_input_fake_data.y_plane_data);
          char* uv_plane_fake_data = reinterpret_cast<char*>(pctx->rc_input_fake_data.uv_plane_data);
          for (uint32_t bi = bsize; bi < bsize_; ++bi) {
            auto src_y = reinterpret_cast<void*>(y_plane_fake_data);
            auto src_uv = reinterpret_cast<void*>(uv_plane_fake_data);
            pctx->rc_op.BatchingUp(src_y, src_uv);
            y_plane_fake_data += vec_data[0]->frame.GetPlaneBytes(0);
            uv_plane_fake_data += vec_data[0]->frame.GetPlaneBytes(1);
          }
          /* do resize convert */
          if (!pctx->rc_op.SyncOneOutput(pctx->mlu_input[0])) {
            throw edk::Exception(pctx->rc_op.GetLastError());
          }
        }
        /* inference by offline model */
        pctx->infer.Run(pctx->mlu_input, pctx->mlu_output);
      } catch (edk::Exception& e) {
        q_ptr_->PostEvent(EVENT_ERROR, e.what());
        return -1;
      }

      return 0;
    };

    auto task = std::make_shared<InferTask>(tfunc);
    task->task_msg = "Invoke task";

    return task;
  }

  InferTaskSptr CreateD2HTask(InferContext* pctx) {
    auto tfunc = [=]() -> int {
      pctx->env.ConfigureForThisThread();
#ifdef CNS_MLU100
      const uint32_t mem_bsize = bsize_;
#elif CNS_MLU270
      const uint32_t mem_bsize = 1;
#endif
      try {
        /* copy results to host */
        pctx->mem_op.MemcpyOutputD2H(pctx->cpu_output, pctx->mlu_output, mem_bsize);
      } catch (edk::Exception& e) {
        q_ptr_->PostEvent(EVENT_ERROR, e.what());
        return -1;
      }
      return 0;
    };

    auto task = std::make_shared<InferTask>(tfunc);
    task->task_msg = "D2H task";

    return task;
  }

  InferTaskSptr CreatePostprocTask(InferContext* pctx, size_t bidx, const std::shared_ptr<CNFrameInfo>& data,
                                   const std::shared_ptr<cnstream::CNInferObject>& obj) {
    auto tfunc = [=]() -> int {
      /* structed inference results */
      if (post_proc_.get() != nullptr) {
        auto shapes = model_loader_->OutputShapes();
        std::vector<float*> results;
        for (size_t output_i = 0; output_i < shapes.size(); ++output_i) {
#ifdef CNS_MLU100
          uint64_t offset = shapes[output_i].DataCount();
#elif CNS_MLU270
          uint64_t offset = shapes[output_i].DataCount() / bsize_;
#endif
          results.push_back(reinterpret_cast<float*>(pctx->cpu_output[output_i]) + bidx * offset);
        }
        if (post_proc_.get()) post_proc_->Execute(results, model_loader_, data, obj);
      }
      return 0;
    };

    auto task = std::make_shared<InferTask>(tfunc);
    task->task_msg = "Postprocess task";

    return task;
  }

  InferTaskSptr CreateTransmitDataTask(InferContext* pctx) {
    auto vec_data = pctx->vec_data;
    auto tfunc = [this, vec_data]() -> int {
      for (auto& it : vec_data) {
        if (q_ptr_->container_) q_ptr_->container_->ProvideData(q_ptr_, it);
      }
      return 0;
    };

    auto task = std::make_shared<InferTask>(tfunc);
    task->task_msg = "Transmit data task";

    return task;
  }

  void Forward(const std::shared_ptr<CNFrameInfo>& data, const std::shared_ptr<CNInferObject>& obj) {
    InferContext* pctx = GetInferContext();
    assert(pctx);
    pctx->timeout_handler->LockOperator();
    std::function<void()> batching_func = [=]() {
      // H2D?
      if (cpu_preproc_.get()) {
        pctx->H2D_task = CreateH2DTask(pctx);
        pctx->H2D_task->BindFrontTasks(pctx->preproc_tasks);
        pctx->H2D_task->BindFrontTask(pctx->invoke_task);
        tpool_.SubmitTask(pctx->H2D_task);
      }
      // invoke task
      pctx->invoke_task = CreateInvokeTask(pctx);
      if (cpu_preproc_.get()) {
        pctx->invoke_task->BindFrontTask(pctx->H2D_task);
      } else {
        pctx->invoke_task->BindFrontTasks(pctx->preproc_tasks);
      }
      tpool_.SubmitTask(pctx->invoke_task);
      // D2H tasks
      pctx->D2H_task = CreateD2HTask(pctx);
      pctx->D2H_task->BindFrontTask(pctx->invoke_task);
      pctx->D2H_task->BindFrontTasks(pctx->postproc_tasks);
      tpool_.SubmitTask(pctx->D2H_task);

      // clear tasks
      pctx->preproc_tasks.clear();
      pctx->postproc_tasks.clear();

      // postproc tasks
      for (size_t bi = 0; bi < pctx->vec_data.size(); ++bi) {
        InferTaskSptr task = CreatePostprocTask(pctx, bi, pctx->vec_data[bi], obj);
        task->BindFrontTask(pctx->D2H_task);
        pctx->postproc_tasks.push_back(task);
        tpool_.SubmitTask(task);
      }
    };  // batching func

    size_t bidx = GetBatchIndex(pctx);
    assert(bidx <= bsize_);

    // preprocess tasks
    InferTaskSptr task = CreatePreprocTask(pctx, bidx, data, obj);
    if (cpu_preproc_.get())
      task->BindFrontTask(pctx->H2D_task);
    else
      task->BindFrontTask(pctx->invoke_task);
    pctx->preproc_tasks.push_back(task);
    tpool_.SubmitTask(task);

    // batch func
    if (bidx == bsize_) {
      batching_func();
      batching_func = NULL;
    }

    // timeout processing
    pctx->timeout_handler->Reset(batching_func);
    pctx->timeout_handler->UnlockOperator();
  }

  DECLARE_PUBLIC(q_ptr_, InferencerEx);
};  // class InferencerPrivate

InferencerEx::InferencerEx(const std::string& name) : Module(name) {
  d_ptr_ = nullptr;
  hasTransmit_.store(1);  // transmit data by module itself
}

InferencerEx::~InferencerEx() {}

bool InferencerEx::Open(ModuleParamSet paramSet) {
  if (paramSet.find("model_path") == paramSet.end() || paramSet.find("func_name") == paramSet.end() ||
      paramSet.find("postproc_name") == paramSet.end()) {
    LOG(WARNING) << "InferencerEx must specify [model_path]、[func_name]、[postproc_name].";
    return false;
  }

  d_ptr_ = new InferencerExPrivate(this);
  if (!d_ptr_) {
    return false;
  }

  std::string model_path = paramSet["model_path"];
  model_path = GetPathRelativeToTheJSONFile(model_path, paramSet);

  std::string func_name = paramSet["func_name"];
  std::string Data_Order;
  if (paramSet.find("data_order") != paramSet.end()) {
    Data_Order = paramSet["data_order"];
  }

  try {
    d_ptr_->model_loader_ = std::make_shared<edk::ModelLoader>(model_path, func_name);
    if (d_ptr_->model_loader_.get() == nullptr) {
      LOG(ERROR) << "[InferencerEx] load model failed, model path: " << model_path;
      return false;
    }
    d_ptr_->model_loader_->InitLayout();
    if (Data_Order == "NCHW") {
      edk::DataLayout layout;
      layout.dtype = edk::DataType::FLOAT32;
      layout.order = edk::DimOrder::NCHW;
      d_ptr_->model_loader_->SetCpuOutputLayout(layout, 0);
    }
    std::string postproc_name = paramSet["postproc_name"];
    d_ptr_->post_proc_ = std::shared_ptr<cnstream::Postproc>(cnstream::Postproc::Create(postproc_name));
    if (d_ptr_->post_proc_.get() == nullptr) {
      LOG(ERROR) << "[InferencerEx] Can not find Postproc implemention by name: " << postproc_name;
      return false;
    }
  } catch (edk::Exception& e) {
    LOG(ERROR) << "model path:" << model_path << ". " << e.what();
    return false;
  }

  d_ptr_->cpu_preproc_ = nullptr;
  auto preproc_name = paramSet.find("preproc_name");
  if (preproc_name != paramSet.end()) {
    d_ptr_->cpu_preproc_ = std::shared_ptr<Preproc>(Preproc::Create(preproc_name->second));
    if (d_ptr_->cpu_preproc_.get() == nullptr) {
      LOG(ERROR) << "[InferencerEx] CPU preproc name not found: " << preproc_name->second;
      return false;
    }
    LOG(INFO) << "[InferencerEx] With CPU preproc set";
  }

  d_ptr_->device_id_ = 0;
  if (paramSet.find("device_id") != paramSet.end()) {
    std::stringstream ss;
    int device_id;
    ss << paramSet["device_id"];
    ss >> device_id;
    d_ptr_->device_id_ = device_id;
  }

#ifdef CNS_MLU100
  if (paramSet.find("batch_size") != paramSet.end()) {
    std::stringstream ss;
    ss << paramSet["batch_size"];
    ss >> d_ptr_->bsize_;
  }
#elif CNS_MLU270
  d_ptr_->bsize_ = d_ptr_->model_loader_->InputShapes()[0].n;
#endif
  DLOG(INFO) << GetName() << " batch size:" << d_ptr_->bsize_;

  if (paramSet.find("infer_interval_") != paramSet.end()) {
    std::stringstream ss;
    ss << paramSet["infer_interval"];
    ss >> d_ptr_->interval_;
    LOG(INFO) << GetName() << " infer_interval:" << d_ptr_->interval_;
  }

  // batching timeout
  if (paramSet.find("batching_timeout") != paramSet.end()) {
    std::stringstream ss;
    ss << paramSet["batching_timeout"];
    ss >> d_ptr_->batching_timeout_;
    LOG(INFO) << GetName() << " batching timeout:" << d_ptr_->batching_timeout_;
  }

  if (container_ == nullptr) {
    LOG(INFO) << name_ << " has not been added into pipeline.";
  } else {
    // calculate thread number by module parallelism.
    int parallelism = container_->GetModuleConfig(GetName()).parallelism;
#ifdef CNS_MLU100
    int total = parallelism + 3 * parallelism * d_ptr_->bsize_;
#elif CNS_MLU270
    int total = 4 + 3 * parallelism * d_ptr_->bsize_;
#endif
    d_ptr_->tpool_.Init(0, total);
  }

  // hold this code. when all threads that set the cnrt device id exit, cnrt may release the memory itself
  edk::MluContext ctx;
  ctx.SetDeviceId(d_ptr_->device_id_);
  ctx.ConfigureForThisThread();

  return true;
}

void InferencerEx::Close() {
  if (nullptr == d_ptr_) return;

  d_ptr_->tpool_.Destroy();

  // destroy infer contexts
  std::lock_guard<std::mutex> lk(d_ptr_->ctx_mtx_);
  for (InferContext* it : d_ptr_->infer_ctxs_) {
    delete it;
  }
  d_ptr_->infer_ctxs_.clear();

  delete d_ptr_;
  d_ptr_ = nullptr;
}

int InferencerEx::Process(CNFrameInfoPtr data) {
  InferContext* pctx = d_ptr_->GetInferContext();

  // d_ptr_->CheckAndUpdateRCOp(pctx, data);
  pctx->vec_data.push_back(data);

  auto last_transmit_task = pctx->transmit_task;
  pctx->transmit_task = d_ptr_->CreateTransmitDataTask(pctx);

  // Judgment of eos frame
  if (data->frame.flags & CNFrameFlag::CN_FRAME_FLAG_EOS) {
    pctx->transmit_task->BindFrontTasks(pctx->postproc_tasks);
    pctx->transmit_task->BindFrontTask(last_transmit_task);
    pctx->vec_data.clear();
    d_ptr_->tpool_.SubmitTask(pctx->transmit_task);
    return 1;
  }

  // check inference interval
  if (d_ptr_->interval_ > 0) {
    if (pctx->drop_count++ % d_ptr_->interval_ != 0) {
      pctx->drop_count %= d_ptr_->interval_;
      return 0;
    }
  }

  std::vector<std::shared_ptr<CNInferObject>> filterOutBox;

  // nms
  mtcnn::nms(&data->objs, &filterOutBox, FLAGS_nms_threshold, mtcnn::UNION);

  // conver to square
  mtcnn::convertToSquare(&filterOutBox);

  // loop infer
  for (uint32_t i = 0; i < filterOutBox.size(); ++i) {
    if (filterOutBox[i]->bbox.x >= 0 && filterOutBox[i]->bbox.y >= 0 &&
        filterOutBox[i]->bbox.x + filterOutBox[i]->bbox.w <= data->frame.width &&
        filterOutBox[i]->bbox.y + filterOutBox[i]->bbox.h <= data->frame.height) {
      d_ptr_->Forward(data, filterOutBox[i]);
    }
  }

  // tramsmit data
  pctx->transmit_task->BindFrontTasks(pctx->postproc_tasks);
  pctx->transmit_task->BindFrontTask(last_transmit_task);
  pctx->vec_data.clear();
  d_ptr_->tpool_.SubmitTask(pctx->transmit_task);

  return 1;
}

}  // namespace cnstream
