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
#include <map>
#include <utility>
#include <vector>

#include "pypreproc.h"

namespace py = pybind11;

namespace cnstream {

PyPreproc::~PyPreproc() {
  py::gil_scoped_acquire gil;
  pyexecute_.release();
  pyinit_.release();
  pyinstance_.release();
}

namespace detail {

class Pybind11Preproc {
 public:
  virtual bool Init(const std::map<std::string, std::string> &params) {
    return true;
  }
  virtual std::vector<std::vector<float>> Execute(
      const std::vector<std::vector<int>> &input_shapes,
      const cnstream::CNFrameInfoPtr &finfo) = 0;
};  // class Pybind11Preproc

class Pybind11PreprocV : public Pybind11Preproc {
 public:
  bool Init(const std::map<std::string, std::string> &params) override {
    PYBIND11_OVERRIDE(
        bool,
        Pybind11Preproc,
        Init,
        params);
  }
  std::vector<std::vector<float>> Execute(
      const std::vector<std::vector<int>> &input_shapes,
      const cnstream::CNFrameInfoPtr &finfo) override {
    PYBIND11_OVERRIDE_PURE(
        std::vector<std::vector<float>>,
        Pybind11Preproc,
        Execute,
        input_shapes,
        finfo);
  }
};  // class Pybind11PreprocV

}  // namespace detail

IMPLEMENT_REFLEX_OBJECT_EX(PyPreproc, Preproc);

extern
std::pair<std::string, std::string> SplitPyModuleAndClass(const std::string &fullname);

bool PyPreproc::Init(const std::map<std::string, std::string> &params) {
  auto pyclass_name_iter = params.find("pyclass_name");
  if (pyclass_name_iter == params.end()) {
    LOGE(PyPreproc) << "pyclass_name must be set.";
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
    LOGE(PyPreproc) << "pyclass_name : [" << pyclass_name_iter->second << "]. " << e.what();
    return false;
  }

  pyclass_name_ = pyclass_name_iter->second;
  return true;
}

int PyPreproc::Execute(const std::vector<float*> &net_inputs,
                       const std::shared_ptr<edk::ModelLoader> &model,
                       const cnstream::CNFrameInfoPtr &finfo) {
  std::vector<std::vector<int>> input_shapes;
  for (uint32_t i = 0; i < model->InputNum(); ++i)
    input_shapes.emplace_back(model->InputShape(i).Vectorize());
  std::vector<std::vector<float>> results;
  try {
    py::gil_scoped_acquire gil;
    results = py::cast<std::vector<std::vector<float>>>(pyexecute_(input_shapes, finfo));
  } catch (std::runtime_error e) {
    LOGF(PyPreproc) << "[" << pyclass_name_ << "] Call execute failed : " << e.what();
  }
  std::string wrong_result_log_prefix = "[" + pyclass_name_ + "] The preprocessing result does "
      "not meet the model input requirements! detail : ";
  if (results.size() != static_cast<size_t>(net_inputs.size())) {
    LOGE(PyPreproc) << wrong_result_log_prefix
                    << "model input number [" << net_inputs.size() << "], but got [" << results.size() << "].";
    return -1;
  }
  for (size_t i = 0; i < results.size(); ++i) {
    size_t data_count = static_cast<size_t>(model->InputShape(i).DataCount());
    if (results[i].size() != data_count) {
      LOGE(PyPreproc) << wrong_result_log_prefix << "the length of " << i << "th input is ["
                      << data_count << "], but got [" << results[i].size() << "].";
      return -1;
    }
    const float *src = results[i].data();
    float *dst = net_inputs[i];
    memcpy(dst, src, data_count * sizeof(float));
  }
  return 0;
}

void PreprocWrapper(const py::module &m) {
  py::class_<detail::Pybind11Preproc, detail::Pybind11PreprocV>(m, "Preproc")
      .def(py::init<>())
      .def("init", &detail::Pybind11Preproc::Init)
      .def("execute", &detail::Pybind11Preproc::Execute);
}

}  // namespace cnstream

