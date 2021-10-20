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

#ifndef MODULES_INFERENCE_SRC_INFER_RESOURCE_HPP_
#define MODULES_INFERENCE_SRC_INFER_RESOURCE_HPP_

#include <easybang/resize_and_colorcvt.h>
#include <easyinfer/mlu_memory_op.h>
#include <easyinfer/model_loader.h>

#include <memory>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "exception.hpp"
#include "queuing_server.hpp"

namespace cnstream {

template <typename RetT>
class InferResource : public QueuingServer {
 public:
  InferResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize) : model_(model), batchsize_(batchsize) {}
  virtual ~InferResource() {}
  virtual void Init() {}
  virtual void Destroy() {}
  RetT WaitResourceByTicket(QueuingTicket* pticket) {
    WaitByTicket(pticket);
    return value_;
  }
  RetT GetDataDirectly() const { return value_; }

 protected:
  const std::shared_ptr<edk::ModelLoader> model_;
  const uint32_t batchsize_ = 0;
  RetT value_;
};  // class InferResource

struct IOResValue {
  void** ptrs = nullptr;
  struct OneData {
    void* ptr = nullptr;
    edk::ShapeEx shape;
    size_t batch_offset = 0;
    uint32_t batchsize = 0;
    void* Offset(int batch_idx) const {
      return reinterpret_cast<void*>(reinterpret_cast<char*>(ptr) + batch_offset * batch_idx);
    }
  };
  std::vector<OneData> datas;
};  // struct IOResValue

CNSTREAM_REGISTER_EXCEPTION(IOResource);
class IOResource : public InferResource<IOResValue> {
 public:
  IOResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize);
  virtual ~IOResource();

  void Init() override { value_ = Allocate(model_, batchsize_); }

  void Destroy() override { Deallocate(model_, batchsize_, value_); }

 protected:
  virtual IOResValue Allocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize) = 0;
  virtual void Deallocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, const IOResValue& value) = 0;
};  // class IOResource

class CpuInputResource : public IOResource {
 public:
  CpuInputResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize);
  ~CpuInputResource();

 protected:
  IOResValue Allocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize) override;
  void Deallocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, const IOResValue& value) override;
};  // class CpuInputResource

class CpuOutputResource : public IOResource {
 public:
  CpuOutputResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize);
  ~CpuOutputResource();

 protected:
  IOResValue Allocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize) override;
  void Deallocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, const IOResValue& value) override;
};  // class CpuOutputResource

class MluInputResource : public IOResource {
 public:
  MluInputResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize);
  ~MluInputResource();

 protected:
  IOResValue Allocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize) override;
  void Deallocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, const IOResValue& value) override;
};  // class MluInputResource

class MluOutputResource : public IOResource {
 public:
  MluOutputResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize);
  ~MluOutputResource();

 protected:
  IOResValue Allocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize) override;
  void Deallocate(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, const IOResValue& value) override;
};  // class MluOutputResource

struct RCOpValue {
  edk::MluResizeConvertOp op;
  void** y_plane_fake_data = nullptr;
  void** uv_plane_fake_data = nullptr;
  volatile bool initialized = false;
};  // struct RCOpValue

class RCOpResource : public InferResource<std::shared_ptr<RCOpValue>> {
 public:
  RCOpResource(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, bool keep_aspect_ratio,
               CNDataFormat dst_fmt, int pad_method);
  ~RCOpResource();
  bool Initialized() const { return value_->initialized; }
  void SetMluQueue(std::shared_ptr<edk::MluTaskQueue> mlu_queue) { value_->op.SetMluQueue(mlu_queue); }
  void Init() {}
  void Init(uint32_t dst_w, uint32_t dst_h, CNDataFormat src_fmt, edk::CoreVersion core_ver);
  void Destroy();
  CNDataFormat SrcFmt() const { return src_fmt_; }

 private:
  int pad_method_ = 0;
  int core_number_ = 0;
  bool keep_aspect_ratio_ = false;
  CNDataFormat src_fmt_ = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
  CNDataFormat dst_fmt_ = CNDataFormat::CN_PIXEL_FORMAT_RGBA32;
};  // class ResizeOpResource

}  // namespace cnstream

#endif  // MODULES_INFERENCE_SRC_INFER_RESOURCE_HPP_
