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

#ifndef FEATURE_EXTRACTOR_IMPL_H_
#define FEATURE_EXTRACTOR_IMPL_H_
#include <opencv2/core/core.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "easyinfer/easy_infer.h"
#include "easyinfer/mlu_memory_op.h"
#include "easyinfer/model_loader.h"
#include "easytrack/easy_track.h"

class FeatureExtractor {
 public:
  ~FeatureExtractor();
  bool Init(const std::string& model_path, const std::string& func_name, int dev_id = 0, uint32_t batch_size = 1);
  void Destroy();

  /*******************************************************
   * @brief inference and extract feature of an object
   * @param
   *   frame[in] full image
   *   obj[in] detected object
   * @return return a 128 dimension vector as feature of
   *         object.
   * *****************************************************/
  std::vector<float> ExtractFeature(const edk::TrackFrame& frame, const edk::DetectObject& obj);

 private:
  void Preprocess(const cv::Mat& img);

  edk::EasyInfer infer_;
  edk::MluMemoryOp mem_op_;
  std::shared_ptr<edk::ModelLoader> model_;
  std::mutex mlu_proc_mutex_;
  int device_id_;
  uint32_t batch_size_;
  void** input_cpu_ptr_;
  void** output_cpu_ptr_;
  void** input_mlu_ptr_;
  void** output_mlu_ptr_;
  bool extract_feature_mlu_ = false;
};  // class FeatureExtractorImpl

#endif  // FEATURE_EXTRACTOR_IMPL_H_
