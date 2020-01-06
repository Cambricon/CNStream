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

#ifndef MODULES_CLASSIFIER_INCLUDE_MULTISTEP_HPP_
#define MODULES_CLASSIFIER_INCLUDE_MULTISTEP_HPP_
/**
 *  \file multistep_classifier.hpp
 *
 *  This file contains a declaration of class MultiStepClassifier
 */

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"
#include "multistep_classifier_impl.hpp"
#include "postprocess/postproc.hpp"
#include "preprocess/preproc.hpp"

/// Pointer for frame info
using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

/**
 * @brief MultiStepClassifier context structer
 */
struct MultiStepClassifierContext {
  cnstream::MultiStepClassifierImpl* impl;
};

/**
 * @brief MultiStepClassifier is a module for multi-step classification.
 */
class MultiStepClassifier : public cnstream::Module, public cnstream::ModuleCreator<MultiStepClassifier> {
 public:
  /**
   *  @brief  Generate MultiStepClassifier
   *
   *  @param  Name : module name
   *
   *  @return None
   */
  explicit MultiStepClassifier(const std::string& name);
  /**
   *  @brief  Release MultiStepClassifier
   *
   *  @param  None
   *
   *  @return None
   */
  ~MultiStepClassifier();

  /**
  * @brief Called by pipeline when pipeline start.
  *
  * @param paramSet :
  @verbatim
     dump_dir: ouput_dir
  @endverbatim
  *
  * @return if module open succeed
  */
  bool Open(cnstream::ModuleParamSet paramSet) override;

  /**
   * @brief  Called by pipeline when pipeline stop
   *
   * @param  None
   *
   * @return  None
   */
  void Close() override;

  /**
   * @brief Encode each frame
   *
   * @param data : data to be processed
   *
   * @return whether process succeed
   * @retval 0: succeed and do no intercept data
   * @retval <0: failed
   */
  int Process(CNFrameInfoPtr data) override;

 private:
  MultiStepClassifierContext* GetMultiStepClassifierContext(CNFrameInfoPtr data);
  bool loadModelAndLableList(const std::string& modle_label_file, const std::string& func_name);
  std::vector<std::string> strAttrsName = {"CarBrand", "CarSeries"};
  std::string model_label_list_path_;

  int device_id_ = 0;
  uint32_t batch_size_ = 1;
  int step1_class_index = 0;

  std::shared_ptr<cnstream::Preproc> preproc_;
  std::shared_ptr<cnstream::Postproc> postproc_;
  std::unordered_map<int, std::string> model_files_;
  std::unordered_map<int, std::string> label_files_;
  std::unordered_map<int, std::shared_ptr<edk::ModelLoader>> model_loaders_;
  std::unordered_map<int, std::vector<std::string>> labels_;
  std::unordered_map<int, MultiStepClassifierContext*> ctxs_;
  std::vector<std::string> matches_;
};  // class MultiStepClassifier

#endif  // MODULES_CLASSIFIER_INCLUDE_MULTISTEP_HPP_
