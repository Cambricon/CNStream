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

#ifndef MODULES_INFERENCE_INCLUDE_POSTPROC_HPP_
#define MODULES_INFERENCE_INCLUDE_POSTPROC_HPP_

/**
 *  \file postproc.hpp
 *
 *  This file contains a declaration of class Postproc
 */
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "easyinfer/model_loader.h"
#include "reflex_object.h"

#include "cnstream_frame.hpp"

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

namespace cnstream {

using StringPairs = std::vector<std::pair<std::string, std::string>>;
/**
 * @brief construct a pointer to CNFrameInfo
 */
using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;
/**
 * @brief Base class of post process
 */
class Postproc {
 public:
  /**
   * @brief do nothong
   */
  virtual ~Postproc() {}
  /**
   * @brief create relative postprocess
   *
   * @param proc_name postprocess class name
   *
   * @return None
   */
  static Postproc *Create(const std::string &proc_name);

  /**
   * @brief load labels if there's one.
   *
   * @param label_path labels file path
   *
   * @return true for success, false otherwise
   */
  bool LoadLabels(const std::string &label_path);

  /**
   * @brief load multi labels if there's more than one label for one model.
   *
   * @param label_path labels file path
   *
   * @return true for success, false otherwise
   */
  bool LoadMultiLabels(const std::vector<std::string> &label_path);

  /**
   * @brief set threshold
   *
   * @param threshold the value between 0 and 1
   *
   * @return void
   */
  void set_threshold(const float threshold);

  /**
   * @brief Execute postproc on neural network outputs
   *
   * @param net_outputs: neural network outputs
   * @param model: model information(you can get input shape and output shape from model)
   * @param package: smart pointer of struct to store processed result
   *
   * @return return 0 if succeed
   */
  virtual int Execute(const std::vector<float *> &net_outputs, const std::shared_ptr<edk::ModelLoader> &model,
                      const CNFrameInfoPtr &package) {
    return 0;
  }

  /**
   * @brief Execute postproc with neural network outputs
   *
   * @param[in] net_outputs neural network outputs
   * @param[out] result the result parsed from network outputs
   *
   * @return 0 for success, error code otherwise
   */
  virtual int Execute(const std::pair<float *, uint64_t> &net_outputs, StringPairs *result) { return 0; }

  /**
   * @brief Execute postproc with neural network outputs
   *
   * @param[in] net_outputs neural network outputs
   * @param[out] result the result parsed from network outputs
   *
   * @return 0 for success, error code otherwise
   */
  virtual int Execute(const std::vector<std::pair<float *, uint64_t>> &net_outputs, StringPairs *result) { return 0; }

  /**
   * @brief Execute postproc with neural network outputs
   *
   * @param[in] net_outputs neural network outputs
   * @param[out] result the result parsed from network outputs
   *
   * @return 0 for success, error code otherwise
   */
  virtual int Execute(const std::pair<float *, uint64_t> &net_outputs, std::vector<float> *result) { return 0; }

  /**
   * @brief Execute postproc on neural network outputs
   *
   * @param package: smart pointer of struct to store origin data
   * @oaran poutput: pointer of output
   *
   * @return return 0 if succeed
   */
  virtual int Execute(const CNFrameInfoPtr &package, char *poutput, int len) { return 0; }

 protected:
  float threshold_ = 0;
  std::vector<std::string> labels_;
  std::vector<std::vector<std::string>> multi_labels_;
};  // class Postproc

}  // namespace cnstream

#endif  // ifndef MODULES_INFERENCE_INCLUDE_POSTPROC_HPP_
