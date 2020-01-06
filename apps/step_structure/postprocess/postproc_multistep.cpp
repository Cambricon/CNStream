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
#include <iostream>
#include <utility>
#include <vector>
#include "postproc.hpp"

namespace iva {

class PostprocMultiStep : public cnstream::Postproc {
 public:
  DECLARE_REFLEX_OBJECT_EX(iva::PostprocMultiStep, cnstream::Postproc);
  int Execute(const std::vector<std::pair<float *, uint64_t>> &net_outputs, cnstream::StringPairs *result) override;
};  // class PostprocMultiStep

IMPLEMENT_REFLEX_OBJECT_EX(iva::PostprocMultiStep, cnstream::Postproc)

int PostprocMultiStep::Execute(const std::vector<std::pair<float *, uint64_t>> &net_outputs,
                               cnstream::StringPairs *result) {
  float max_score = 0;
  int max_index = 0;
  for (const auto &net_output : net_outputs) {
    float *net_result = net_output.first;
    uint64_t length = net_output.second;
    for (uint64_t i = 0; i < length; ++i) {
      if (net_result[i] > max_score) {
        max_score = *(net_result + i);
        max_index = i;
      }
    }
  }
  return max_index;
}

}  // namespace iva
