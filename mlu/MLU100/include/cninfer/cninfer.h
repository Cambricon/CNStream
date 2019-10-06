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
#ifndef CNINFER_H_  // NOLINT
#define CNINFER_H_  // NOLINT

#include <memory>
#include "cnbase/streamlibs_error.h"
#include "cnrt.h"  // NOLINT
#include "cninfer/model_loader.h"

namespace libstream {

STREAMLIBS_REGISTER_EXCEPTION(CnInfer);

class CnInfer {
 public:
  CnInfer();
  ~CnInfer();
  void init(std::shared_ptr<ModelLoader> ploader, int batch_size);
  /******************************************************
   * @brief invoke function
   * @param
   *   input[in]: input data in mlu
   *   output[out]: output data in mlu
   ******************************************************/
  void run(void** input, void** output) const;
  std::shared_ptr<ModelLoader> loader() const;
  int batch_size() const;
  inline cnrtStream_t rt_stream() const {
    return stream_;
  }

 private:
  std::shared_ptr<ModelLoader> ploader_ = nullptr;
  cnrtFunction_t function_ = nullptr;
  cnrtStream_t stream_ = nullptr;
  void** param_ = nullptr;
  int batch_size_ = 0;

  CnInfer(const CnInfer&) = delete;
  CnInfer& operator=(const CnInfer&) = delete;
};  // class CnInfer

}  // namespace libstream

#endif  // _STREAM_RUNNER_H_  // NOLINT
