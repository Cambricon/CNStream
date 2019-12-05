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

#ifndef MODULES_INFERENCE_INCLUDE_PREPROC_HPP_
#define MODULES_INFERENCE_INCLUDE_PREPROC_HPP_

/**
 *  \file preproc.hpp
 *
 *  This file contains a declaration of class Preproc
 */

#include <opencv2/opencv.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

#include "reflex_object.h"
#include "easyinfer/model_loader.h"
#include "cnstream_frame.hpp"

namespace cnstream {

/**
 * @brief construct a pointer to CNFrameInfo
 */
using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;
/**
 * @brief Base class of pre process
 */
class Preproc {
 public:
  /**
   * @brief do nothong
   */
  virtual ~Preproc() {}
  /**
   * @brief create relative preprocess
   *
   * @param pre_name preprocess class name
   *
   * @return None
   */
  static Preproc* Create(const std::string& proc_name);

  /**
   * @brief Execute preproc on neural network outputs
   *
   * @param net_inputs: neural network inputs
   * @param model: model information(you can get input shape and output shape from model)
   * @param package: smart pointer of struct to store origin data
   *
   * @return return 0 if succeed
   */
  virtual int Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
                      const CNFrameInfoPtr& package) { return 0; }

  /**
   * @brief Execute preproc
   *
   * @param net_outputs: neural network outputs
   *
   * @return the preproc result
   */
  virtual cv::Mat Execute(const cv::Mat &image) { return image; }
  virtual std::shared_ptr<float> Execute(const cv::Mat &image,
                                         const std::shared_ptr<edk::ModelLoader>& model) { return nullptr; }


  virtual int Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
    const CNFrameInfoPtr& package, cnstream::CNInferBoundingBox bbox) { return 0; }
};  // class Preproc

}  // namespace cnstream

#endif  // ifndef MODULES_INFERENCE_INCLUDE_PREPROC_HPP_
