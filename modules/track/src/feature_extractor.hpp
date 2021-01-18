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

#ifndef FEATURE_EXTRACTOR_HPP_
#define FEATURE_EXTRACTOR_HPP_
#include <opencv2/core/core.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "easybang/resize_and_colorcvt.h"
#include "easyinfer/easy_infer.h"
#include "easyinfer/mlu_memory_op.h"
#include "easyinfer/model_loader.h"

namespace cnstream {

using CNInferObjectPtr = std::shared_ptr<CNInferObject>;

class FeatureExtractor {
 public:
  FeatureExtractor() {}
  explicit FeatureExtractor(const std::shared_ptr<edk::ModelLoader>& model_loader,
                            int device_id = 0);
  ~FeatureExtractor();

  bool Init(CNDataFormat src_fmt, edk::CoreVersion core_ver);
  /*******************************************************
   * @brief inference and extract features of objects
   * @param
   *   frame[in] full image
   *   objs_holder[in] detected objects
   * @return return a 512 dimension vector as feature of
   *         object.
   * *****************************************************/
  void ExtractFeature(const CNDataFramePtr& frame, const CNInferObjsPtr& objs_holder,
                      std::vector<std::vector<float>>* features);


 private:
  void ExtractFeatureOnMlu(const CNDataFramePtr& frame, const CNInferObjsPtr& objs_holder,
                           std::vector<std::vector<float>>* features);
  void ExtractFeatureOnCpu(const CNDataFramePtr& frame, const CNInferObjsPtr& objs_holder,
                           std::vector<std::vector<float>>* features);
  int RunBatch(const uint32_t& inputs_size, std::vector<std::vector<float>>* outputs);
  float CalcFeatureOfRow(const cv::Mat& image, int n);

  edk::EasyInfer infer_;
  std::unique_ptr<edk::MluResizeConvertOp> rc_;
  edk::MluMemoryOp mem_op_;
  std::shared_ptr<edk::ModelLoader> model_loader_ = nullptr;
  int batch_size_ = 1;
  int device_id_;
  void** output_cpu_ptr_;
  void** input_mlu_ptr_;
  void** output_mlu_ptr_;
  bool is_initialized_ = false;
};  // class FeatureExtractor

}  // namespace cnstream

#endif  // FEATURE_EXTRACTOR_HPP_
