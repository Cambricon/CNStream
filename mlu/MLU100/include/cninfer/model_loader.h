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
#ifndef LIBSTREAM_INCLUDE_CNINFER_MODEL_LOADER_H_
#define LIBSTREAM_INCLUDE_CNINFER_MODEL_LOADER_H_

#include <cnrt.h>
#include <string>
#include <vector>
#include "cnbase/cnshape.h"
#include "cnbase/cntypes.h"
#include "cnbase/streamlibs_error.h"

namespace libstream {

STREAMLIBS_REGISTER_EXCEPTION(ModelLoader);

class ModelLoader {
 public:
  ModelLoader(const std::string& model_path,
              const std::string& function_name);
  ModelLoader(const char* model_path,
              const char* function_name);
  ModelLoader(void* mem_ptr, const char* function_name);
  ~ModelLoader();
  bool WithRGB0Output(int* output_index = nullptr) const;
  bool WithYUVInput() const;
  void InitLayout();
  bool AdjustStackMemory();
  inline uint32_t output_num() const;
  inline uint32_t input_num() const;
  inline cnrtDataDescArray_t input_desc_array() const;
  inline cnrtDataDescArray_t output_desc_array() const;
  inline const std::vector<CnShape>& input_shapes() const;
  inline const std::vector<CnShape>& output_shapes() const;
  inline cnrtFunction_t function() const;

 private:
  int o_num_;
  int i_num_;
  cnrtDataDescArray_t i_desc_array_, o_desc_array_;
  cnrtModel_t model_;
  cnrtFunction_t function_;
  std::vector<CnShape> vec_i_shape_, vec_o_shape_;
  void release_model();

  ModelLoader(const ModelLoader&) = delete;
  ModelLoader& operator=(const ModelLoader&) = delete;
};  // class ModelLoader

inline uint32_t ModelLoader::output_num() const {
  return o_num_;
}

inline uint32_t ModelLoader::input_num() const {
  return i_num_;
}

inline cnrtDataDescArray_t ModelLoader::input_desc_array() const {
  return i_desc_array_;
}

inline cnrtDataDescArray_t ModelLoader::output_desc_array() const {
  return o_desc_array_;
}

inline const std::vector<CnShape>& ModelLoader::input_shapes() const {
  return vec_i_shape_;
}

inline const std::vector<CnShape>& ModelLoader::output_shapes() const {
  return vec_o_shape_;
}

inline cnrtFunction_t ModelLoader::function() const {
  return function_;
}

}  // namespace libstream
#endif  // LIBSTREAM_INCLUDE_CNINFER_MODEL_LOADER_H_
