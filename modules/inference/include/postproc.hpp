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

#ifndef POSTPROC_H_
#define POSTPROC_H_

#include <string>
#include <utility>
#include <vector>

#include "cnbase/reflex_object.h"
#include "cninfer/model_loader.h"

#include "cnstream_frame.hpp"

namespace cnstream {
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
  static Postproc* Create(const std::string& proc_name);
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
  virtual int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<libstream::ModelLoader>& model,
                      const CNFrameInfoPtr& package) = 0;

 protected:
  float threshold_ = 0;
};  // class Postproc

/**
 * @brief Ssd of post process
 */
class PostprocSsd : public Postproc, virtual public libstream::ReflexObjectEx<Postproc> {
 public:
  /**
   * @brief Execute postproc on neural ssd network outputs
   *
   * @param net_outputs: neural network outputs
   * @param model: model information(you can get input shape and output shape from model)
   * @param package: smart pointer of struct to store processed result
   *
   * @return return 0 if succeed
   */
  int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<libstream::ModelLoader>& model,
              const CNFrameInfoPtr& package) override;

  DECLARE_REFLEX_OBJECT_EX(PostprocSsd, Postproc)
};  // class PostprocSsd

}  // namespace cnstream

#endif  // ifndef POSTPROC_H_
