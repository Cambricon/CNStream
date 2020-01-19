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

/**
 * @file model_loader_internal.h
 *
 * This file contains a declaration of the ModelLoaderInternalInterface class to hide cnrt interface.
 */

#ifndef EASYINFER_MODEL_LOADER_INTERNAL_H_
#define EASYINFER_MODEL_LOADER_INTERNAL_H_

#include <cnrt.h>

namespace edk {

class ModelLoader;

class ModelLoaderInternalInterface {
 public:
  explicit ModelLoaderInternalInterface(ModelLoader* model) : model_(model) {}

  int64_t InputDataSize(int data_index) const;
  int64_t OutputDataSize(int data_index) const;
  DataLayout GetMluInputLayout(int data_index) const;
  DataLayout GetMluOutputLayout(int data_index) const;

  /**
   * @brief Get inference function stored in model
   *
   * @return cnrtFunction_t
   */
  cnrtFunction_t Function() const;

 private:
  ModelLoader* model_ = nullptr;
};  // class ModelLoaderInternalInterface

}  // namespace edk

#endif  // EASYINFER_MODEL_LOADER_INTERNAL_H_
