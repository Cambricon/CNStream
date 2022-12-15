/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_INFERENCER_HPP_
#define MODULES_INFERENCER_HPP_
/**
 *  This file contains a declaration of class Inferencer
 */

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cnstream_core.hpp"
#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "cnstream_module.hpp"
#include "cnstream_postproc.hpp"
#include "cnstream_preproc.hpp"
#include "object_filter_video.hpp"
#include "private/cnstream_param.hpp"

namespace cnstream {

using InferVideoPixelFmt = infer_server::NetworkInputFormat;
using InferBatchStrategy = infer_server::BatchStrategy;
/**
 * @brief inference parameters used in Inferencer Module.
 */
typedef struct InferParams {
  uint32_t device_id = 0;
  uint32_t priority = 0;
  uint32_t engine_num = 1;
  uint32_t interval = 0;
  bool show_stats = false;
  InferBatchStrategy batch_strategy = InferBatchStrategy::DYNAMIC;
  uint32_t batch_timeout = 1000;  ///< only support in dynamic batch strategy
  InferVideoPixelFmt input_format = infer_server::NetworkInputFormat::BGR;
  std::string model_path = "";
  std::vector<std::string> label_path;

  std::string preproc_name = "";
  bool preproc_use_cpu = true;

  std::string postproc_name = "";
  float threshold = 0.f;

  std::string filter_name = "";
  std::vector<std::string> filter_categories;
  std::unordered_map<std::string, std::string> custom_preproc_params;
  std::unordered_map<std::string, std::string> custom_postproc_params;
} InferParams;

class InferObserver;

/**
 * @brief for inference based on infer_server.
 */
class Inferencer : public ModuleEx,
                   public ModuleCreator<Inferencer>,
                   public infer_server::IPreproc,
                   public infer_server::IPostproc {
 public:
  /**
   *  @brief  Generate Inferencer
   *
   *  @param  Name : Module name
   *
   *  @return None
   */
  explicit Inferencer(const std::string &name);
  virtual ~Inferencer();

  /**
   * @brief Called by pipeline when pipeline start.
   *
   * @param param_set: parameters for this module.
   *
   * @return whether module open succeed.
   */
  bool Open(ModuleParamSet param_set) override;

  /**
   * @brief Called by pipeline when pipeline end.
   *
   * @return void.
   */
  void Close() override;

  /**
   * @brief Process each data frame.
   *
   * @param data : Pointer to the frame info.
   *
   * @return whether post data to communicate processor succeed.
   *
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

  /**
   * @brief Check ParamSet for this module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Return true if this API run successfully. Otherwise, return false.
   */
  bool CheckParamSet(const ModuleParamSet& paramSet) const override;

  // user preproc
  int OnTensorParams(const infer_server::CnPreprocTensorParams *params) override {
    if (preproc_) {
      return preproc_->OnTensorParams(params);
    }
    return -1;
  }
  int OnPreproc(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
                const std::vector<CnedkTransformRect> &src_rects) override {
    if (preproc_) {
      return preproc_->Execute(src, dst, src_rects);
    }
    return -1;
  }
  // user postproc
  int OnPostproc(const std::vector<infer_server::InferData *> &data_vec,
                 const infer_server::ModelIO &model_output,
                 const infer_server::ModelInfo *model_info) override;

  void OnProcessDone(const CNFrameInfoPtr data) {
    this->TransmitData(data);
  }

 private:
  std::unique_ptr<ModuleParamsHelper<InferParams>> param_helper_ = nullptr;
  std::unique_ptr<infer_server::InferServer> server_ = nullptr;
  infer_server::Session_t session_ = nullptr;
  std::shared_ptr<InferObserver> observer_ = nullptr;
  std::shared_ptr<ObjectFilterVideo> filter_ = nullptr;
  std::shared_ptr<Preproc> preproc_ = nullptr;
  std::shared_ptr<Postproc> postproc_ = nullptr;
  LabelStrings label_strings_;
  std::map<std::string, uint32_t> drop_cnt_map_;
  std::mutex drop_cnt_map_mtx_;
};  // class Inferencer

}  // namespace cnstream
#endif
