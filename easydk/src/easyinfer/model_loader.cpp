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

#include "easyinfer/model_loader.h"
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "cxxutil/logger.h"
#include "model_loader_internal.h"

#define ONLY_SUPPORT_FLOAT32(layout)                                 \
  do {                                                               \
    if (layout.dtype != DataType::FLOAT32) {                         \
      throw ModelLoaderError("Only support float32 for cpu layout"); \
    }                                                                \
  } while (0)

namespace edk {

cnrtDataType CastDataType(const DataType& type) {
  switch (type) {
    case DataType::UINT8:
      return CNRT_UINT8;
    case DataType::FLOAT32:
      return CNRT_FLOAT32;
    case DataType::FLOAT16:
      return CNRT_FLOAT16;
    case DataType::INT16:
      return CNRT_INT16;
    case DataType::INT32:
      return CNRT_INT32;
    default:
      throw ModelLoaderError("Unsupported data type");
  }
}

DataType CastDataType(const cnrtDataType& type) {
  switch (type) {
    case CNRT_UINT8:
      return DataType::UINT8;
    case CNRT_FLOAT32:
      return DataType::FLOAT32;
    case CNRT_FLOAT16:
      return DataType::FLOAT16;
    case CNRT_INT16:
      return DataType::INT16;
    case CNRT_INT32:
      return DataType::INT32;
    default:
      throw ModelLoaderError("Unsupported data type");
  }
}

cnrtDimOrder CastDimOrder(const DimOrder& order) {
  switch (order) {
    case DimOrder::NCHW:
      return CNRT_NCHW;
    case DimOrder::NHWC:
      return CNRT_NHWC;
    default:
      throw ModelLoaderError("Unsupported dimension order");
  }
}

DimOrder CastDimOrder(const cnrtDimOrder& order) {
  switch (order) {
    case CNRT_NCHW:
      return DimOrder::NCHW;
    case CNRT_NHWC:
      return DimOrder::NHWC;
    default:
      throw ModelLoaderError("Unsupported dimension order");
  }
}

static const char* DataTypeStr(DataType type) {
  switch (type) {
    case DataType::UINT8:
      return "DataType UINT8";
    case DataType::FLOAT32:
      return "DataType FLOAT32";
    case DataType::FLOAT16:
      return "DataType FLOAT16";
    case DataType::INT16:
      return "DataType INT16";
    case DataType::INT32:
      return "DataType INT32";
    default:
      throw ModelLoaderError("Unsupported data type");
  }
}

static const char* DimOrderStr(DimOrder order) {
  switch (order) {
    case DimOrder::NCHW:
      return "DimOrder NCHW";
    case DimOrder::NHWC:
      return "DimOrder NHWC";
    default:
      throw ModelLoaderError("Unsupported dimension order");
  }
}

class ModelLoaderPrivate {
 public:
  explicit ModelLoaderPrivate(ModelLoader* q) : q_ptr_(q) {}
  void LoadFunction(const char* function_name);

#ifdef CNSTK_MLU100
  cnrtDataDescArray_t i_desc_array_, o_desc_array_;
#elif CNSTK_MLU270
  std::vector<int64_t> i_data_sizes_, o_data_sizes_;
  std::vector<DataLayout> i_mlu_layouts_, o_mlu_layouts_;
#endif
  int o_num_;
  int i_num_;
  int model_parallelism_;
  std::vector<DataLayout> i_cpu_layouts_, o_cpu_layouts_;
  std::vector<Shape> input_shapes_ = {}, output_shapes_ = {};
  cnrtModel_t model_;
  cnrtFunction_t function_;
  ModelLoader* q_ptr_ = nullptr;
};  // class ModelLoaderPrivate

ModelLoader::ModelLoader(const std::string& model_path, const std::string& function_name)
    : ModelLoader(model_path.c_str(), function_name.c_str()) {}

ModelLoader::ModelLoader(const char* model_path, const char* function_name) : d_ptr_(new ModelLoaderPrivate(this)) {
  if (FILE* file = fopen(model_path, "r")) {
    fclose(file);
  } else {
    throw ModelLoaderError("Model file not exist. Please check model path");
  }

  // 1. get cnrtModel and cnrtFunction
  cnrtRet_t error_code = cnrtLoadModel(&d_ptr_->model_, model_path);
  if (CNRT_RET_SUCCESS != error_code) {
    throw ModelLoaderError("Load model failed, error code : " + std::to_string(error_code));
  }

  d_ptr_->LoadFunction(function_name);
}

ModelLoader::ModelLoader(void* mem_ptr, const char* function_name) : d_ptr_(new ModelLoaderPrivate(this)) {
  // 1. get cnrtModel and cnrtFunction
  cnrtRet_t error_code = cnrtLoadModelFromMem(&d_ptr_->model_, reinterpret_cast<char*>(mem_ptr));
  if (CNRT_RET_SUCCESS != error_code) {
    throw ModelLoaderError("Load model from memory failed, error code : " + std::to_string(error_code));
  }

  d_ptr_->LoadFunction(function_name);
}

void ModelLoaderPrivate::LoadFunction(const char* function_name) {
  cnrtRet_t error_code;

  error_code = cnrtCreateFunction(&function_);
  if (CNRT_RET_SUCCESS != error_code) {
    throw ModelLoaderError("Create function failed, error code : " + std::to_string(error_code));
  }
  error_code = cnrtExtractFunction(&function_, model_, function_name);
  if (error_code != CNRT_RET_SUCCESS) {
    throw ModelLoaderError("Extract function failed, error code : " + std::to_string(error_code));
  }
  error_code = cnrtQueryModelParallelism(model_, &model_parallelism_);
  if (error_code != CNRT_RET_SUCCESS) {
    throw ModelLoaderError("Query Model Parallelism failed, error code : " + std::to_string(error_code));
  }

  LOG(INFO, "Load offline model succeeded");

// 2. get IO messages

// 2.1 get io number
#ifdef CNSTK_MLU100
  error_code = cnrtGetInputDataDesc(&i_desc_array_, &i_num_, function_);
  if (error_code != CNRT_RET_SUCCESS) {
    throw ModelLoaderError("Get input data desc failed, error code : " + std::to_string(error_code));
  }
  error_code = cnrtGetOutputDataDesc(&o_desc_array_, &o_num_, function_);
  if (error_code != CNRT_RET_SUCCESS) {
    throw ModelLoaderError("Get output data desc failed, error code : " + std::to_string(error_code));
  }

  // 2.2 get io shapes
  input_shapes_.resize(i_num_);
  for (int i = 0; i < i_num_; ++i) {
    cnrtDataDesc_t desc = i_desc_array_[i];
    Shape& sp = input_shapes_[i];
    unsigned int n, c, h, w;
    error_code = cnrtGetDataShape(desc, &n, &c, &h, &w);
    if (CNRT_RET_SUCCESS != error_code) {
      throw ModelLoaderError("Get data shape failed, error code : " + std::to_string(error_code));
    }
    sp.n = n;
    sp.c = c;
    sp.h = h;
    sp.w = w;
  }

  output_shapes_.resize(o_num_);
  for (int i = 0; i < o_num_; ++i) {
    cnrtDataDesc_t desc = o_desc_array_[i];
    Shape& sp = output_shapes_[i];
    unsigned int n, c, h, w;
    error_code = cnrtGetDataShape(desc, &n, &c, &h, &w);
    if (CNRT_RET_SUCCESS != error_code) {
      throw ModelLoaderError("Get data shape failed, error code : " + std::to_string(error_code));
    }
    sp.n = n;
    sp.c = c;
    sp.h = h;
    sp.w = w;
  }
#elif CNSTK_MLU270
  // 2.1 get io number and data size
  int64_t* input_sizes = nullptr;
  int input_num = 0;
  error_code = cnrtGetInputDataSize(&input_sizes, &input_num, function_);
  if (CNRT_RET_SUCCESS != error_code) {
    throw ModelLoaderError("Get input data size failed, error code : " + std::to_string(error_code));
  }
  i_num_ = input_num;
  i_data_sizes_ = std::vector<int64_t>(input_sizes, input_sizes + input_num);

  int64_t* output_sizes = nullptr;
  int output_num = 0;
  error_code = cnrtGetOutputDataSize(&output_sizes, &output_num, function_);
  if (CNRT_RET_SUCCESS != error_code) {
    throw ModelLoaderError("Get output data size failed, error code : " + std::to_string(error_code));
  }
  o_num_ = output_num;
  o_data_sizes_ = std::vector<int64_t>(output_sizes, output_sizes + output_num);

  // 2.2 get io shapes
  int* input_dim_values = nullptr;
  int dim_num = 0;
  input_shapes_.clear();
  for (int i = 0; i < input_num; ++i) {
    error_code = cnrtGetInputDataShape(&input_dim_values, &dim_num, i, function_);
    if (CNRT_RET_SUCCESS != error_code) {
      throw ModelLoaderError("Get input data shape failed, error code : " + std::to_string(error_code));
    }
    if (dim_num != 4) {
      throw ModelLoaderError("Unable to process a model whose input is not 4-dimensional data.");
    }
    // nhwc shape
    Shape sp;
    sp.n = input_dim_values[0];
    sp.h = input_dim_values[1];
    sp.w = input_dim_values[2];
    sp.c = input_dim_values[3];
    input_shapes_.push_back(sp);
  }

  int* output_dim_values = nullptr;
  output_shapes_.clear();
  for (int i = 0; i < output_num; ++i) {
    error_code = cnrtGetOutputDataShape(&output_dim_values, &dim_num, i, function_);
    if (CNRT_RET_SUCCESS != error_code) {
      throw ModelLoaderError("Get output data shape failed, error code : " + std::to_string(error_code));
    }
    if (dim_num != 4) {
      throw ModelLoaderError("Unable to process a model whose output is not 4-dimensional data.");
    }
    // nhwc shape
    Shape sp;
    sp.n = output_dim_values[0];
    sp.h = output_dim_values[1];
    sp.w = output_dim_values[2];
    sp.c = output_dim_values[3];
    output_shapes_.push_back(sp);
  }

  // 2.3 get mlu io data type
  cnrtDataType_t* input_dtypes = nullptr;
  error_code = cnrtGetInputDataType(&input_dtypes, &input_num, function_);
  if (CNRT_RET_SUCCESS != error_code) {
    throw ModelLoaderError("Get input data type failed, error code : " + std::to_string(error_code));
  }
  if (static_cast<size_t>(input_num) != i_data_sizes_.size()) {
    throw ModelLoaderError("Internel error, maybe input number from cnrtGetInputDataType is wrong.");
  }
  i_mlu_layouts_.resize(i_num_);
  for (int i = 0; i < i_num_; ++i) {
    i_mlu_layouts_[i].dtype = CastDataType(input_dtypes[i]);
    i_mlu_layouts_[i].order = DimOrder::NHWC;  // mlu data order is always NHWC
  }

  cnrtDataType_t* output_dtypes = nullptr;
  error_code = cnrtGetOutputDataType(&output_dtypes, &output_num, function_);
  if (CNRT_RET_SUCCESS != error_code) {
    throw ModelLoaderError("Get output data type failed, error code : " + std::to_string(error_code));
  }
  if (static_cast<size_t>(output_num) != o_data_sizes_.size()) {
    throw ModelLoaderError("Internal error, maybe output number from cnrtGetOutputDataType is wrong.");
  }
  o_mlu_layouts_.resize(o_num_);
  for (int i = 0; i < o_num_; ++i) {
    o_mlu_layouts_[i].dtype = CastDataType(output_dtypes[i]);
    o_mlu_layouts_[i].order = DimOrder::NHWC;  // mlu data order is always NHWC
  }
#endif  // CNSTK_MLU

  // set default cpu layouts
  // this decided by network framework(eg. cambricon caffe)
  i_cpu_layouts_.resize(i_num_);
  for (DataLayout& layout : i_cpu_layouts_) {
    layout.dtype = DataType::FLOAT32;
    layout.order = DimOrder::NHWC;
  }
  o_cpu_layouts_.resize(o_num_);
  for (DataLayout& layout : o_cpu_layouts_) {
    layout.dtype = DataType::FLOAT32;
    layout.order = DimOrder::NHWC;
  }
  int rgb0_index = -1;
  q_ptr_->WithRGB0Output(&rgb0_index);
  if (-1 != rgb0_index) {
    // with rgb0 output
    if (!(rgb0_index > 0 && rgb0_index < o_num_)) {
      throw ModelLoaderError("Invalid RGB0 data index");
    }
    o_cpu_layouts_[rgb0_index].dtype = DataType::UINT8;
    o_cpu_layouts_[rgb0_index].order = DimOrder::NCHW;  // FIXME(liumingxuan): problems!!!
  }
}

#ifdef CNSTK_MLU100
cnrtDataDescArray_t ModelLoaderInternalInterface::InputDescArray() const { return model_->d_ptr_->i_desc_array_; }

cnrtDataDescArray_t ModelLoaderInternalInterface::OutputDescArray() const { return model_->d_ptr_->o_desc_array_; }
#elif CNSTK_MLU270
int64_t ModelLoaderInternalInterface::InputDataSize(int data_index) const {
  if (data_index < 0 || data_index >= static_cast<int>(model_->InputNum())) return 0;
  return model_->d_ptr_->i_data_sizes_[data_index];
}

int64_t ModelLoaderInternalInterface::OutputDataSize(int data_index) const {
  if (data_index < 0 || data_index >= static_cast<int>(model_->OutputNum())) return 0;
  return model_->d_ptr_->o_data_sizes_[data_index];
}

DataLayout ModelLoaderInternalInterface::GetMluInputLayout(int data_index) const {
  if (data_index < 0 || data_index >= static_cast<int>(model_->InputNum())) return {};
  return model_->d_ptr_->i_mlu_layouts_[data_index];
}

DataLayout ModelLoaderInternalInterface::GetMluOutputLayout(int data_index) const {
  if (data_index < 0 || data_index >= static_cast<int>(model_->OutputNum())) return {};
  return model_->d_ptr_->o_mlu_layouts_[data_index];
}
#endif

cnrtFunction_t ModelLoaderInternalInterface::Function() const { return model_->d_ptr_->function_; }

bool ModelLoader::WithRGB0Output(int* output_index) const {
  if (!WithYUVInput()) return false;

  const Shape& i_shape = d_ptr_->input_shapes_[0];

  for (size_t index = 0; index < d_ptr_->output_shapes_.size(); index++) {
    const Shape& o_shape = d_ptr_->output_shapes_[index];
    if (i_shape.h == o_shape.h * 3 / 2 && i_shape.w == o_shape.w && o_shape.c == 4) {
      if (output_index) {
        *output_index = index;
      }
      return true;
    }
  }

  return false;
}

bool ModelLoader::WithYUVInput() const {
  if (d_ptr_->input_shapes_.size() < 1) return false;

  if (d_ptr_->input_shapes_[0].c == 1) return true;

  return false;
}

void ModelLoader::InitLayout() {
#ifdef CNSTK_MLU100
  // 1. set input host data layout
  LOG(INFO, "Set input layout");
  for (int i = 0; i < d_ptr_->i_num_; ++i) {
    cnrtDataDesc_t desc = d_ptr_->i_desc_array_[i];
    const DataLayout& layout = d_ptr_->i_cpu_layouts_[i];
    cnrtSetHostDataLayout(desc, CastDataType(layout.dtype), CastDimOrder(layout.order));
    LOG(INFO, std::to_string(i) + ": " + DataTypeStr(layout.dtype) + "\t" + DimOrderStr(layout.order));
  }

  // 2. set output host data layout
  LOG(INFO, "Set output layout");
  for (int i = 0; i < d_ptr_->o_num_; ++i) {
    cnrtDataDesc_t desc = d_ptr_->o_desc_array_[i];
    const DataLayout& layout = d_ptr_->o_cpu_layouts_[i];
    cnrtSetHostDataLayout(desc, CastDataType(layout.dtype), CastDimOrder(layout.order));
    LOG(INFO, std::to_string(i) + ": " + DataTypeStr(layout.dtype) + "\t" + DimOrderStr(layout.order));
  }

  LOG(INFO, "Offline model init layout succeeded");
#endif
}

void ModelLoader::SetCpuInputLayout(DataLayout layout, int data_index) {
  if (data_index < 0 || data_index >= d_ptr_->i_num_) {
    throw ModelLoaderError("SetCpuInputLayout: Data index out of range");
  }
  ONLY_SUPPORT_FLOAT32(layout);

  d_ptr_->i_cpu_layouts_[data_index] = layout;

  LOG(INFO, "Set CPU input data layout");
  LOG(INFO, "%s\t%s", DataTypeStr(layout.dtype), DimOrderStr(layout.order));
#ifdef CNSTK_MLU100
  cnrtDataDesc_t desc = d_ptr_->i_desc_array_[data_index];
  cnrtSetHostDataLayout(desc, CastDataType(layout.dtype), CastDimOrder(layout.order));
#endif
}

void ModelLoader::SetCpuOutputLayout(DataLayout layout, int data_index) {
  if (data_index < 0 || data_index >= d_ptr_->o_num_) {
    throw ModelLoaderError("SetCpuOutputLayout: Data index out of range");
  }
  ONLY_SUPPORT_FLOAT32(layout);

  d_ptr_->o_cpu_layouts_[data_index] = layout;

  LOG(INFO, "Set CPU output data layout");
  LOG(INFO, "%s\t%s", DataTypeStr(layout.dtype), DimOrderStr(layout.order));
#ifdef CNSTK_MLU100
  cnrtDataDesc_t desc = d_ptr_->o_desc_array_[data_index];
  cnrtSetHostDataLayout(desc, CastDataType(layout.dtype), CastDimOrder(layout.order));
#endif
}

DataLayout ModelLoader::GetCpuInputLayout(int data_index) const {
  if (data_index < 0 || data_index >= static_cast<int>(InputNum())) return {};
  return d_ptr_->i_cpu_layouts_[data_index];
}

DataLayout ModelLoader::GetCpuOutputLayout(int data_index) const {
  if (data_index < 0 || data_index >= static_cast<int>(OutputNum())) return {};
  return d_ptr_->o_cpu_layouts_[data_index];
}

bool ModelLoader::AdjustStackMemory() {
  uint64_t stack_size;
  uint32_t current_device_size;

  cnrtRet_t error_code = cnrtQueryModelStackSize(d_ptr_->model_, &stack_size);
  if (error_code != CNRT_RET_SUCCESS) {
    throw ModelLoaderError(
        "Query model stack size failed."
        "error_code : " +
        std::to_string(error_code));
  }

  error_code = cnrtGetStackMem(&current_device_size);
  if (error_code != CNRT_RET_SUCCESS) {
    throw ModelLoaderError(
        "Get current device stack size failed."
        "error_code : " +
        std::to_string(error_code));
  }

  if (stack_size > current_device_size) {
    error_code = cnrtSetStackMem(stack_size + 50);
    if (error_code != CNRT_RET_SUCCESS) {
      throw ModelLoaderError(
          "Set stack size failed."
          "error_code : " +
          std::to_string(error_code));
    }
    LOG(INFO, "Adjust stack memory to %d MB", stack_size + 50);
    return true;
  }
  return false;
}

uint32_t ModelLoader::OutputNum() const { return d_ptr_->o_num_; }

uint32_t ModelLoader::InputNum() const { return d_ptr_->i_num_; }

const std::vector<Shape>& ModelLoader::InputShapes() const { return d_ptr_->input_shapes_; }

const std::vector<Shape>& ModelLoader::OutputShapes() const { return d_ptr_->output_shapes_; }

int ModelLoader::ModelParallelism() const { return d_ptr_->model_parallelism_; }

int64_t ModelLoader::GetInputDataBatchAlignSize(int data_index) const {
  if (data_index < 0 || data_index >= static_cast<int>(InputNum())) return 0;
  int64_t size = 0;
#ifdef CNSTK_MLU100
  auto desc = d_ptr_->i_desc_array_[data_index];
  cnrtGetMemcpyBatchAlignment(desc, reinterpret_cast<unsigned int*>(&size));
#elif CNSTK_MLU270
  ModelLoaderInternalInterface model_loader_internal(const_cast<ModelLoader*>(this));
  size = model_loader_internal.InputDataSize(data_index) / d_ptr_->input_shapes_[data_index].n;
#endif
  return size;
}

int64_t ModelLoader::GetOutputDataBatchAlignSize(int data_index) const {
  if (data_index < 0 || data_index >= static_cast<int>(OutputNum())) return 0;
  int64_t size = 0;
#ifdef CNSTK_MLU100
  auto desc = d_ptr_->o_desc_array_[data_index];
  cnrtGetMemcpyBatchAlignment(desc, reinterpret_cast<unsigned int*>(&size));
#elif CNSTK_MLU270
  ModelLoaderInternalInterface model_loader_internal(const_cast<ModelLoader*>(this));
  size = model_loader_internal.OutputDataSize(data_index) / d_ptr_->output_shapes_[data_index].n;
#endif
  return size;
}

void ModelLoader::ReleaseModel() {
  cnrtRet_t error_code = cnrtDestroyFunction(d_ptr_->function_);
  if (error_code != CNRT_RET_SUCCESS) {
    throw ModelLoaderError(
        "Destroy function failed:"
        " error code : " +
        std::to_string(error_code));
  }
  error_code = cnrtUnloadModel(d_ptr_->model_);
  if (error_code != CNRT_RET_SUCCESS) {
    throw ModelLoaderError(
        "Unload model failed:"
        " error code : " +
        std::to_string(error_code));
  }
}

ModelLoader::~ModelLoader() {
  ReleaseModel();
  delete d_ptr_;
  d_ptr_ = nullptr;
}

}  // namespace edk
