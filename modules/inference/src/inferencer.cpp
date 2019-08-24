/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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

#include "inferencer.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "cninfer/cninfer.h"
#include "cninfer/mlu_context.h"
#include "cninfer/mlu_memory_op.h"
#include "cninfer/model_loader.h"
#include "cnpreproc/resize_and_colorcvt.h"

#include "cnstream_eventbus.hpp"
#include "cnstream_frame.hpp"
#include "cnstream_timer.hpp"
#include "postproc.hpp"
#include "preproc.hpp"

namespace cnstream {

constexpr uint32_t kBatchSize = 1;

class InferencerPrivate {
 public:
  std::shared_ptr<libstream::ModelLoader> model_loader_;
  std::shared_ptr<Preproc> cpu_preproc_;
  std::shared_ptr<Postproc> post_proc_;
  int device_id_ = 0;
  bool open_mlu_resize_ = false;  // open mlu resize option
  uint32_t resize_src_w_ = 0, resize_src_h_ = 0;
  std::mutex ctx_mut_;
  std::unordered_map<int, InferContext *> infer_ctxs_;
};  // class InferencerPrivate

Inferencer::Inferencer(const std::string &name) : Module(name) { d_ptr_ = nullptr; }

Inferencer::~Inferencer() { Close(); }

bool Inferencer::Open(ModuleParamSet paramSet) {
  if (paramSet.find("model_path") == paramSet.end() || paramSet.find("func_name") == paramSet.end() ||
      paramSet.find("postproc_name") == paramSet.end()) {
    return false;
  }

  d_ptr_ = new InferencerPrivate;
  if (!d_ptr_) {
    return false;
  }

  std::string model_path = paramSet.find("model_path")->second;
  std::string func_name = paramSet.find("func_name")->second;
  try {
    d_ptr_->model_loader_ = std::make_shared<libstream::ModelLoader>(model_path, func_name);
    if (d_ptr_->model_loader_.get() == nullptr) {
      LOG(ERROR) << "Can not find model_loader_ implemention by name: " << model_path;
      return false;
    }
    d_ptr_->model_loader_->InitLayout();

    std::string postproc_name = paramSet.find("postproc_name")->second;
    d_ptr_->post_proc_ = std::shared_ptr<cnstream::Postproc>(cnstream::Postproc::Create(postproc_name));
    if (d_ptr_->post_proc_.get() == nullptr) {
      LOG(ERROR) << "Can not find Postproc implemention by name: " << postproc_name;
      return false;
    }
  } catch (libstream::StreamlibsError &e) {
    LOG(ERROR) << e.what();
    return false;
  }

  d_ptr_->cpu_preproc_ = nullptr;
  if (paramSet.find("preproc_name") != paramSet.end()) {
    // TODO
    // d_ptr_->cpu_preproc_ = cpu_preproc;
    LOG(ERROR) << "cpu_preproc_ not implemented yet";
    return false;
  }

  d_ptr_->device_id_ = 0;
  if (paramSet.find("device_id") != paramSet.end()) {
    std::stringstream ss;
    int device_id;
    ss << paramSet.find("device_id")->second;
    ss >> device_id;
    d_ptr_->device_id_ = device_id;
  }

  return true;
}

void Inferencer::Close() {
  if (d_ptr_ == nullptr) {
    return;
  }
  try {
    if (d_ptr_->model_loader_.get() != nullptr) {
      InferContext *ctx;
      int i_num = d_ptr_->model_loader_->input_num();
      int o_num = d_ptr_->model_loader_->output_num();
      std::unique_lock<std::mutex> lk(d_ptr_->ctx_mut_);
      for (auto &ctx_pair : d_ptr_->infer_ctxs_) {
        ctx = ctx_pair.second;
        ctx->env.ConfigureForThisThread();
        ctx->mem_op.free_mem_array_on_mlu(ctx->mlu_output, o_num);
        ctx->mem_op.free_output_mem_on_cpu(ctx->cpu_output);
        ctx->mem_op.free_mem_array_on_mlu(ctx->mlu_input, i_num);
        if (d_ptr_->open_mlu_resize_) {
          ctx->rc_op.Destroy();
        }
        delete ctx;
      }
      d_ptr_->infer_ctxs_.clear();
      lk.unlock();
      d_ptr_->model_loader_.reset();
    }
  } catch (libstream::StreamlibsError &e) {
    LOG(ERROR) << e.what();
  }
  delete d_ptr_;
  d_ptr_ = nullptr;
}

void Inferencer::RunMluRCOp(CNDataFrame *frame, void *resize_output_data, libstream::MluRCOp *resize_op) {
  if (frame->GetPlanes() == 0) {
    throw InferencerError("No plane data.");
  }
  void *src_y = frame->data[0]->GetMutableMluData();
  void *src_uv = frame->data[1]->GetMutableMluData();
  void *dst = resize_output_data;
  // Run Mlu reszie and color convert operaor (MluRCOp)
  if (-1 == resize_op->InvokeOp(dst, src_y, src_uv)) {
    throw InferencerError("RCOp Invoke failed. " + resize_op->GetLastError());
  }
  cnrtRet_t error_code = cnrtSyncStream(resize_op->cnrt_stream());
  if (CNRT_RET_SUCCESS != error_code) {
    throw InferencerError("Sync stream failed. Error code: " + std::to_string(error_code));
  }
}

InferContext *Inferencer::GetInferContext(CNFrameInfoPtr data) {
  InferContext *context;

  auto search = d_ptr_->infer_ctxs_.find(data->channel_idx);
  if (search != d_ptr_->infer_ctxs_.end()) {
    // context exists
    context = search->second;
  } else {
    // process for first time
    LOG(INFO) << "[Inferencer] Create new context";
    // check input
    if (data->frame.ctx.dev_type == DevContext::DevType::INVALID) {
      throw InferencerError("Input data type is INVALID");
    }

    // input information flags
    bool yuv_input = d_ptr_->model_loader_->WithYUVInput();
    bool rgb0_output = d_ptr_->model_loader_->WithRGB0Output(nullptr);
    if (!yuv_input && rgb0_output) {
      throw InferencerError("NN Model has wrong IO shape");
    }
    CNDataFormat fmt = data->frame.fmt;
    switch (fmt) {
      case CN_PIXEL_FORMAT_YUV420_NV12:
      case CN_PIXEL_FORMAT_YUV420_NV21:
        d_ptr_->open_mlu_resize_ = yuv_input ? false : true;
        break;
      case CN_PIXEL_FORMAT_RGB24:
      case CN_PIXEL_FORMAT_BGR24:
        d_ptr_->open_mlu_resize_ = false;
        break;
      default:
        throw InferencerError("Input frame format is wrong");
        break;
    }

    if (data->frame.ctx.dev_type == DevContext::DevType::CPU) {
      d_ptr_->open_mlu_resize_ = false;
    }

    context = new InferContext;
    context->mem_op.set_loader(d_ptr_->model_loader_);

    // Configure mlu context
    context->env.set_dev_id(d_ptr_->device_id_);
    context->env.ConfigureForThisThread();
    // Create stream infer
    context->infer.init(d_ptr_->model_loader_, kBatchSize);

    // Allocate memory for output data
    context->mlu_output = context->mem_op.alloc_mem_on_mlu_for_output(kBatchSize);
    context->cpu_output = context->mem_op.alloc_mem_on_cpu_for_output(kBatchSize);

    // Allocate MluRCOp output.
    context->mlu_input = context->mem_op.alloc_mem_on_mlu_for_input(kBatchSize);

    if (d_ptr_->open_mlu_resize_) {
      // Set source and destination data shape and other parameters
      uint32_t src_w = data->frame.width;
      uint32_t src_h = data->frame.height;
      libstream::CnShape model_input_shape = d_ptr_->model_loader_->input_shapes()[0];
      uint32_t dst_w = model_input_shape.w();
      uint32_t dst_h = model_input_shape.h();
      context->rc_op.set_src_resolution(src_w, src_h);
      context->rc_op.set_dst_resolution(dst_w, dst_h);
      cnrtFunctionType_t func_type = CNRT_FUNC_TYPE_BLOCK;
      context->rc_op.set_ftype(func_type);
      context->rc_op.set_cnrt_stream(context->infer.rt_stream());
      context->rc_op.set_cmode(libstream::MluRCOp::YUV2BGRA_NV21);
      // Init MluRCOp
      if (!context->rc_op.Init()) {
        std::string msg = "Init MluRCOp failed:" + context->rc_op.GetLastError();
        throw InferencerError(msg);
      }
    }  // if (MluResizeOpened)

    std::unique_lock<std::mutex> lk(d_ptr_->ctx_mut_);
    d_ptr_->infer_ctxs_[data->channel_idx] = context;
  }
  return context;
}

int Inferencer::Process(CNFrameInfoPtr data) {
  try {
    LOG(INFO) << "[Inferencer] Processing...";
    InferContext *ctx = GetInferContext(data);

    // prepare input on mlu
    void *mlu_input[1];
    // get model input ptr on mlu
    if (d_ptr_->open_mlu_resize_) {
      // Set MluRCOp output to infer input. And run MluRCOp.
      if (data->frame.GetPlanes() != 2) {
        throw InferencerError("Input frame planes is wrong");
      }
      mlu_input[0] = ctx->mlu_input[0];
      RunMluRCOp(&data->frame, ctx->mlu_input[0], &ctx->rc_op);
    } else if (data->frame.ctx.dev_type == DevContext::DevType::CPU) {
      if (d_ptr_->cpu_preproc_) {
        d_ptr_->cpu_preproc_->Execute(ctx->mlu_input, data);
        mlu_input[0] = ctx->mlu_input[0];
      } else {
        throw InferencerError("CPU preproc not set");
      }
    } else {
      /********************************************************************
       * case 1: input from decoder. only support on mlu100.
       *         model with YUV input.
       * case 2: input from ResizeAndConver,
       *         model with RGBA input.
       ********************************************************************/
      if (data->frame.GetPlanes() != 1) {
        throw InferencerError("Input frame planes is wrong");
        return -1;
      }
      mlu_input[0] = data->frame.data[0]->GetMutableMluData();
    }

    // run inference
    ctx->infer.run(mlu_input, ctx->mlu_output);

    // copy out
    ctx->mem_op.memcpy_output_d2h(ctx->mlu_output, ctx->cpu_output, kBatchSize);

    // postproc
    std::vector<std::pair<float *, uint64_t>> nn_outputs;
    int o_num = d_ptr_->model_loader_->output_num();
    std::vector<libstream::CnShape> out_shapes = d_ptr_->model_loader_->output_shapes();

    for (int i = 0; i < o_num; ++i) {
      nn_outputs.push_back(
          std::make_pair<float *, uint64_t>(reinterpret_cast<float *>(ctx->cpu_output[i]), out_shapes[i].DataCount()));
    }

    d_ptr_->post_proc_->Execute(nn_outputs, data);
  } catch (libstream::StreamlibsError &error) {
    PostEvent(EventType::EVENT_ERROR, error.what());
    return -1;
  } catch (CnstreamError &error) {
    PostEvent(EventType::EVENT_ERROR, error.what());
    return -1;
  }
  LOG(INFO) << "[Inferencer] Processed on frame";
  return 0;
}

}  // namespace cnstream
