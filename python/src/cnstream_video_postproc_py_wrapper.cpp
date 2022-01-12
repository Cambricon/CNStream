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
#include "pyvideopostproc.h"

namespace py = pybind11;

namespace cnstream {

PyVideoPostproc::~PyVideoPostproc() {
  py::gil_scoped_acquire gil;
  pyexecute_.release();
  pyinit_.release();
  pyinstance_.release();
}

namespace detail {

class Pybind11VideoPostproc {
 public:
  virtual bool Init(const std::unordered_map<std::string, std::string> &params) {
    return true;
  }
  virtual void Execute(
      infer_server::InferData *output_data,
      const std::vector<py::array> &net_outputs,
      infer_server::ModelInfo *model_info) = 0;
};  // class Pybind11VideoPostproc

class Pybind11VideoPostprocV : public Pybind11VideoPostproc {
 public:
  bool Init(const std::unordered_map<std::string, std::string> &params) override {
    PYBIND11_OVERRIDE(
        bool,
        Pybind11VideoPostproc,
        Init,
        params);
  }
  void Execute(
      infer_server::InferData *output_data,
      const std::vector<py::array> &net_outputs,
      infer_server::ModelInfo *model_info) override {
    PYBIND11_OVERRIDE_PURE(
        void,
        Pybind11VideoPostproc,
        Execute,
        output_data,
        net_outputs,
        model_info);
  }
};  // class Pybind11VideoPostprocV

}  // namespace detail

IMPLEMENT_REFLEX_OBJECT_EX(PyVideoPostproc, VideoPostproc);

extern
std::pair<std::string, std::string> SplitPyModuleAndClass(const std::string &fullname);

bool PyVideoPostproc::Init(const std::unordered_map<std::string, std::string> &params) {
  auto pyclass_name_iter = params.find("pyclass_name");
  if (pyclass_name_iter == params.end()) {
    LOGE(PyVideoPostproc) << "pyclass_name must be set.";
    return false;
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
    auto tparams = params;
    tparams.erase("pyclass_name");
    return py::cast<bool>(pyinit_(tparams));
  } catch (std::runtime_error e) {
    LOGE(PyVideoPostproc) << "pyclass_name : [" << pyclass_name_iter->second << "]. " << e.what();
    return false;
  }
  return true;
}

static
std::vector<py::array> ToArray(const std::vector<const float*> &vec, const std::vector<std::vector<int64_t>> &shapes) {
  std::vector<py::array> ret;
  for (size_t i = 0; i < vec.size(); ++i) {
    auto sp_ignore_n = shapes[i];
    sp_ignore_n.erase(sp_ignore_n.begin());
    ret.emplace_back(py::array(sp_ignore_n, vec[i]));
  }
  return ret;
}

bool PyVideoPostproc::Execute(infer_server::InferData *output_data,
                              const infer_server::ModelIO &model_output,
                              const infer_server::ModelInfo *model_info) {
  std::vector<const float*> net_outputs;
  std::vector<std::vector<int64_t>> output_shapes;
  for (size_t i = 0; i < model_output.buffers.size(); ++i) {
    net_outputs.emplace_back(static_cast<const float*>(model_output.buffers[i].Data()));
    output_shapes.emplace_back(model_output.shapes[i].Vectorize());
  }
  try {
    py::gil_scoped_acquire gil;
    pyexecute_(output_data, ToArray(net_outputs, output_shapes), model_info);
  } catch (std::runtime_error e) {
    LOGF(PyVideoPostproc) << "[" << pyclass_name_ << "] Call execute failed : " << e.what();
  }

  return true;
}

void VideoPostprocWrapper(const py::module &m) {
  py::class_<detail::Pybind11VideoPostproc, detail::Pybind11VideoPostprocV>(m, "VideoPostproc")
      .def(py::init<>())
      .def("init", &detail::Pybind11VideoPostproc::Init)
      .def("execute", &detail::Pybind11VideoPostproc::Execute);
}

}  // namespace cnstream
