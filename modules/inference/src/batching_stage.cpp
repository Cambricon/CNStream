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

#include "batching_stage.hpp"
#include <cn_codec_common.h>
#include <cnrt.h>
#include <easyinfer/model_loader.h>
#include <memory>
#include <vector>
#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "infer_resource.hpp"
#include "infer_task.hpp"
#include "preproc.hpp"

namespace cnstream {

std::shared_ptr<InferTask> IOBatchingStage::Batching(std::shared_ptr<CNFrameInfo> finfo) {
  bool reserve_ticket = false;
  if (batch_idx_ + 1 == batchsize_) {
    // ready to next batch, do not reserve resource ticket.
    reserve_ticket = false;
  } else {
    // in one batch, reserve resource ticket to parallel.
    reserve_ticket = true;
  }
  QueuingTicket ticket = output_res_->PickUpTicket(reserve_ticket);
  auto bidx = batch_idx_;
  std::shared_ptr<InferTask> task = std::make_shared<InferTask>([this, ticket, finfo, bidx]() -> int {
    QueuingTicket t = ticket;
    IOResValue value = this->output_res_->WaitResourceByTicket(&t);
    this->ProcessOneFrame(finfo, bidx, value);
    this->output_res_->DeallingDone();
    return 0;
  });
  task->task_msg = "infer task.";
  batch_idx_ = (batch_idx_ + 1) % batchsize_;
  return task;
}

CpuPreprocessingBatchingStage::CpuPreprocessingBatchingStage(std::shared_ptr<edk::ModelLoader> model,
                                                             uint32_t batchsize, std::shared_ptr<Preproc> preprocessor,
                                                             std::shared_ptr<CpuInputResource> cpu_input_res)
    : IOBatchingStage(model, batchsize, cpu_input_res), preprocessor_(preprocessor) {}

CpuPreprocessingBatchingStage::~CpuPreprocessingBatchingStage() {}

void CpuPreprocessingBatchingStage::ProcessOneFrame(std::shared_ptr<CNFrameInfo> finfo, uint32_t batch_idx,
                                                    const IOResValue& value) {
  std::vector<float*> net_inputs;
  for (auto it : value.datas) {
    net_inputs.push_back(reinterpret_cast<float*>(it.Offset(batch_idx)));
  }
  preprocessor_->Execute(net_inputs, model_, finfo);
}

ResizeConvertBatchingStage::ResizeConvertBatchingStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                                       int dev_id, std::shared_ptr<RCOpResource> rcop_res)
    : BatchingStage(model, batchsize), rcop_res_(rcop_res), dev_id_(dev_id) {}

ResizeConvertBatchingStage::~ResizeConvertBatchingStage() {}

std::shared_ptr<InferTask> ResizeConvertBatchingStage::Batching(std::shared_ptr<CNFrameInfo> finfo) {
  if (cnstream::IsStreamRemoved(finfo->stream_id)) {
    return NULL;
  }
  CNDataFramePtr frame = finfo->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
  if (frame->fmt != CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12 &&
      frame->fmt != CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21) {
    throw CnstreamError("Can not handle this frame with format :" + std::to_string(static_cast<int>(frame->fmt)));
  }

  // make sure device_id set
  frame->data[0]->SetMluDevContext(dev_id_);
  frame->data[1]->SetMluDevContext(dev_id_);
  void* src_y = const_cast<void*>(frame->data[0]->GetMluData());
  void* src_uv = const_cast<void*>(frame->data[1]->GetMluData());
  QueuingTicket ticket = rcop_res_->PickUpTicket();
  std::shared_ptr<RCOpValue> value = rcop_res_->WaitResourceByTicket(&ticket);
  if (!rcop_res_->Initialized()) {
    uint32_t dst_w = model_->InputShape(0).W();
    uint32_t dst_h = model_->InputShape(0).H();
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(dev_id_);
    mlu_ctx.BindDevice();
    edk::CoreVersion core_ver = mlu_ctx.GetCoreVersion();
    rcop_res_->Init(dst_w, dst_h, frame->fmt, core_ver);
  } else {
    if (frame->fmt != rcop_res_->SrcFmt()) {
      throw CnstreamError(
          "Resize convert operator should be reinitialized, but we can not do this."
          " Maybe you have different pixel format between each frame, we can not use mlu preprocessing to deal with "
          "this.");
    }
  }
  edk::MluResizeConvertOp::InputData input_data;
  input_data.src_w = frame->width;
  input_data.src_h = frame->height;
  input_data.src_stride = frame->stride[0];
  input_data.planes[0] = src_y;
  input_data.planes[1] = src_uv;

  value->op.BatchingUp(input_data);
  rcop_res_->DeallingDone();
  return NULL;
}

ScalerBatchingStage::ScalerBatchingStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, int dev_id,
                                         std::shared_ptr<MluInputResource> mlu_input_res)
    : IOBatchingStage(model, batchsize, mlu_input_res), dev_id_(dev_id) {}

ScalerBatchingStage::~ScalerBatchingStage() {}

void ScalerBatchingStage::ProcessOneFrame(std::shared_ptr<CNFrameInfo> finfo, uint32_t batch_idx,
                                          const IOResValue& value) {
  CNDataFramePtr frame = finfo->collection.Get<CNDataFramePtr>(kCNDataFrameTag);

  // make sure device_id set
  frame->data[0]->SetMluDevContext(0);
  frame->data[1]->SetMluDevContext(0);
  void* src_y = const_cast<void*>(frame->data[0]->GetMluData());
  void* src_uv = const_cast<void*>(frame->data[1]->GetMluData());
  void* dst = value.datas[0].Offset(batch_idx);
  cncodecWorkInfo work_info;
  cncodecFrame src_frame;
  cncodecFrame dst_frame;
  memset(&work_info, 0, sizeof(work_info));
  memset(&src_frame, 0, sizeof(src_frame));
  memset(&dst_frame, 0, sizeof(dst_frame));

  cncodecPixelFormat fmt = CNCODEC_PIX_FMT_NV12;
  switch (frame->fmt) {
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
      fmt = CNCODEC_PIX_FMT_NV21;
      break;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
      fmt = CNCODEC_PIX_FMT_NV12;
      break;
    default:
      LOGE(INFERENCER) << "Scaler: unsupport fmt: " << static_cast<int>(frame->fmt);
      break;
  }

  src_frame.pixelFmt = fmt;
  src_frame.colorSpace = CNCODEC_COLOR_SPACE_BT_709;
  src_frame.width = frame->width;
  src_frame.height = frame->height;
  src_frame.planeNum = frame->GetPlanes();
  src_frame.plane[0].size = frame->GetPlaneBytes(0);
  src_frame.plane[0].addr = reinterpret_cast<u64_t>(src_y);
  src_frame.plane[1].size = frame->GetPlaneBytes(1);
  src_frame.plane[1].addr = reinterpret_cast<u64_t>(src_uv);
  src_frame.stride[0] = frame->stride[0];
  src_frame.stride[1] = frame->stride[1];
  src_frame.channel = 1;
  src_frame.deviceId = dev_id_;

  const auto sp = value.datas[0].shape;  // model input shape
  auto align_to_128 = [](uint32_t x) { return (x + 127) & ~127; };
  auto row_align = align_to_128(sp.W() * 4);
  dst_frame.width = sp.W();
  dst_frame.height = sp.H();
  dst_frame.pixelFmt = CNCODEC_PIX_FMT_ARGB;
  dst_frame.planeNum = 1;
  dst_frame.plane[0].size = row_align * sp.H();
  dst_frame.stride[0] = row_align;
  dst_frame.plane[0].addr = reinterpret_cast<u64_t>(dst);
  dst_frame.deviceId = dev_id_;

  work_info.inMsg.instance = 0;
  work_info.inMsg.cardId = dev_id_;

  auto ret = cncodecImageTransform(&dst_frame, nullptr, &src_frame, nullptr, CNCODEC_Filter_BiLinear, &work_info);

  if (CNCODEC_SUCCESS != ret) {
    throw CnstreamError("scaler failed, error code:" + std::to_string(ret));
  }
}

}  // namespace cnstream
