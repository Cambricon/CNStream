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
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <bitset>

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
  mem_op.SetModel(model);
  IOResValue value;
  value.datas.resize(input_num);
  value.ptrs = mem_op.AllocCpuInput();
  for (int input_idx = 0; input_idx < input_num; ++input_idx) {
    value.datas[input_idx].ptr = value.ptrs[input_idx];
    value.datas[input_idx].shape = model->InputShape(input_idx);
    value.datas[input_idx].batch_offset = value.datas[input_idx].shape.DataCount() * sizeof(float);
    value.datas[input_idx].batchsize = batchsize;
  }
  return value;
}

void CpuInputResource::Deallocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                  const IOResValue& value) {
  edk::MluMemoryOp mem_op;
  mem_op.SetModel(model);
  if (value.ptrs) mem_op.FreeCpuInput(value.ptrs);
}

CpuOutputResource::CpuOutputResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize)
    : IOResource(model, batchsize) {}

CpuOutputResource::~CpuOutputResource() {}

IOResValue CpuOutputResource::Allocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize) {
  int output_num = model->OutputNum();
  edk::MluMemoryOp mem_op;
  mem_op.SetModel(model);
  IOResValue value;
  value.datas.resize(output_num);
  value.ptrs = mem_op.AllocCpuOutput();
  for (int output_idx = 0; output_idx < output_num; ++output_idx) {
    value.datas[output_idx].ptr = value.ptrs[output_idx];
    value.datas[output_idx].shape = model->OutputShape(output_idx);
    value.datas[output_idx].batch_offset =
        value.datas[output_idx].shape.DataCount() * sizeof(float);
    value.datas[output_idx].batchsize = batchsize;
  }
  return value;
}

void CpuOutputResource::Deallocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                   const IOResValue& value) {
  edk::MluMemoryOp mem_op;
  mem_op.SetModel(model);
  if (value.ptrs) mem_op.FreeCpuOutput(value.ptrs);
}

MluInputResource::MluInputResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize)
    : IOResource(model, batchsize) {}

MluInputResource::~MluInputResource() {}

IOResValue MluInputResource::Allocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize) {
  int input_num = model->InputNum();
  edk::MluMemoryOp mem_op;
  mem_op.SetModel(model);
  IOResValue value;
  value.datas.resize(input_num);
  value.ptrs = mem_op.AllocMluInput();
  for (int input_idx = 0; input_idx < input_num; ++input_idx) {
    value.datas[input_idx].ptr = value.ptrs[input_idx];
    value.datas[input_idx].shape = model->InputShape(input_idx);
    value.datas[input_idx].batch_offset = model->GetInputDataBatchAlignSize(input_idx);
    value.datas[input_idx].batchsize = batchsize;
  }
  return value;
}

void MluInputResource::Deallocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                  const IOResValue& value) {
  edk::MluMemoryOp mem_op;
  mem_op.SetModel(model);
  if (value.ptrs) mem_op.FreeMluInput(value.ptrs);
}

MluOutputResource::MluOutputResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize)
    : IOResource(model, batchsize) {}

MluOutputResource::~MluOutputResource() {}

IOResValue MluOutputResource::Allocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize) {
  int output_num = model->OutputNum();
  edk::MluMemoryOp mem_op;
  mem_op.SetModel(model);
  IOResValue value;
  value.datas.resize(output_num);
  value.ptrs = mem_op.AllocMluOutput();
  for (int output_idx = 0; output_idx < output_num; ++output_idx) {
    value.datas[output_idx].ptr = value.ptrs[output_idx];
    value.datas[output_idx].shape = model->OutputShape(output_idx);
    value.datas[output_idx].batch_offset = model->GetOutputDataBatchAlignSize(output_idx);
    value.datas[output_idx].batchsize = batchsize;
  }
  return value;
}

void MluOutputResource::Deallocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                   const IOResValue& value) {
  edk::MluMemoryOp mem_op;
  mem_op.SetModel(model);
  if (value.ptrs) mem_op.FreeMluOutput(value.ptrs);
}

RCOpResource::RCOpResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, bool keep_aspect_ratio,
                           CNDataFormat dst_fmt, int pad_method)
    : InferResource(model, batchsize),
      pad_method_(pad_method),
      keep_aspect_ratio_(keep_aspect_ratio),
      dst_fmt_(dst_fmt) {
  value_ = std::make_shared<RCOpValue>();
  core_number_ = model_->ModelParallelism();
}

RCOpResource::~RCOpResource() {
  if (Initialized()) {
    Destroy();
  }
}

void RCOpResource::Init(uint32_t dst_w, uint32_t dst_h, CNDataFormat src_fmt,
                        edk::CoreVersion core_ver) {
  enum class ColorCvt : uint64_t {
    YUV2RGBA32_NV21 = static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21) << 32 |
                      static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_RGBA32),
    YUV2RGBA32_NV12 = static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12) << 32 |
                      static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_RGBA32),

    YUV2BGRA32_NV21 = static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21) << 32 |
                      static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_BGRA32),
    YUV2BGRA32_NV12 = static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12) << 32 |
                      static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_BGRA32),

    YUV2ARGB32_NV21 = static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21) << 32 |
                      static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_ARGB32),
    YUV2ARGB32_NV12 = static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12) << 32 |
                      static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_ARGB32),

    YUV2ABGR32_NV21 = static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21) << 32 |
                      static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_ABGR32),
    YUV2ABGR32_NV12 = static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12) << 32 |
                      static_cast<uint64_t>(CNDataFormat::CN_PIXEL_FORMAT_ABGR32)
  };

  ColorCvt color_cvt_mode =
      static_cast<ColorCvt>(static_cast<uint64_t>(src_fmt) << 32 | static_cast<uint64_t>(dst_fmt_));

  static const std::map<decltype(color_cvt_mode), edk::MluResizeConvertOp::ColorMode> cvt_mode_map = {
      std::make_pair(ColorCvt::YUV2RGBA32_NV21, edk::MluResizeConvertOp::ColorMode::YUV2RGBA_NV21),
      std::make_pair(ColorCvt::YUV2RGBA32_NV12, edk::MluResizeConvertOp::ColorMode::YUV2RGBA_NV12),
      std::make_pair(ColorCvt::YUV2BGRA32_NV21, edk::MluResizeConvertOp::ColorMode::YUV2BGRA_NV21),
      std::make_pair(ColorCvt::YUV2BGRA32_NV12, edk::MluResizeConvertOp::ColorMode::YUV2BGRA_NV12),
      std::make_pair(ColorCvt::YUV2ARGB32_NV21, edk::MluResizeConvertOp::ColorMode::YUV2ARGB_NV21),
      std::make_pair(ColorCvt::YUV2ARGB32_NV12, edk::MluResizeConvertOp::ColorMode::YUV2ARGB_NV12),
      std::make_pair(ColorCvt::YUV2ABGR32_NV21, edk::MluResizeConvertOp::ColorMode::YUV2ABGR_NV21),
      std::make_pair(ColorCvt::YUV2ABGR32_NV12, edk::MluResizeConvertOp::ColorMode::YUV2ABGR_NV12)};

  LOGF_IF(INFERENCER, cvt_mode_map.find(color_cvt_mode) == cvt_mode_map.end())
      << "Unsupported color convert mode. src pixel format : " << static_cast<uint64_t>(src_fmt)
      << ", dst pixel format : " << static_cast<uint64_t>(dst_fmt_);

  if (Initialized()) {
    Destroy();
  }
  edk::MluResizeConvertOp::Attr op_attr;
  op_attr.dst_w = dst_w;
  op_attr.dst_h = dst_h;
  op_attr.color_mode = cvt_mode_map.find(color_cvt_mode)->second;
  op_attr.batch_size = batchsize_;
  op_attr.core_version = core_ver;
  op_attr.keep_aspect_ratio = keep_aspect_ratio_;
  op_attr.core_number = core_number_;
  op_attr.padMethod = pad_method_;
  value_->op.Init(op_attr);
  value_->initialized = true;
  src_fmt_ = src_fmt;
}

void RCOpResource::Destroy() {
  value_->op.Destroy();
  value_->initialized = false;
}

}  // namespace cnstream
