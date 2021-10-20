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

#include "cnis/infer_server.h"
#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "util/cnstream_queue.hpp"

namespace cnstream {

using CNInferObjectPtr = std::shared_ptr<CNInferObject>;

class FeatureExtractor {
 public:
  explicit FeatureExtractor(std::function<void(const CNFrameInfoPtr, bool)> callback) : callback_(callback) {}
  FeatureExtractor(const std::shared_ptr<infer_server::ModelInfo>& model,
                   std::function<void(const CNFrameInfoPtr, bool)> callback, int device_id = 0);
  ~FeatureExtractor();

  bool Init(int engine_num);
  /*******************************************************
   * @brief inference and extract features of objects
   * @param
   *   frame[in] full frame info
   * @return true if success, otherwise false
   * *****************************************************/
  bool ExtractFeature(const CNFrameInfoPtr& info);

  void WaitTaskDone(const std::string& stream_id);

 private:
  bool ExtractFeatureOnMlu(const CNFrameInfoPtr& info);
  bool ExtractFeatureOnCpu(const CNFrameInfoPtr& info);
  float CalcFeatureOfRow(const cv::Mat& image, int n);

  std::shared_ptr<infer_server::ModelInfo> model_{nullptr};
  std::unique_ptr<infer_server::InferServer> server_{nullptr};
  infer_server::Session_t session_{nullptr};
  std::function<void(const CNFrameInfoPtr, bool)> callback_{nullptr};
  int device_id_;
  bool is_initialized_ = false;
};  // class FeatureExtractor

}  // namespace cnstream

#endif  // FEATURE_EXTRACTOR_HPP_
