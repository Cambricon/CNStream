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

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cnis/contrib/video_helper.h"
#include "common_wrapper.hpp"
#include "pyvideopreproc.h"

namespace py = pybind11;

namespace cnstream {

PyVideoPreproc::~PyVideoPreproc() {
  py::gil_scoped_acquire gil;
  pyexecute_.release();
  pyinit_.release();
  pyinstance_.release();
}

namespace detail {

class Pybind11VideoPreproc {
 public:
  virtual bool Init(const std::unordered_map<std::string, std::string> &params) {
    return true;
  }
  virtual std::vector<std::vector<float>> Execute(
      const infer_server::InferData& input_data,
      const infer_server::ModelInfo* model_info) = 0;
};  // class Pybind11VideoPreproc

class Pybind11VideoPreprocV : public Pybind11VideoPreproc {
 public:
  bool Init(const std::unordered_map<std::string, std::string> &params) override {
    PYBIND11_OVERRIDE(
        bool,
        Pybind11VideoPreproc,
        Init,
        params);
  }
  std::vector<std::vector<float>> Execute(
      const infer_server::InferData& input_data,
      const infer_server::ModelInfo* model_info) override {
    PYBIND11_OVERRIDE_PURE(
        std::vector<std::vector<float>>,
        Pybind11VideoPreproc,
        Execute,
        input_data,
        model_info);
  }
};  // class Pybind11VideoPreprocV

}  // namespace detail

IMPLEMENT_REFLEX_OBJECT_EX(PyVideoPreproc, VideoPreproc);

extern
std::pair<std::string, std::string> SplitPyModuleAndClass(const std::string &fullname);

bool PyVideoPreproc::Init(const std::unordered_map<std::string, std::string> &params) {
  auto pyclass_name_iter = params.find("pyclass_name");
  if (pyclass_name_iter == params.end()) {
    LOGE(PyVideoPreproc) << "pyclass_name must be set.";
    return false;
  }

  try {
    auto t = SplitPyModuleAndClass(pyclass_name_iter->second);
    std::string pymodule_name = std::move(t.first);
    std::string pyclass_name = std::move(t.second);
    py::module pymodule = py::module::import(pymodule_name.c_str());
    pyinstance_ = pymodule.attr(pyclass_name.c_str())();
    pyinit_ = pyinstance_.attr("init");
    pyexecute_ = pyinstance_.attr("execute");
    auto tparams = params;
    tparams.erase("pyclass_name");
    return py::cast<bool>(pyinit_(tparams));
  } catch (std::runtime_error e) {
    LOGE(PyVideoPreproc) << "pyclass_name : [" << pyclass_name_iter->second << "]. " << e.what();
    return false;
  }

  pyclass_name_ = pyclass_name_iter->second;
  return true;
}

bool PyVideoPreproc::Execute(infer_server::ModelIO *model_input,
                            const infer_server::InferData &input_data,
                            const infer_server::ModelInfo *model_info) {
  std::vector<std::vector<float>> results;
  try {
    py::gil_scoped_acquire gil;
    results = py::cast<std::vector<std::vector<float>>>(pyexecute_(input_data, model_info));
  } catch (std::runtime_error e) {
    LOGF(PyVideoPreproc) << "[" << pyclass_name_ << "] Call execute failed : " << e.what();
  }
  std::string wrong_result_log_prefix = "[" + pyclass_name_ + "] The preprocessing result does "
      "not meet the model input requirements! detail : ";
  if (results.size() != static_cast<size_t>(model_input->buffers.size())) {
    LOGE(PyVideoPreproc) << wrong_result_log_prefix
        << "model input number [" << model_input->buffers.size() << "], but got [" << results.size() << "].";
    return false;
  }
  for (size_t i = 0; i < results.size(); ++i) {
    size_t data_count = static_cast<size_t>(model_info->InputShape(i).DataCount());
    if (results[i].size() != data_count) {
      LOGE(PyVideoPreproc) << wrong_result_log_prefix << "the length of " << i << "th input is ["
                      << data_count << "], but got [" << results[i].size() << "].";
      return false;
    }
    const float *src = results[i].data();
    float *dst = static_cast<float*>(model_input->buffers[i].MutableData());
    memcpy(dst, src, data_count * sizeof(float));
  }
  return true;
}

void VideoPreprocWrapper(const py::module &m) {
  py::class_<detail::Pybind11VideoPreproc, detail::Pybind11VideoPreprocV>(m, "VideoPreproc")
      .def(py::init<>())
      .def("init", &detail::Pybind11VideoPreproc::Init)
      .def("execute", &detail::Pybind11VideoPreproc::Execute);
}

}  // namespace cnstream

