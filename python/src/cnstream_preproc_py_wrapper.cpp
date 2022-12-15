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
#include "pypreproc.h"

namespace py = pybind11;

namespace cnstream {

PyPreproc::~PyPreproc() {
  py::gil_scoped_acquire gil;
  pyon_tensor_params_.release();
  pyexecute_.release();
  pyinit_.release();
  pyinstance_.release();
}

namespace detail {

class Pybind11Preproc {
 public:
  virtual int Init(const std::unordered_map<std::string, std::string> &params) {
    return 0;
  }
  virtual int OnTensorParams(const infer_server::CnPreprocTensorParams *params) = 0;
  virtual int Execute(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
                      const std::vector<CnedkTransformRect> &src_rects) = 0;
};  // class Pybind11Preproc

class Pybind11PreprocV : public Pybind11Preproc {
 public:
  int Init(const std::unordered_map<std::string, std::string> &params) override {
    PYBIND11_OVERRIDE(
        int,
        Pybind11Preproc,
        Init,
        params);
  }
  int OnTensorParams(const infer_server::CnPreprocTensorParams *params) override {
    PYBIND11_OVERRIDE_PURE(
        int,
        Pybind11Preproc,
        OnTensorParams,
        params);
  }
  int Execute(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
              const std::vector<CnedkTransformRect> &src_rects) override {
    PYBIND11_OVERRIDE_PURE(
        int,
        Pybind11Preproc,
        Execute,
        src,
        dst,
        src_rects);
  }
};  // class Pybind11PreprocV

}  // namespace detail

IMPLEMENT_REFLEX_OBJECT_EX(PyPreproc, Preproc);

extern
std::pair<std::string, std::string> SplitPyModuleAndClass(const std::string &fullname);

int PyPreproc::Init(const std::unordered_map<std::string, std::string> &params) {
  auto pyclass_name_iter = params.find("pyclass_name");
  if (pyclass_name_iter == params.end()) {
    LOGE(PyPreproc) << "pyclass_name must be set.";
    return -1;
  }

  try {
    auto t = SplitPyModuleAndClass(pyclass_name_iter->second);
    std::string pymodule_name = std::move(t.first);
    std::string pyclass_name = std::move(t.second);
    py::module pymodule = py::module::import(pymodule_name.c_str());
    pyinstance_ = pymodule.attr(pyclass_name.c_str())();
    pyinit_ = pyinstance_.attr("init");
    pyon_tensor_params_ = pyinstance_.attr("on_tensor_params");
    pyexecute_ = pyinstance_.attr("execute");
    auto tparams = params;
    tparams.erase("pyclass_name");
    return py::cast<int>(pyinit_(tparams));
  } catch (std::runtime_error e) {
    LOGE(PyPreproc) << "pyclass_name : [" << pyclass_name_iter->second << "]. " << e.what();
    return -1;
  }

  pyclass_name_ = pyclass_name_iter->second;
  return 0;
}

int PyPreproc::OnTensorParams(const infer_server::CnPreprocTensorParams *params) {
  int ret = 0;
  try {
    py::gil_scoped_acquire gil;
    ret = py::cast<int>(pyon_tensor_params_(params));
  } catch (std::runtime_error e) {
    LOGF(PyPreproc) << "[" << pyclass_name_ << "] Call on_tensor_params failed : " << e.what();
  }
  return ret;
}

int PyPreproc::Execute(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
                       const std::vector<CnedkTransformRect> &src_rects) {
  int ret = 0;
  try {
    py::gil_scoped_acquire gil;
    ret = py::cast<int>(pyexecute_(src, dst, src_rects));
  } catch (std::runtime_error e) {
    LOGF(PyPreproc) << "[" << pyclass_name_ << "] Call execute failed : " << e.what();
  }
  return ret;
}

void PreprocWrapper(const py::module &m) {
  py::class_<detail::Pybind11Preproc, detail::Pybind11PreprocV>(m, "Preproc")
      .def(py::init<>())
      .def("init", &detail::Pybind11Preproc::Init)
      .def("on_tensor_params", &detail::Pybind11Preproc::OnTensorParams)
      .def("execute", &detail::Pybind11Preproc::Execute);
}

}  // namespace cnstream

