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
#include <glog/logging.h>
#include <memory>
#include <vector>
#include "cnstream_frame.hpp"
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

YUVSplitBatchingStage::YUVSplitBatchingStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                             std::shared_ptr<MluInputResource> mlu_input_res)
    : IOBatchingStage(model, batchsize, mlu_input_res) {}

YUVSplitBatchingStage::~YUVSplitBatchingStage() {}

void YUVSplitBatchingStage::ProcessOneFrame(std::shared_ptr<CNFrameInfo> finfo, uint32_t batch_idx,
                                            const IOResValue& value) {
  CHECK_EQ(value.datas.size(), 2) << "Internel error, yuv split model. input number not 2";
  // copy y plane
  void* dst_y = value.datas[0].Offset(batch_idx);
  void* src_y = finfo->frame.data[0]->GetMutableMluData();
  cnrtRet_t ret = cnrtMemcpy(dst_y, src_y, finfo->frame.GetPlaneBytes(0), CNRT_MEM_TRANS_DIR_DEV2DEV);
  CHECK_EQ(ret, CNRT_RET_SUCCESS) << "memcpy d2d failed. dst, src, size:" << dst_y << ", " << src_y << ", "
                                  << finfo->frame.GetPlaneBytes(0);
  void* dst_uv = value.datas[1].Offset(batch_idx);
  void* src_uv = finfo->frame.data[1]->GetMutableMluData();
  ret = cnrtMemcpy(dst_uv, src_uv, finfo->frame.GetPlaneBytes(1), CNRT_MEM_TRANS_DIR_DEV2DEV);
  CHECK_EQ(ret, CNRT_RET_SUCCESS) << "memcpy d2d failed. dst, src, size:" << dst_uv << ", " << src_uv << ", "
                                  << finfo->frame.GetPlaneBytes(1);
}

YUVPackedBatchingStage::YUVPackedBatchingStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                               std::shared_ptr<MluInputResource> mlu_input_res)
    : IOBatchingStage(model, batchsize, mlu_input_res) {}

YUVPackedBatchingStage::~YUVPackedBatchingStage() {}

void YUVPackedBatchingStage::ProcessOneFrame(std::shared_ptr<CNFrameInfo> finfo, uint32_t batch_idx,
                                             const IOResValue& value) {
  CHECK_EQ(value.datas.size(), 1) << "Internel error, yuv packed model. input number not 1";
  // copy y plane
  void* dst_y = value.datas[0].Offset(batch_idx);
  void* src_y = finfo->frame.data[0]->GetMutableMluData();
  cnrtRet_t ret = cnrtMemcpy(dst_y, src_y, finfo->frame.GetPlaneBytes(0), CNRT_MEM_TRANS_DIR_DEV2DEV);
  CHECK_EQ(ret, CNRT_RET_SUCCESS) << "memcpy d2d failed. dst, src, size:" << dst_y << ", " << src_y << ", "
                                  << finfo->frame.GetPlaneBytes(0);
  void* dst_uv = reinterpret_cast<void*>(reinterpret_cast<char*>(dst_y) + value.datas[0].shape.hw() / 3 * 2);
  void* src_uv = finfo->frame.data[1]->GetMutableMluData();
  ret = cnrtMemcpy(dst_uv, src_uv, finfo->frame.GetPlaneBytes(1), CNRT_MEM_TRANS_DIR_DEV2DEV);
  CHECK_EQ(ret, CNRT_RET_SUCCESS) << "memcpy d2d failed. dst, src, size, y offset:" << dst_uv << ", " << src_uv << ", "
                                  << finfo->frame.GetPlaneBytes(1) << ", " << value.datas[0].shape.hw() / 3 * 2;
}

ResizeConvertBatchingStage::ResizeConvertBatchingStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                                       int dev_id, std::shared_ptr<RCOpResource> rcop_res)
    : BatchingStage(model, batchsize), rcop_res_(rcop_res), dev_id_(dev_id) {}

ResizeConvertBatchingStage::~ResizeConvertBatchingStage() {}

std::shared_ptr<InferTask> ResizeConvertBatchingStage::Batching(std::shared_ptr<CNFrameInfo> finfo) {
  void* src_y = finfo->frame.data[0]->GetMutableMluData();
  void* src_uv = finfo->frame.data[1]->GetMutableMluData();
  QueuingTicket ticket = rcop_res_->PickUpTicket();
  std::shared_ptr<RCOpValue> value = rcop_res_->WaitResourceByTicket(&ticket);
  edk::MluResizeConvertOp::ColorMode cmode = edk::MluResizeConvertOp::ColorMode::YUV2ABGR_NV12;
  if (finfo->frame.fmt == CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12) {
    cmode = edk::MluResizeConvertOp::ColorMode::YUV2RGBA_NV12;
  } else if (finfo->frame.fmt == CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21) {
    cmode = edk::MluResizeConvertOp::ColorMode::YUV2RGBA_NV21;
  } else {
    throw CnstreamError("Can not handle this frame with format :" + std::to_string(static_cast<int>(finfo->frame.fmt)));
  }
  if (!rcop_res_->Initialized()) {
    uint32_t src_w = finfo->frame.width;
    uint32_t src_h = finfo->frame.height;
    uint32_t src_stride = finfo->frame.stride[0];
    uint32_t dst_w = model_->InputShapes()[0].w;
    uint32_t dst_h = model_->InputShapes()[0].h;
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(dev_id_);
    mlu_ctx.ConfigureForThisThread();
    edk::CoreVersion core_ver = mlu_ctx.GetCoreVersion();
    rcop_res_->Init(src_w, src_h, src_stride, dst_w, dst_h, cmode, core_ver);
  } else {
    edk::MluResizeConvertOp::Attr rc_attr = value->op.GetAttr();
    if (static_cast<int>(rc_attr.src_w) != finfo->frame.width ||
        static_cast<int>(rc_attr.src_h) != finfo->frame.height ||
        static_cast<int>(rc_attr.src_stride) != finfo->frame.stride[0] || cmode != rc_attr.color_mode) {
      throw CnstreamError(
          "Resize convert operator should be reinitialized, but we can not do this."
          " Maybe you have different attributes between each frame, wo can not use mlu preprocessing to deal with "
          "this.");
    }
  }
  value->op.BatchingUp(src_y, src_uv);
  rcop_res_->DeallingDone();
  return NULL;
}

ScalerBatchingStage::ScalerBatchingStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                         std::shared_ptr<MluInputResource> mlu_input_res)
    : IOBatchingStage(model, batchsize, mlu_input_res) {}

ScalerBatchingStage::~ScalerBatchingStage() {}

void ScalerBatchingStage::ProcessOneFrame(std::shared_ptr<CNFrameInfo> finfo, uint32_t batch_idx,
                                          const IOResValue& value) {
  void* src_y = finfo->frame.data[0]->GetMutableMluData();
  void* src_uv = finfo->frame.data[1]->GetMutableMluData();
  void* dst = value.datas[0].Offset(batch_idx);
  cncodecWorkInfo work_info;
  cncodecFrame src_frame;
  cncodecFrame dst_frame;
  memset(&work_info, 0, sizeof(work_info));
  memset(&src_frame, 0, sizeof(src_frame));
  memset(&dst_frame, 0, sizeof(dst_frame));

  src_frame.pixelFmt = CNCODEC_PIX_FMT_NV21;
  src_frame.colorSpace = CNCODEC_COLOR_SPACE_BT_709;
  src_frame.width = finfo->frame.width;
  src_frame.height = finfo->frame.height;
  src_frame.planeNum = finfo->frame.GetPlanes();
  src_frame.plane[0].size = finfo->frame.GetPlaneBytes(0);
  src_frame.plane[0].addr = reinterpret_cast<u64_t>(src_y);
  src_frame.plane[1].size = finfo->frame.GetPlaneBytes(1);
  src_frame.plane[1].addr = reinterpret_cast<u64_t>(src_uv);
  src_frame.stride[0] = finfo->frame.stride[0];
  src_frame.stride[1] = finfo->frame.stride[1];
  src_frame.channel = 1;
  src_frame.deviceId = 0;  // FIXME

  const auto sp = value.datas[0].shape;  // model input shape
  auto align_to_128 = [](uint32_t x) { return (x + 127) & ~127; };
  auto row_align = align_to_128(sp.w * 4);
  dst_frame.width = sp.w;
  dst_frame.height = sp.h;
  dst_frame.pixelFmt = CNCODEC_PIX_FMT_ARGB;
  dst_frame.planeNum = 1;
  dst_frame.plane[0].size = row_align * sp.h;
  dst_frame.stride[0] = row_align;
  dst_frame.plane[0].addr = reinterpret_cast<u64_t>(dst);

  work_info.inMsg.instance = 0;

  auto ret = cncodecImageTransform(&dst_frame, nullptr, &src_frame, nullptr, CNCODEC_Filter_BiLinear, &work_info);

  if (CNCODEC_SUCCESS != ret) {
    throw CnstreamError("scaler failed, error code:" + std::to_string(ret));
  }
}

}  // namespace cnstream
