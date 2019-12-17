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

#ifndef MODULES_CLASSIFIER_INCLUDE_MULTISTEP_IMPL_HPP_
#define MODULES_CLASSIFIER_INCLUDE_MULTISTEP_IMPL_HPP_
/**
 *  \file multistep_classifier_impl.hpp
 *
 *  This file contains a declaration of class MultiStepClassifierImpl
 */

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "easyinfer/easy_infer.h"
#include "easyinfer/mlu_context.h"
#include "easyinfer/mlu_memory_op.h"
#include "easyinfer/model_loader.h"
#ifdef HAVE_OPENCV
#include "opencv2/opencv.hpp"
#else
#error OpenCV required
#endif

namespace cnstream {

struct MultiStepClassifierImplData {
  cv::Mat image;
  int channel_idx;
  int id;
};

/**
 * @brief MultiStepClassifierImpl is implementation of multi-step classifier
 */
class MultiStepClassifierImpl {
 public:
  /**
   *  @brief  Generate MultiStepClassifierImpl
   *
   *  @param  Name : module name
   *
   *  @return None
   */
  explicit MultiStepClassifierImpl(int step1_classid, int bsize, int device_id,
                                   std::unordered_map<int, std::shared_ptr<edk::ModelLoader>> modelloader,
                                   std::unordered_map<int, std::vector<std::string>> labels);
  /**
   *  @brief  Release MultiStepClassifierImpl
   *
   *  @param  None
   *
   *  @return None
   */
  ~MultiStepClassifierImpl();

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
  bool Init();

  /**
   * @brief  Called by pipeline when pipeline stop
   *
   * @param  None
   *
   * @return  None
   */
  void Destory();

  /**
   * @brief Encode each frame
   *
   * @param data : data to be processed
   *
   * @return whether process succeed
   * @retval 0: succeed and do no intercept data
   * @retval <0: failed
   */
  std::vector<std::pair<float*, uint64_t>> Classify(const int& class_idx);

  std::unordered_map<int, void**> cpu_inputs_;

 private:
  int step1_class_index_ = 0;
  uint32_t batch_size_ = 1;
  std::unordered_map<int, std::shared_ptr<edk::ModelLoader>> model_loaders_;
  int device_id_ = 0;
  std::unordered_map<int, std::vector<std::string>> labels_;

  std::unordered_map<int, edk::MluContext*> envs_;
  std::unordered_map<int, edk::MluMemoryOp*> memops_;
  std::unordered_map<int, edk::EasyInfer*> infers_;
  std::unordered_map<int, void**> mlu_inputs_;
  std::unordered_map<int, void**> cpu_outputs_;
  std::unordered_map<int, void**> mlu_outputs_;
  volatile bool initialized = false;
};  // class MultiStepClassifierImpl

}  // namespace cnstream

#endif
