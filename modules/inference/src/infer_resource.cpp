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

#include "infer_resource.hpp"
#include <cnrt.h>
#include <glog/logging.h>
#include <memory>
#include <string>
#include "cnstream_error.hpp"
#include "inferencer.hpp"

namespace cnstream {

IOResource::IOResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize)
    : InferResource<IOResValue>(model, batchsize) {}

IOResource::~IOResource() {}

CpuInputResource::CpuInputResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize)
    : IOResource(model, batchsize) {}

CpuInputResource::~CpuInputResource() {}

IOResValue CpuInputResource::Allocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize) {
  int input_num = model->InputNum();
  edk::MluMemoryOp mem_op;
  mem_op.SetLoader(model);
  IOResValue value;
  value.datas.resize(input_num);
  value.ptrs = mem_op.AllocCpuInput(1);
  for (int input_idx = 0; input_idx < input_num; ++input_idx) {
    value.datas[input_idx].ptr = value.ptrs[input_idx];
    value.datas[input_idx].shape = model->InputShapes()[input_idx];
    value.datas[input_idx].batch_offset = static_cast<size_t>(value.datas[input_idx].shape.hwc()) * sizeof(float);
    value.datas[input_idx].batchsize = batchsize;
  }
  return value;
}

void CpuInputResource::Deallocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                  const IOResValue& value) {
  edk::MluMemoryOp mem_op;
  mem_op.SetLoader(model);
  if (value.ptrs) mem_op.FreeCpuInput(value.ptrs);
}

CpuOutputResource::CpuOutputResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize)
    : IOResource(model, batchsize) {}

CpuOutputResource::~CpuOutputResource() {}

IOResValue CpuOutputResource::Allocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize) {
  int output_num = model->OutputNum();
  edk::MluMemoryOp mem_op;
  mem_op.SetLoader(model);
  IOResValue value;
  value.datas.resize(output_num);
  value.ptrs = mem_op.AllocCpuOutput(1);
  for (int output_idx = 0; output_idx < output_num; ++output_idx) {
    value.datas[output_idx].ptr = value.ptrs[output_idx];
    value.datas[output_idx].shape = model->OutputShapes()[output_idx];
    value.datas[output_idx].batch_offset =
        static_cast<size_t>(value.datas[output_idx].shape.nhwc() / batchsize) * sizeof(float);
    value.datas[output_idx].batchsize = batchsize;
  }
  return value;
}

void CpuOutputResource::Deallocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                   const IOResValue& value) {
  edk::MluMemoryOp mem_op;
  mem_op.SetLoader(model);
  if (value.ptrs) mem_op.FreeCpuOutput(value.ptrs);
}

MluInputResource::MluInputResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize)
    : IOResource(model, batchsize) {}

MluInputResource::~MluInputResource() {}

IOResValue MluInputResource::Allocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize) {
  int input_num = model->InputNum();
  edk::MluMemoryOp mem_op;
  mem_op.SetLoader(model);
  IOResValue value;
  value.datas.resize(input_num);
  value.ptrs = mem_op.AllocMluInput(1);
  for (int input_idx = 0; input_idx < input_num; ++input_idx) {
    value.datas[input_idx].ptr = value.ptrs[input_idx];
    value.datas[input_idx].shape = model->InputShapes()[input_idx];
    value.datas[input_idx].batch_offset = model->GetInputDataBatchAlignSize(input_idx);
    value.datas[input_idx].batchsize = batchsize;
  }
  return value;
}

void MluInputResource::Deallocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                  const IOResValue& value) {
  int input_num = model->InputNum();
  edk::MluMemoryOp mem_op;
  mem_op.SetLoader(model);
  if (value.ptrs) mem_op.FreeArrayMlu(value.ptrs, input_num);
}

MluOutputResource::MluOutputResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize)
    : IOResource(model, batchsize) {}

MluOutputResource::~MluOutputResource() {}

IOResValue MluOutputResource::Allocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize) {
  int output_num = model->OutputNum();
  edk::MluMemoryOp mem_op;
  mem_op.SetLoader(model);
  IOResValue value;
  value.datas.resize(output_num);
  value.ptrs = mem_op.AllocMluOutput(1);
  for (int output_idx = 0; output_idx < output_num; ++output_idx) {
    value.datas[output_idx].ptr = value.ptrs[output_idx];
    value.datas[output_idx].shape = model->OutputShapes()[output_idx];
    value.datas[output_idx].batch_offset = model->GetOutputDataBatchAlignSize(output_idx);
    value.datas[output_idx].batchsize = batchsize;
  }
  return value;
}

void MluOutputResource::Deallocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                   const IOResValue& value) {
  int input_num = model->InputNum();
  edk::MluMemoryOp mem_op;
  mem_op.SetLoader(model);
  if (value.ptrs) mem_op.FreeArrayMlu(value.ptrs, input_num);
}

RCOpResource::RCOpResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, bool keep_aspect_ratio)
    : InferResource(model, batchsize), keep_aspect_ratio_(keep_aspect_ratio) {
  value_ = std::make_shared<RCOpValue>();
}

RCOpResource::~RCOpResource() {
  if (Initialized()) {
    Destroy();
  }
}

void RCOpResource::Init(uint32_t dst_w, uint32_t dst_h,
                        edk::MluResizeConvertOp::ColorMode cmode, edk::CoreVersion core_ver) {
  if (Initialized()) {
    Destroy();
  }
  edk::MluResizeConvertOp::Attr op_attr;
  op_attr.dst_w = dst_w;
  op_attr.dst_h = dst_h;
  op_attr.color_mode = cmode;
  op_attr.batch_size = batchsize_;
  op_attr.core_version = core_ver;
  op_attr.keep_aspect_ratio = keep_aspect_ratio_;
  value_->op.Init(op_attr);
  value_->initialized = true;
}

void RCOpResource::Destroy() {
  value_->op.Destroy();
  value_->initialized = false;
}

}  // namespace cnstream
