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

#ifndef MODULES_INFERENCE_SRC_BATCHING_DONE_STAGE_HPP_
#define MODULES_INFERENCE_SRC_BATCHING_DONE_STAGE_HPP_

#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "cnstream_frame_va.hpp"
#include "profiler/module_profiler.hpp"

namespace edk {
class ModelLoader;
class EasyInfer;
struct MluTaskQueue;
}  // namespace edk

namespace cnstream {

class Postproc;
class ObjPostproc;
class CpuInputResource;
class CpuOutputResource;
class MluInputResource;
class MluOutputResource;
class RCOpResource;
class InferTask;
class CNFrameInfo;
struct CNInferObject;
class FrameInfoResource;

struct AutoSetDone {
  explicit AutoSetDone(const std::shared_ptr<std::promise<void>>& p,
                       std::shared_ptr<CNFrameInfo> data)
      : p_(p), data_(data) {}
  ~AutoSetDone() {
    p_->set_value();
  }
  std::shared_ptr<std::promise<void>> p_;
  std::shared_ptr<CNFrameInfo> data_;
};  // struct AutoSetDone

using BatchingDoneInput = std::vector<std::pair<std::shared_ptr<CNFrameInfo>, std::shared_ptr<AutoSetDone>>>;

class BatchingDoneStage {
 public:
  BatchingDoneStage() = default;
  BatchingDoneStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, int dev_id)
      : model_(model), batchsize_(batchsize), dev_id_(dev_id) {}
  virtual ~BatchingDoneStage() {}

  virtual std::vector<std::shared_ptr<InferTask>> BatchingDone(const BatchingDoneInput& finfos) = 0;
  void SetDumpResizedImageDir(const std::string &dir) {
     dump_resized_image_dir_ = dir;
  }

  void SetSavingInputData(const bool& saving_infer_input, const std::string& module_name) {
    saving_infer_input_ = saving_infer_input;
    module_name_ = module_name;
  }

  ModuleProfiler* profiler_ = nullptr;

 protected:
  std::shared_ptr<edk::ModelLoader> model_;
  uint32_t batchsize_ = 0;
  int dev_id_ = -1;  // only for EasyInfer::Init
  std::string dump_resized_image_dir_ = "";
  bool saving_infer_input_ = false;
  std::string module_name_ = "";
};  // class BatchingDoneStage

class H2DBatchingDoneStage : public BatchingDoneStage {
 public:
  H2DBatchingDoneStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, int dev_id,
                       std::shared_ptr<CpuInputResource> cpu_input_res, std::shared_ptr<MluInputResource> mlu_input_res)
      : BatchingDoneStage(model, batchsize, dev_id), cpu_input_res_(cpu_input_res), mlu_input_res_(mlu_input_res) {}

  std::vector<std::shared_ptr<InferTask>> BatchingDone(const BatchingDoneInput& finfos);

 private:
  std::shared_ptr<CpuInputResource> cpu_input_res_;
  std::shared_ptr<MluInputResource> mlu_input_res_;
};  // class H2DBatchingDoneStage

class ResizeConvertBatchingDoneStage : public BatchingDoneStage {
 public:
  ResizeConvertBatchingDoneStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, int dev_id,
                                 std::shared_ptr<RCOpResource> rcop_res,
                                 std::shared_ptr<MluInputResource> mlu_input_res)
      : BatchingDoneStage(model, batchsize, dev_id), rcop_res_(rcop_res), mlu_input_res_(mlu_input_res) {}

  std::vector<std::shared_ptr<InferTask>> BatchingDone(const BatchingDoneInput& finfos);

 private:
  std::shared_ptr<RCOpResource> rcop_res_;
  std::shared_ptr<MluInputResource> mlu_input_res_;
};  // class ResizeConvertBatchingDoneStage

class InferBatchingDoneStage : public BatchingDoneStage {
 public:
  InferBatchingDoneStage(std::shared_ptr<edk::ModelLoader> model,
                         CNDataFormat model_input_fmt,
                         uint32_t batchsize, int dev_id,
                         std::shared_ptr<MluInputResource> mlu_input_res,
                         std::shared_ptr<MluOutputResource> mlu_output_res);
  ~InferBatchingDoneStage();

  std::vector<std::shared_ptr<InferTask>> BatchingDone(const BatchingDoneInput& finfos);

  std::shared_ptr<edk::MluTaskQueue> SharedMluQueue() const;

 private:
  CNDataFormat model_input_fmt_;
  std::shared_ptr<MluInputResource> mlu_input_res_;
  std::shared_ptr<MluOutputResource> mlu_output_res_;
  std::shared_ptr<edk::EasyInfer> easyinfer_;
};  // class InferBatchingDoneStage

class D2HBatchingDoneStage : public BatchingDoneStage {
 public:
  D2HBatchingDoneStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, int dev_id,
                       std::shared_ptr<MluOutputResource> mlu_output_res,
                       std::shared_ptr<CpuOutputResource> cpu_output_res)
      : BatchingDoneStage(model, batchsize, dev_id), mlu_output_res_(mlu_output_res), cpu_output_res_(cpu_output_res) {}

  std::vector<std::shared_ptr<InferTask>> BatchingDone(const BatchingDoneInput& finfos);

 private:
  std::shared_ptr<MluOutputResource> mlu_output_res_;
  std::shared_ptr<CpuOutputResource> cpu_output_res_;
};  // class D2HBatchingDoneStage

class PostprocessingBatchingDoneStage : public BatchingDoneStage {
 public:
  PostprocessingBatchingDoneStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, int dev_id,
                                  std::shared_ptr<Postproc> postprocessor,
                                  std::shared_ptr<CpuOutputResource> cpu_output_res)
      : BatchingDoneStage(model, batchsize, dev_id), postprocessor_(postprocessor), cpu_output_res_(cpu_output_res) {}

  PostprocessingBatchingDoneStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, int dev_id,
                                  std::shared_ptr<Postproc> postprocessor,
                                  std::shared_ptr<MluOutputResource> mlu_output_res)
      : BatchingDoneStage(model, batchsize, dev_id), postprocessor_(postprocessor), mlu_output_res_(mlu_output_res) {}

  std::vector<std::shared_ptr<InferTask>> BatchingDone(const BatchingDoneInput& finfos) override;
  std::vector<std::shared_ptr<InferTask>> BatchingDone(const BatchingDoneInput& finfos,
                                                       const std::shared_ptr<CpuOutputResource> &cpu_output_res);
  std::vector<std::shared_ptr<InferTask>> BatchingDone(const BatchingDoneInput& finfos,
                                                       const std::shared_ptr<MluOutputResource> &mlu_output_res);

 private:
  std::shared_ptr<Postproc> postprocessor_;
  std::shared_ptr<CpuOutputResource> cpu_output_res_ = nullptr;
  std::shared_ptr<MluOutputResource> mlu_output_res_ = nullptr;
};  // class PostprocessingBatchingDoneStage

class ObjPostprocessingBatchingDoneStage : public BatchingDoneStage {
 public:
  ObjPostprocessingBatchingDoneStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, int dev_id,
                                     std::shared_ptr<ObjPostproc> postprocessor,
                                     std::shared_ptr<CpuOutputResource> cpu_output_res)
      : BatchingDoneStage(model, batchsize, dev_id), postprocessor_(postprocessor), cpu_output_res_(cpu_output_res) {}

  ObjPostprocessingBatchingDoneStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, int dev_id,
                                     std::shared_ptr<ObjPostproc> postprocessor,
                                     std::shared_ptr<MluOutputResource> mlu_output_res)
      : BatchingDoneStage(model, batchsize, dev_id), postprocessor_(postprocessor), mlu_output_res_(mlu_output_res) {}

  std::vector<std::shared_ptr<InferTask>> BatchingDone(const BatchingDoneInput& finfos) override { return {}; }

  std::vector<std::shared_ptr<InferTask>> ObjBatchingDone(const BatchingDoneInput& finfos,
                                                          const std::vector<std::shared_ptr<CNInferObject>>& objs);
  std::vector<std::shared_ptr<InferTask>> ObjBatchingDone(const BatchingDoneInput& finfos,
                                                          const std::vector<std::shared_ptr<CNInferObject>>& objs,
                                                          const std::shared_ptr<CpuOutputResource> &cpu_output_res);
  std::vector<std::shared_ptr<InferTask>> ObjBatchingDone(const BatchingDoneInput& finfos,
                                                          const std::vector<std::shared_ptr<CNInferObject>>& objs,
                                                          const std::shared_ptr<MluOutputResource> &mlu_output_res);

 private:
  std::shared_ptr<ObjPostproc> postprocessor_;
  std::shared_ptr<CpuOutputResource> cpu_output_res_ = nullptr;
  std::shared_ptr<MluOutputResource> mlu_output_res_ = nullptr;
};  // class PostprocessingBatchingDoneStage

}  // namespace cnstream

#endif  // MODULES_INFERENCE_SRC_BATCHING_DONE_STAGE_HPP_
