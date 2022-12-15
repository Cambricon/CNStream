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
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "pybind11/embed.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "cnstream_logging.hpp"
#include "common_wrapper.hpp"
#include "pypostproc.h"

namespace py = pybind11;

namespace cnstream {

PyPostproc::~PyPostproc() {
  py::gil_scoped_acquire gil;
  pyexecute_.release();
  pyinit_.release();
  pyinstance_.release();
}

namespace detail {

class Pybind11Postproc {
 public:
  virtual int Init(const std::unordered_map<std::string, std::string> &params) {
    return 0;
  }
  virtual int Execute(const NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                      const std::vector<CNFrameInfoPtr>& packages,
                      const LabelStrings& labels = LabelStrings()) = 0;
  virtual int ExecuteSecondary(const NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                               const std::vector<CNFrameInfoPtr>& packages,
                               const std::vector<CNInferObjectPtr>& objects,
                               const LabelStrings& labels = LabelStrings()) = 0;
};  // class Pybind11Postproc

class Pybind11PostprocV : public Pybind11Postproc {
 public:
  int Init(const std::unordered_map<std::string, std::string> &params) override {
    PYBIND11_OVERRIDE(
        int,
        Pybind11Postproc,
        Init,
        params);
  }
  int Execute(const NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
              const std::vector<CNFrameInfoPtr>& packages,
              const LabelStrings& labels = LabelStrings()) override {
    PYBIND11_OVERRIDE_PURE(
        int,
        Pybind11Postproc,
        Execute,
        net_outputs,
        model_info,
        packages,
        labels);
  }
  int ExecuteSecondary(const NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                       const std::vector<CNFrameInfoPtr>& packages,
                       const std::vector<CNInferObjectPtr>& objects,
                       const LabelStrings& labels = LabelStrings()) {
    PYBIND11_OVERRIDE_PURE(
        int,
        Pybind11Postproc,
        ExecuteSecondary,
        net_outputs,
        model_info,
        packages,
        objects,
        labels);
  }
};  // class Pybind11PostprocV

}  // namespace detail

IMPLEMENT_REFLEX_OBJECT_EX(PyPostproc, Postproc);

extern
std::pair<std::string, std::string> SplitPyModuleAndClass(const std::string &fullname);

int PyPostproc::Init(const std::unordered_map<std::string, std::string> &params) {
  auto pyclass_name_iter = params.find("pyclass_name");
  if (pyclass_name_iter == params.end()) {
    LOGE(PyPostproc) << "pyclass_name must be set.";
    return -1;
  }
  pyclass_name_ = pyclass_name_iter->second;
  try {
    auto t = SplitPyModuleAndClass(pyclass_name_iter->second);
    std::string pymodule_name = std::move(t.first);
    std::string pyclass_name = std::move(t.second);
    py::module pymodule = py::module::import(pymodule_name.c_str());
    pyinstance_ = pymodule.attr(pyclass_name.c_str())();
    pyinit_ = pyinstance_.attr("init");
    pyexecute_ = pyinstance_.attr("execute");
    pyexecute_secondary_ = pyinstance_.attr("execute_secondary");
    auto tparams = params;
    tparams.erase("pyclass_name");
    return py::cast<int>(pyinit_(tparams));
  } catch (std::runtime_error e) {
    LOGE(PyPostproc) << "pyclass_name : [" << pyclass_name_iter->second << "]. " << e.what();
    return -1;
  }
  return 0;
}

int PyPostproc::Execute(const NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                        const std::vector<CNFrameInfoPtr>& packages,
                        const LabelStrings& labels) {
  int ret = 0;
  try {
    py::gil_scoped_acquire gil;
    py::list net_outputs_list = py::cast(net_outputs, py::return_value_policy::reference);
    py::list packages_list = py::cast(packages, py::return_value_policy::reference);
    py::list labels_list = py::cast(labels, py::return_value_policy::reference);
    auto info = py::cast(model_info, py::return_value_policy::reference);
    ret = py::cast<int>(pyexecute_(net_outputs_list, info, packages_list, labels_list));
  } catch (std::runtime_error e) {
    LOGF(PyPostproc) << "[" << pyclass_name_ << "] Call execute failed : " << e.what();
  }

  return ret;
}

int PyPostproc::Execute(const NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                        const std::vector<CNFrameInfoPtr>& packages, const std::vector<CNInferObjectPtr>& objects,
                        const LabelStrings& labels) {
  int ret = 0;
  try {
    py::gil_scoped_acquire gil;
    py::list net_outputs_list = py::cast(net_outputs, py::return_value_policy::reference);
    py::list packages_list = py::cast(packages, py::return_value_policy::reference);
    py::list objects_list = py::cast(objects, py::return_value_policy::reference);
    py::list labels_list = py::cast(labels, py::return_value_policy::reference);
    auto info = py::cast(model_info, py::return_value_policy::reference);
    ret = py::cast<int>(pyexecute_secondary_(net_outputs_list, info, objects_list, objects_list, labels_list));
  } catch (std::runtime_error e) {
    LOGF(PyPostproc) << "[" << pyclass_name_ << "] Call execute failed : " << e.what();
  }

  return ret;
}

void PostprocWrapper(const py::module &m) {
  py::class_<detail::Pybind11Postproc, detail::Pybind11PostprocV>(m, "Postproc")
      .def(py::init<>())
      .def("init", &detail::Pybind11Postproc::Init)
      .def("execute", &detail::Pybind11Postproc::Execute)
      .def("execute_secondary", &detail::Pybind11Postproc::ExecuteSecondary);
}

}  // namespace cnstream
