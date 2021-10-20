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

#ifndef MODULES_INFERENCE_SRC_INFER_ENGINE_HPP_
#define MODULES_INFERENCE_SRC_INFER_ENGINE_HPP_

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "batching_done_stage.hpp"
#include "cnstream_frame_va.hpp"
#include "timeout_helper.hpp"

namespace edk {
class ModelLoader;
}  // namespace edk

namespace cnstream {

class BatchingStage;
class ObjBatchingStage;
class MluInputResource;
class CpuOutputResource;
class RCOpResource;
class ScalerResource;
class Preproc;
class ObjFilter;
class ObjPreproc;
class Postproc;
class ObjPostproc;
class InferThreadPool;
class CNFrameInfo;
class ModuleProfiler;

class InferEngine {
 public:
  class ResultWaitingCard {
   public:
    explicit ResultWaitingCard(std::shared_ptr<std::promise<void>> ret_promise) : promise_(ret_promise) {}
    void WaitForCall();

   private:
    std::shared_ptr<std::promise<void>> promise_;
  };  // class ResultWaitingCard
  InferEngine(int dev_id, std::shared_ptr<edk::ModelLoader> model, std::shared_ptr<Preproc> preprocessor,
              std::shared_ptr<Postproc> postprocessor, uint32_t batchsize, uint32_t batching_timeout, bool use_scaler,
              std::string infer_thread_id, const std::function<void(const std::string& err_msg)>& error_func = NULL,
              bool keep_aspect_ratio = false, bool batching_by_obj = false,
              const std::shared_ptr<ObjPreproc>& obj_preprocessor = nullptr,
              const std::shared_ptr<ObjPostproc>& obj_postprocessor = nullptr,
              const std::shared_ptr<ObjFilter>& obj_filter = nullptr, std::string dump_resized_image_dir = "",
              CNDataFormat model_input_pixel_format = CNDataFormat::CN_PIXEL_FORMAT_RGBA32,
              bool mem_on_mlu_for_postproc = false, bool saving_infer_input = false, std::string module_name = "",
              ModuleProfiler* profiler = nullptr, int pad_method = 0);
  ~InferEngine();
  ResultWaitingCard FeedData(std::shared_ptr<CNFrameInfo> finfo);

  void ForceBatchingDone() {
    BatchingDone();
  }

 private:
  void StageAssemble();
  void BatchingDone();
  std::shared_ptr<edk::ModelLoader> model_;
  std::shared_ptr<Preproc> preprocessor_;
  std::shared_ptr<Postproc> postprocessor_;
  const uint32_t batchsize_ = 0;
  const uint32_t batching_timeout_ = 0;
  BatchingDoneInput batched_finfos_;
  /* batch up data , preprocessing */
  std::shared_ptr<BatchingStage> batching_stage_ = nullptr;
  /* （h2d） infer, d2h, postprocessing, transmit data */
  std::vector<std::shared_ptr<BatchingDoneStage>> batching_done_stages_;
  std::shared_ptr<CpuInputResource> cpu_input_res_;
  std::shared_ptr<CpuOutputResource> cpu_output_res_;
  std::shared_ptr<MluInputResource> mlu_input_res_;
  std::shared_ptr<MluOutputResource> mlu_output_res_;
  std::shared_ptr<RCOpResource> rcop_res_;

  TimeoutHelper timeout_helper_;
  std::shared_ptr<InferThreadPool> tp_;
  std::function<void(const std::string& err_msg)> error_func_ = NULL;
  int dev_id_ = 0;
  bool use_scaler_ = false;
  bool batching_by_obj_ = false;
  /* batch up object, preprocessing */
  std::shared_ptr<ObjBatchingStage> obj_batching_stage_ = nullptr;
  std::shared_ptr<ObjPreproc> obj_preprocessor_ = nullptr;
  std::shared_ptr<ObjPostproc> obj_postprocessor_ = nullptr;
  std::shared_ptr<ObjFilter> obj_filter_ = nullptr;
  std::vector<std::shared_ptr<CNInferObject>> batched_objs_;
  std::shared_ptr<ObjPostprocessingBatchingDoneStage> obj_postproc_stage_ = nullptr;
  std::string infer_thread_id_;
  std::string dump_resized_image_dir_ = "";
  CNDataFormat model_input_fmt_ = CNDataFormat::CN_PIXEL_FORMAT_RGBA32;
  uint32_t cached_frame_cnt_ = 0;
  bool mem_on_mlu_for_postproc_ = false;
  bool saving_infer_input_ = false;
  std::string module_name_ = "";
  ModuleProfiler* profiler_ = nullptr;
};  // class InferEngine

}  // namespace cnstream

#endif  // MODULES_INFERENCE_SRC_INFER_ENGINE_HPP_
