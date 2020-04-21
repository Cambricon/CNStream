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
 * @file model_loader.h
 *
 * This file contains a declaration of the ModelLoader class and involved struct
 */

#ifndef EASYINFER_MODEL_LOADER_H_
#define EASYINFER_MODEL_LOADER_H_

#include <string>
#include <vector>
#include "cxxutil/exception.h"
#include "easyinfer/shape.h"

namespace edk {

TOOLKIT_REGISTER_EXCEPTION(ModelLoader);

/**
 * @brief Enumeration to specify data type of model input and output
 */
enum class DataType { UINT8, FLOAT32, FLOAT16, INT16, INT32 };

/**
 * @brief Enumeration to specify dim order of model input and output
 */
enum class DimOrder { NCHW, NHWC, HWCN, TNC, NTC };

/**
 * @brief Describe data layout on MLU or CPU
 */
struct DataLayout {
  DataType dtype;  ///< @see DataType
  DimOrder order;  ///< @see DimOrder
};

class ModelLoaderPrivate;
class ModelLoaderInternalInterface;

/**
 * @brief A helper class to load offline model and get model infomation
 */
class ModelLoader {
 public:
  friend class ModelLoaderInternalInterface;
  /**
   * @brief Constructor 1. Construct a new Model Loader object
   *
   * @note Delegate to constructor 2 for construct
   * @param model_path[in] Inference offline model path
   * @param function_name[in] Name of function in offline model
   */
  ModelLoader(const std::string& model_path, const std::string& function_name);

  /**
   * @brief Constructor 2. Construct a new Model Loader object
   *
   * @note Delegate to constructor 3 for construct
   * @param model_path[in] Model path
   * @param function_name[in] Function name
   */
  ModelLoader(const char* model_path, const char* function_name);

  /**
   * @brief Constructor 3. Construct a new Model Loader object
   *
   * @param mem_ptr[in] Offline model binary stored in memory
   * @param function_name[in] Function name
   */
  ModelLoader(void* mem_ptr, const char* function_name);

  /**
   * @brief Destroy the Model Loader object
   */
  ~ModelLoader();

  /**
   * @brief Query whether model outputs RGB0 image together with inference result
   *
   * @param output_index[out] RGB0 output index
   * @return Return true if have RGB0 output, and RGB0 index assigned to output_index
   */
  bool WithRGB0Output(int* output_index = nullptr) const;

  /**
   * @brief Query whether model input is YUV image
   *
   * @return Return true if model input is YUV image
   */
  bool WithYUVInput() const;

  /**
   * @brief Init model input data and output data layout on MLU100
   * @attention Implement as an empty function on this version
   * @deprecated
   */
  void InitLayout();

  /**
   * @brief Set specified input data layout on CPU
   *
   * @param layout[in] Data layout
   * @param data_index[in] Data index
   */
  void SetCpuInputLayout(DataLayout layout, int data_index);

  /**
   * @brief Set specified output data layout on CPU
   *
   * @param layout[in] Data layout
   * @param data_index[in] Data index
   */
  void SetCpuOutputLayout(DataLayout layout, int data_index);

  /**
   * @brief Get specified input data layout on CPU
   *
   * @param data_index[in] Data index
   * @return Data layout
   */
  DataLayout GetCpuInputLayout(int data_index) const;

  /**
   * @brief Get specified output data layout on CPU
   *
   * @param data_index[in] Data index
   * @return Data layout
   */
  DataLayout GetCpuOutputLayout(int data_index) const;

  /**
   * @brief Adjust MLU stack memory according to model size
   *
   * @note Adjust MLU stack memory. Do nothing if model size is not larger than current stack memory size.
   * @return Return true if stack memory is adjusted.
   */
  bool AdjustStackMemory();

  /**
   * @brief Get model output number
   *
   * @return Model output number
   */
  uint32_t OutputNum() const;

  /**
   * @brief Get model input number
   *
   * @return Model input number
   */
  uint32_t InputNum() const;

  /**
   * @brief Get model input data shapes
   *
   * @return Model input data shapes
   */
  const std::vector<Shape>& InputShapes() const;

  /**
   * @brief Get model output data shapes
   *
   * @return Model output data shapes
   */
  const std::vector<Shape>& OutputShapes() const;

  /**
   * @brief Get model parallelism
   *
   * @note Not supported on MLU100, always return 1.
   * @return Model parallelism
   */
  int ModelParallelism() const;

  /**
   * @brief Get the input data batch align size
   *
   * @param data_index[in] Data index
   * @return input data batch align size
   */
  int64_t GetInputDataBatchAlignSize(int data_index) const;

  /**
   * @brief Get the output data batch align size
   *
   * @param data_index[in] Data index
   * @return output data batch align size
   */
  int64_t GetOutputDataBatchAlignSize(int data_index) const;

 private:
  void ReleaseModel();

  ModelLoaderPrivate* d_ptr_ = nullptr;

  ModelLoader(const ModelLoader&) = delete;
  ModelLoader& operator=(const ModelLoader&) = delete;
  ModelLoader(ModelLoader&&) = delete;
  ModelLoader& operator=(ModelLoader&&) = delete;
};  // class ModelLoader

}  // namespace edk

#endif  // EASYINFER_MODEL_LOADER_H_
