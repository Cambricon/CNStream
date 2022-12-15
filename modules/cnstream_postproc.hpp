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

#ifndef CNSTREAM_POSTPROC_HPP_
#define CNSTREAM_POSTPROC_HPP_

/**
 *  \file postproc_video.hpp
 *
 *  This file contains a declaration of class Postproc
 */

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "reflex_object.h"

namespace cnstream {

/*!
 * Defines an alias for std::vector<std::vector<std::string>> for labels.
 */
using LabelStrings = std::vector<std::vector<std::string>>;

/*!
 * Defines an alias for std::vector<std::pair<cnedk::BufSurfWrapperPtr, const infer_server::Shape>> for model outputs.
 */
using NetOutputs = std::vector<std::pair<cnedk::BufSurfWrapperPtr, const infer_server::Shape>>;

class Postproc : virtual public ReflexObjectEx<Postproc> {
 public:
  /**
   * @brief Destructs an object.
   *
   * @return No return value.
   */
  virtual ~Postproc() {}

  /**
   * @brief create relative postprocess
   *
   * @param proc_name postprocess class name
   *
   * @return None
   */
  static Postproc* Create(const std::string& name);
  /**
   * @brief Initializes postprocessing parameters.
   *
   * @param[in] params The postprocessing parameters.
   *
   * @return Returns 0 for success, otherwise returns <0.
   **/
  virtual int Init(const std::unordered_map<std::string, std::string> &params) { return 0; }

  /**
   * @brief Execute. You can parse network output data and fill results to frame.
   *
   * Usually, this function is for primary network.
   *
   * @param[in] net_outputs The model output data.
   * @param[in] model_info The model information, e.g., input/output number, shape and etc.
   * @param[inout] packages The smart pointer of struct to store processed result.
   * @param[in] labels The label vector for object ids.
   *
   * @return Returns 0 for success, otherwise returns <0.
   */
  virtual int Execute(const NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                      const std::vector<CNFrameInfoPtr>& packages,
                      const LabelStrings& labels = LabelStrings()) {
    return -1;
  }

  /**
   * @brief Execute. You can parse network output data and fill attributes to object.
   *
   * Usually, this function is for secondary network.
   *
   * @param[in] net_outputs The model output data.
   * @param[in] model_info The model information, e.g., input/output number, shape and etc.
   * @param[inout] packages The smart pointer of struct to store processed result.
   * @param[inout] objects The smart pointer of struct to store infer object.
   * @param[in] labels The label vector for object attributes.
   *
   * @return Returns 0 for success, otherwise returns <0.
   */
  virtual int Execute(const NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                      const std::vector<CNFrameInfoPtr>& packages, const std::vector<CNInferObjectPtr>& objects,
                      const LabelStrings& labels = LabelStrings()) {
    return -1;
  }

 public:
  float threshold_ = 0;  /*!< The threshold. Objects with confidence value less than threshold will be ignored.  */
};  // class Postproc

}  // namespace cnstream

#endif  // CNSTREAM_POSTPROC_HPP_
