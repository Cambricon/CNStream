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

#include "inferencer.hpp"
#include "cnstream_pipeline.hpp"

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

/*********************************************************************************
 * @brief Inferencer thread context
 *********************************************************************************/
struct InferContext {
  libstream::MluMemoryOp mem_op;
  libstream::CnInfer infer;
  libstream::MluContext env;
  libstream::MluRCOp rc_op;
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
      if (nullptr != mlu_output) mem_op.free_mem_array_on_mlu(mlu_output, mem_op.loader()->output_num());
      if (nullptr != cpu_output) mem_op.free_output_mem_on_cpu(cpu_output);
      if (cpu_input != nullptr) mem_op.free_input_mem_on_cpu(cpu_input);
      if (nullptr != mlu_input) mem_op.free_mem_array_on_mlu(mlu_input, mem_op.loader()->input_num());
      rc_op.Destroy();
    }
  }
};  // struct InferContext

static thread_local InferContext g_tl_ctx;

static libstream::MluRCOp::ColorMode FMTCONVERT2CMODE(CNDataFormat fmt) {
  switch (fmt) {
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
      return libstream::MluRCOp::ColorMode::YUV2BGRA_NV12;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
      return libstream::MluRCOp::ColorMode::YUV2BGRA_NV21;
    default:
      LOG(FATAL) << "Can not deal with this format.";
  }
  return libstream::MluRCOp::ColorMode::YUV2BGRA_NV21;
}

class InferencerPrivate {
 public:
  explicit InferencerPrivate(Inferencer* q) : q_ptr_(q) {}
  std::shared_ptr<libstream::ModelLoader> model_loader_;
  std::shared_ptr<Preproc> cpu_preproc_;
  std::shared_ptr<Postproc> post_proc_;
  int device_id_ = 0;
  uint32_t bsize_ = 1;
  void CheckAndUpdateRCOp(CNFrameInfoPtr data) {
    InferContext* pctx = GetInferContext();
    uint32_t w, h, stride;
    pctx->rc_op.get_src_resolution(&w, &h, &stride);
    if (static_cast<int>(w) != data->frame.width || static_cast<int>(h) != data->frame.height ||
        static_cast<int>(stride) != data->frame.stride[0] ||
        FMTCONVERT2CMODE(data->frame.fmt) != pctx->rc_op.color_mode()) {
      pctx->rc_op.Destroy();
      pctx->rc_op.set_src_resolution(data->frame.width, data->frame.height, data->frame.stride[0]);
      pctx->rc_op.set_cmode(FMTCONVERT2CMODE(data->frame.fmt));
      bool ret = pctx->rc_op.Init();
      LOG_IF(FATAL, !ret);
    }
  }
  InferContext* GetInferContext() {
    const uint32_t bsize = bsize_;

    InferContext* pctx = &g_tl_ctx;

    if (!g_tl_ctx.initialized) {
      // process for first time
      LOG(INFO) << "[Inferencer] Create new context";

      // input information flags
      bool model_yuv_input = model_loader_->WithYUVInput();
      bool model_rgb0_output = model_loader_->WithRGB0Output(nullptr);
      if (!model_yuv_input && model_rgb0_output) {
        throw InferencerError("Model has wrong IO shape");
      }

      pctx->mem_op.set_loader(model_loader_);

      // Configure mlu context
      pctx->env.set_dev_id(device_id_);
      pctx->env.ConfigureForThisThread();

      // Create inference tool
      pctx->infer.init(model_loader_, bsize);

      // prepare memory
      pctx->cpu_input = pctx->mem_op.alloc_mem_on_cpu_for_input(bsize);
      pctx->mlu_output = pctx->mem_op.alloc_mem_on_mlu_for_output(bsize);
      pctx->cpu_output = pctx->mem_op.alloc_mem_on_cpu_for_output(bsize);
      pctx->mlu_input = pctx->mem_op.alloc_mem_on_mlu_for_input(bsize);

      // resize and convert op
      uint32_t src_w = 0;
      uint32_t src_h = 0;
      libstream::CnShape model_input_shape = model_loader_->input_shapes()[0];
      uint32_t dst_w = model_input_shape.w();
      uint32_t dst_h = model_input_shape.h();
      pctx->rc_op.set_src_resolution(src_w, src_h);
      pctx->rc_op.set_dst_resolution(dst_w, dst_h);
      pctx->rc_op.set_ftype(CNRT_FUNC_TYPE_BLOCK);
      pctx->rc_op.set_cnrt_stream(pctx->infer.rt_stream());
      pctx->rc_op.set_cmode(libstream::MluRCOp::YUV2BGRA_NV21);
      pctx->initialized = true;
    }
    return pctx;
  }
  DECLARE_PUBLIC(q_ptr_, Inferencer);

};  // class InferencerPrivate

Inferencer::Inferencer(const std::string& name) : Module(name) {
  d_ptr_ = nullptr;
  hasTransmit_.store(1);  // transmit data by module itself
}

Inferencer::~Inferencer() {}

bool Inferencer::Open(ModuleParamSet paramSet) {
  if (paramSet.find("model_path") == paramSet.end() || paramSet.find("func_name") == paramSet.end() ||
      paramSet.find("postproc_name") == paramSet.end()) {
    return false;
  }

  d_ptr_ = new InferencerPrivate(this);
  if (!d_ptr_) {
    return false;
  }

  std::string model_path = paramSet["model_path"];
  std::string func_name = paramSet["func_name"];
  try {
    d_ptr_->model_loader_ = std::make_shared<libstream::ModelLoader>(model_path, func_name);
    if (d_ptr_->model_loader_.get() == nullptr) {
      LOG(ERROR) << "[Inferencer] load model failed, model path: " << model_path;
      return false;
    }
    d_ptr_->model_loader_->InitLayout();

    std::string postproc_name = paramSet["postproc_name"];
    d_ptr_->post_proc_ = std::shared_ptr<cnstream::Postproc>(cnstream::Postproc::Create(postproc_name));
    if (d_ptr_->post_proc_.get() == nullptr) {
      LOG(ERROR) << "[Inferencer] Can not find Postproc implemention by name: " << postproc_name;
      return false;
    }
  } catch (libstream::StreamlibsError& e) {
    LOG(ERROR) << e.what();
    return false;
  }

  d_ptr_->cpu_preproc_ = nullptr;
  auto preproc_name = paramSet.find("preproc_name");
  if (preproc_name != paramSet.end()) {
    d_ptr_->cpu_preproc_ = std::shared_ptr<Preproc>(Preproc::Create(preproc_name->second));
    if (d_ptr_->cpu_preproc_.get() == nullptr) {
      LOG(ERROR) << "[Inferencer] CPU preproc name not found: " << preproc_name->second;
      return false;
    }
    LOG(INFO) << "[Inferencer] With CPU preproc set";
  }

  d_ptr_->device_id_ = 0;
  if (paramSet.find("device_id") != paramSet.end()) {
    std::stringstream ss;
    int device_id;
    ss << paramSet["device_id"];
    ss >> device_id;
    d_ptr_->device_id_ = device_id;
  }

  if (paramSet.find("batch_size") != paramSet.end()) {
    std::stringstream ss;
    ss << paramSet["batch_size"];
    ss >> d_ptr_->bsize_;
    DLOG(INFO) << GetName() << " batch size:" << d_ptr_->bsize_;
  }

  /* hold this code. when all threads that set the cnrt device id exit, cnrt may release the memory itself */
  libstream::MluContext ctx;
  ctx.set_dev_id(d_ptr_->device_id_);
  ctx.ConfigureForThisThread();

  return true;
}

void Inferencer::Close() {
  if (d_ptr_ != nullptr) {
    delete d_ptr_;
    d_ptr_ = nullptr;
  }
}

int Inferencer::Process(CNFrameInfoPtr data) {
  InferContext* pctx = d_ptr_->GetInferContext();
  if (data->frame.flags & CNFrameFlag::CN_FRAME_FLAG_EOS) {
    /* EOS, process batched data and transmit eos */
    int ret = ProcessBatch();
    pctx->vec_data.clear();
    if (container_) container_->ProvideData(this, data);
    return ret;
  } else {
    /* normal data, do preprocessing and batch it */
    const auto shapes = d_ptr_->model_loader_->input_shapes();
    const uint32_t batch_index = pctx->vec_data.size();

    if (d_ptr_->cpu_preproc_.get() != nullptr) {
      /* preprocessing use cpu */
      std::vector<float*> net_inputs;  // neuron network inputs
      for (size_t input_i = 0; input_i < shapes.size(); ++input_i) {
        uint64_t offset = shapes[input_i].DataCount();
        net_inputs.push_back(reinterpret_cast<float*>(pctx->cpu_input[input_i]) + input_i * offset);
      }
      d_ptr_->cpu_preproc_->Execute(net_inputs, d_ptr_->model_loader_, data);
    } else {
      /* preprocessing use mlu */

      const CNDataFormat fmt = data->frame.fmt;
      switch (fmt) {
        case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
        case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12: {
          unsigned int offset = 0;
          cnrtRet_t ret = cnrtGetMemcpyBatchAlignment(d_ptr_->model_loader_->input_desc_array()[0], &offset);
          CHECK_EQ(CNRT_RET_SUCCESS, ret);
          void* output_data =
              reinterpret_cast<void*>(reinterpret_cast<uint64_t>(pctx->mlu_input[0]) + offset * batch_index);
          void* plane_y = data->frame.data[0]->GetMutableMluData();
          void* plane_uv = data->frame.data[1]->GetMutableMluData();
          if (d_ptr_->model_loader_->WithYUVInput()) {
            CHECK_EQ(d_ptr_->model_loader_->input_shapes()[0].w(), static_cast<uint32_t>(data->frame.width))
                << "Your offline model can not deal with this frame, frame size mismatch.";
            CHECK_EQ(d_ptr_->model_loader_->input_shapes()[0].h(), static_cast<uint32_t>(data->frame.height * 1.5))
                << "Your offline model can not deal with this frame, frame size mismatch.";
            // yuv, copy to batch memory
            ret = cnrtMemcpy(output_data, plane_y, data->frame.GetPlaneBytes(0), CNRT_MEM_TRANS_DIR_DEV2DEV);
            CHECK_EQ(CNRT_RET_SUCCESS, ret)
                << "offset:" << offset << " output data:" << output_data << " plane_y:" << plane_y
                << " plane bytes: " << data->frame.GetPlaneBytes(0);
            output_data =
                reinterpret_cast<void*>(reinterpret_cast<uint64_t>(output_data) + data->frame.GetPlaneBytes(0));
            ret = cnrtMemcpy(output_data, plane_uv, data->frame.GetPlaneBytes(1), CNRT_MEM_TRANS_DIR_DEV2DEV);
            CHECK_EQ(CNRT_RET_SUCCESS, ret)
                << "offset:" << offset << " output data:" << output_data << " plane_uv:" << plane_uv
                << " plane bytes: " << data->frame.GetPlaneBytes(1);
          } else {
            // run resize and convert.
            try {
              d_ptr_->CheckAndUpdateRCOp(data);
              pctx->rc_op.InvokeOp(output_data, plane_y, plane_uv);
              ret = cnrtSyncStream(pctx->rc_op.cnrt_stream());
              if (CNRT_RET_SUCCESS != ret) {
                throw libstream::StreamlibsError("mlu preprocess failed. error code: " + std::to_string(ret));
              }
            } catch (libstream::StreamlibsError& e) {
              PostEvent(EVENT_ERROR, e.what());
              return -1;
            }
          }
          break;
        }
        default:
          PostEvent(EVENT_ERROR, "Unsupported data format:" + std::to_string(fmt));
          return -1;
      }
    }

    /* batching */
    pctx->vec_data.push_back(data);
    if (pctx->vec_data.size() == d_ptr_->bsize_) {
      ProcessBatch();
      pctx->vec_data.clear();
    }
  }
  return 1;
}

int Inferencer::ProcessBatch() {
  InferContext* pctx = d_ptr_->GetInferContext();
  LOG_IF(FATAL, pctx->vec_data.size() > d_ptr_->bsize_);  // assert failure, check Process
  if (pctx->vec_data.size() == 0) return 1;
  const uint32_t bsize = pctx->vec_data.size();
  try {
    if (d_ptr_->cpu_preproc_.get() != nullptr) {
      /* data from host, copy to device first */
      pctx->mem_op.memcpy_input_h2d(pctx->cpu_input, pctx->mlu_input, bsize);
    }

    /* inference by offline model */
    pctx->infer.run(pctx->mlu_input, pctx->mlu_output);

    /* copy results to host */
    pctx->mem_op.memcpy_output_d2h(pctx->mlu_output, pctx->cpu_output, bsize);

    /* structed inference results */
    if (d_ptr_->post_proc_.get() != nullptr) {
      auto shapes = d_ptr_->model_loader_->output_shapes();
      for (size_t bi = 0; bi < bsize; ++bi) {
        std::vector<float*> results;
        for (size_t output_i = 0; output_i < shapes.size(); ++output_i) {
          uint64_t offset = shapes[output_i].DataCount();
          results.push_back(reinterpret_cast<float*>(pctx->cpu_output[output_i]) + bi * offset);
        }
        d_ptr_->post_proc_->Execute(results, d_ptr_->model_loader_, pctx->vec_data[bi]);
      }
    }
  } catch (libstream::StreamlibsError& e) {
    PostEvent(EVENT_ERROR, e.what());
    return -1;
  }

  /* transmit data */
  for (auto& data : pctx->vec_data) {
    if (container_) container_->ProvideData(this, data);
  }
  return 1;
}

}  // namespace cnstream
