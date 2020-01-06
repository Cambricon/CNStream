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
#include <utility>
#include <vector>

namespace edk {
class ModelLoader;
class EasyInfer;
class MluTaskQueue;
}  // namespace edk

namespace cnstream {

class Postproc;
class CpuInputResource;
class CpuOutputResource;
class MluInputResource;
class MluOutputResource;
class RCOpResource;
class InferTask;
class CNFrameInfo;
class FrameInfoResource;

using BatchingDoneInput = std::vector<std::pair<std::shared_ptr<CNFrameInfo>, std::shared_ptr<std::promise<void>>>>;

class BatchingDoneStage {
 public:
  BatchingDoneStage() = default;
  BatchingDoneStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, int dev_id)
      : model_(model), batchsize_(batchsize), dev_id_(dev_id) {}
  virtual ~BatchingDoneStage() {}

  virtual std::vector<std::shared_ptr<InferTask>> BatchingDone(const BatchingDoneInput& finfos) = 0;

 protected:
  std::shared_ptr<edk::ModelLoader> model_;
  uint32_t batchsize_ = 0;
  int dev_id_ = -1;  // only for EasyInfer::Init
};                   // class BatchingDoneStage

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
  InferBatchingDoneStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, int dev_id,
                         std::shared_ptr<MluInputResource> mlu_input_res,
                         std::shared_ptr<MluOutputResource> mlu_output_res);
  ~InferBatchingDoneStage();

  std::vector<std::shared_ptr<InferTask>> BatchingDone(const BatchingDoneInput& finfos);

  std::shared_ptr<edk::MluTaskQueue> SharedMluQueue() const;

 private:
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

  std::vector<std::shared_ptr<InferTask>> BatchingDone(const BatchingDoneInput& finfos);

 private:
  std::shared_ptr<Postproc> postprocessor_;
  std::shared_ptr<CpuOutputResource> cpu_output_res_;
};  // class PostprocessingBatchingDoneStage

}  // namespace cnstream

#endif  // MODULES_INFERENCE_SRC_BATCHING_DONE_STAGE_HPP_
