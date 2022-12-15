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

#include "pypostproc.h"

namespace py = pybind11;

int TestPyPostproc(const std::unordered_map<std::string, std::string> &params) {
  int device_id = 0;
  auto engine = std::make_shared<infer_server::InferServer>(device_id);
  auto model_info = engine->LoadModel("../../data/models/yolov5m_v0.13.0_4b_rgb_uint8.magicmind");

  infer_server::ModelIO model_output;
  for (uint32_t i = 0; i < model_info->OutputNum(); ++i) {
    CnedkBufSurfaceCreateParams create_params;
    memset(&create_params, 0, sizeof(create_params));
    create_params.device_id = device_id;
    create_params.batch_size = 1;
    create_params.size = model_info->OutputShape(i).DataCount()*sizeof(uint8_t);
    create_params.color_format = CNEDK_BUF_COLOR_FORMAT_TENSOR;
    CnedkBufSurface* surf;
    CnedkBufSurfaceCreate(&surf, &create_params);
    auto wrapper = std::make_shared<cnedk::BufSurfaceWrapper>(surf);

    model_output.surfs.emplace_back(wrapper);
    model_output.shapes.emplace_back(model_info->OutputShape(i));
  }

  cnstream::NetOutputs net_outputs;
  for (size_t i = 0; i < model_output.surfs.size(); i++) {
    net_outputs.emplace_back(model_output.surfs[i], model_output.shapes[i]);
  }

  std::vector<cnstream::CNFrameInfoPtr> packages;
  packages.push_back(cnstream::CNFrameInfo::Create("stream_0"));

  cnstream::PyPostproc pypostproc;
  if (pypostproc.Init(params) != 0) {
    LOGE(PYTHON_API_TEST) << "TestPyPostproc(): Init pypostproc failed";
    return -1;
  }

  cnstream::LabelStrings label = {};
  if (pypostproc.Execute(net_outputs, *model_info, packages, label) != 0) {
    LOGE(PYTHON_API_TEST) << "TestPyPostproc(): pypostproc Execute failed";
    return -1;
  }
  return 0;
}

void PostprocTestWrapper(py::module &m) {  // NOLINT
  m.def("cpptest_pypostproc", &TestPyPostproc);
}
