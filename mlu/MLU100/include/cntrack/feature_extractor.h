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
#ifndef FEATURE_EXTRACTOR_H_
#define FEATURE_EXTRACTOR_H_

#include <opencv2/core/core.hpp>
#include <string>
#include <memory>
#include <utility>
#include <vector>

#include "cninfer/cninfer.h"
#include "cninfer/mlu_memory_op.h"
#include "cninfer/model_loader.h"

namespace libstream {

class FeatureExtractor {
 public:
  void Init(std::shared_ptr<ModelLoader> model);
  void Destroy();

  /*******************************************************
   * @brief inference and extract feature of an object
   * @param img[in] the image of detected object
   * @return return a 128 dimension vector as feature of
   *         object.
   * *****************************************************/
  std::vector<float> ExtractFeature(const cv::Mat& img);

 private:
  void Preprocess(const cv::Mat& img);

  CnInfer infer_;
  MluMemoryOp mem_op_;
  std::shared_ptr<ModelLoader> model_;
  void** input_cpu_ptr_;
  void** output_cpu_ptr_;
  void** input_mlu_ptr_;
  void** output_mlu_ptr_;
};  // class FeatureExtractor

}  // namespace libstream

#endif  // FEATURE_EXTRACTOR_H_

