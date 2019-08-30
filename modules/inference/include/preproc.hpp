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

#ifndef PREPROC_H_
#define PREPROC_H_

#include <string>
#include <utility>
#include <vector>

#include "cnbase/cnshape.h"
#include "cnbase/reflex_object.h"

#include "cnstream_frame.hpp"

namespace cnstream {

using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;

class Preproc {
 public:
  virtual ~Preproc() {}
  static Preproc* Create(const std::string& proc_name);

  /******************************************************************************
   * @brief Execute preproc on neural network inputs
   * @param
   *   package: smart pointer of struct stored network input image
   *   nn_inputs: neural network input data and shapes
   * @return return true if succeed, false otherwise
   ******************************************************************************/
  virtual bool Execute(CNFrameInfoPtr package, std::vector<std::pair<float*, libstream::CnShape>> nn_inputs) = 0;
};  // class Preproc

class PreprocCpu : public Preproc, virtual public libstream::ReflexObjectEx<Preproc> {
 public:
  /******************************************************************************
   * @attention nn_inputs is a pointer to pre-allocated cpu memory
   ******************************************************************************/
  bool Execute(CNFrameInfoPtr package, std::vector<std::pair<float*, libstream::CnShape>> nn_inputs) override;

  DECLARE_REFLEX_OBJECT_EX(PreprocCpu, Preproc);
};  // class PreprocCpu

}  // namespace cnstream

#endif  // ifndef PREPROC_H_
