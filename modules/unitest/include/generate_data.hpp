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

#ifndef MODULES_TEST_INCLUDE_GENERATE_DATA_HPP_
#define MODULES_TEST_INCLUDE_GENERATE_DATA_HPP_

#include <memory>
#include <string>

#include "cninfer/mlu_memory_op.h"
#include "connector.hpp"
#include "tensor.hpp"

namespace cnstream {

void GenerateData(unsigned char* data, DataType type);

class GenerateMluTestData {
 public:
  explicit GenerateMluTestData(TensorDesc desc);
  GenerateMluTestData(TensorDesc desc, const std::string& model_path, const std::string& fname);

  inline std::shared_ptr<Tensor> GetTensor() { return tensor_; }
  ~GenerateMluTestData();

 private:
  std::shared_ptr<Tensor> tensor_;
  libstream::MluMemoryOp mem_op_;
  void* mem_ptr_ = nullptr;
};  // class GenerateMluTestData

}  // namespace cnstream

#endif  // MODULES_TEST_INCLUDE_GENERATE_DATA_HPP_
