/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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
#include <memory>
#include <functional>
#include <utility>
#include <string>
#include <unordered_map>
#include <vector>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "cnstream_logging.hpp"
#include "pypreproc.h"

namespace py = pybind11;

int TestPyPreproc(const std::unordered_map<std::string, std::string> &params) {
  int device_id = 0;
  auto engine = std::make_shared<infer_server::InferServer>(device_id);
  auto model_info = engine->LoadModel("../../data/models/yolov5m_v0.13.0_4b_rgb_uint8.magicmind");

  CnedkBufSurfaceCreateParams src_create_params;
  memset(&src_create_params, 0, sizeof(src_create_params));
  src_create_params.mem_type = CNEDK_BUF_MEM_DEVICE;
  src_create_params.device_id = device_id;
  src_create_params.batch_size = 1;
  src_create_params.width = 1920;
  src_create_params.height = 1080;
  src_create_params.color_format = CNEDK_BUF_COLOR_FORMAT_RGB;
  CnedkBufSurface* src_surf;
  CnedkBufSurfaceCreate(&src_surf, &src_create_params);
  auto src_wrapper = std::make_shared<cnedk::BufSurfaceWrapper>(src_surf);

  CnedkBufSurfaceCreateParams dst_create_params;
  memset(&dst_create_params, 0, sizeof(dst_create_params));
  dst_create_params.mem_type = CNEDK_BUF_MEM_DEVICE;
  dst_create_params.device_id = device_id;
  dst_create_params.batch_size = 1;
  dst_create_params.size = model_info->InputShape(0).DataCount()*sizeof(uint8_t);
  dst_create_params.width = 0;
  dst_create_params.height = 0;
  dst_create_params.color_format = CNEDK_BUF_COLOR_FORMAT_TENSOR;
  CnedkBufSurface* dst_surf;
  CnedkBufSurfaceCreate(&dst_surf, &dst_create_params);
  auto dst_wrapper = std::make_shared<cnedk::BufSurfaceWrapper>(dst_surf);

  cnstream::PyPreproc pypreproc;
  if (pypreproc.Init(params) != 0) {
    LOGE(PYTHON_API_TEST) << "TestPyPreproc(): Init pypreproc failed";
    return -1;
  }

  infer_server::CnPreprocTensorParams tensor_params;
  if (pypreproc.OnTensorParams(&tensor_params) != 0) {
    LOGE(PYTHON_API_TEST) << "TestPyPreproc(): pypreproc OnTensorParams failed";
    return -1;
  }

  if (pypreproc.Execute(src_wrapper, dst_wrapper, {}) != 0) {
    LOGE(PYTHON_API_TEST) << "TestPyPreproc(): pypreproc Execute failed";
    return -1;
  }

  return 0;
}

void PreprocTestWrapper(py::module &m) {  // NOLINT
  m.def("cpptest_pypreproc", &TestPyPreproc);
}
