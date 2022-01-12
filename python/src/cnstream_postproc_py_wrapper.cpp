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
#include <pybind11/numpy.h>

#include <memory>
#include <string>
#include <map>
#include <utility>
#include <vector>

#include "pypostproc.h"

namespace py = pybind11;

namespace cnstream {

PostprocPyObjects::~PostprocPyObjects() {
  py::gil_scoped_acquire gil;
  pyexecute.release();
  pyinit.release();
  pyinstance.release();
}

namespace detail {

class Pybind11Postproc {
 public:
  virtual bool Init(const std::map<std::string, std::string> &params) {
    return true;
  }
  virtual void Execute(
      const std::vector<py::array> &net_outputs,
      const std::vector<std::vector<int>> &input_shapes,
      const cnstream::CNFrameInfoPtr &finfo) = 0;
};  // class Pybind11Postproc

class Pybind11ObjPostproc {
 public:
  virtual bool Init(const std::map<std::string, std::string> &params) {
    return true;
  }
  virtual void Execute(
      const std::vector<py::array> &net_outputs,
      const std::vector<std::vector<int>> &input_shapes,
      const cnstream::CNFrameInfoPtr &finfo,
      const std::shared_ptr<CNInferObject> &obj) = 0;
};  // class Pybind11ObjPostproc

class Pybind11PostprocV : public Pybind11Postproc {
 public:
  bool Init(const std::map<std::string, std::string> &params) override {
    PYBIND11_OVERRIDE(
        bool,
        Pybind11Postproc,
        Init,
        params);
  }
  void Execute(
      const std::vector<py::array> &net_outputs,
      const std::vector<std::vector<int>> &input_shapes,
      const cnstream::CNFrameInfoPtr &finfo) override {
    PYBIND11_OVERRIDE_PURE(
        void,
        Pybind11Postproc,
        Execute,
        net_outputs,
        input_shapes,
        finfo);
  }
};  // class Pybind11PostprocV

class Pybind11ObjPostprocV : public Pybind11ObjPostproc {
 public:
  bool Init(const std::map<std::string, std::string> &params) override {
    PYBIND11_OVERRIDE(
        bool,
        Pybind11ObjPostproc,
        Init,
        params);
  }
  void Execute(
      const std::vector<py::array> &net_outputs,
      const std::vector<std::vector<int>> &input_shapes,
      const cnstream::CNFrameInfoPtr &finfo,
      const std::shared_ptr<CNInferObject> &obj) override {
    PYBIND11_OVERRIDE_PURE(
        void,
        Pybind11ObjPostproc,
        Execute,
        net_outputs,
        input_shapes,
        finfo,
        obj);
  }
};  // class Pybind11ObjPostprocV

}  // namespace detail

IMPLEMENT_REFLEX_OBJECT_EX(PyPostproc, Postproc);
IMPLEMENT_REFLEX_OBJECT_EX(PyObjPostproc, ObjPostproc);

extern
std::pair<std::string, std::string> SplitPyModuleAndClass(const std::string &fullname);

bool PostprocPyObjects::Init(const std::map<std::string, std::string> &params) {
  auto pyclass_name_iter = params.find("pyclass_name");
  if (pyclass_name_iter == params.end()) {
    LOGE(PostprocPyObjects) << "pyclass_name must be set.";
    return false;
  }
  this->pyclass_name = pyclass_name_iter->second;
  try {
    py::gil_scoped_acquire gil;
    auto t = SplitPyModuleAndClass(pyclass_name_iter->second);
    std::string pymodule_name = std::move(t.first);
    std::string pyclass_name = std::move(t.second);
    py::module pymodule = py::module::import(pymodule_name.c_str());
    pyinstance = pymodule.attr(pyclass_name.c_str())();
    pyinit = pyinstance.attr("init");
    pyexecute = pyinstance.attr("execute");
    auto tparams = params;
    tparams.erase("pyclass_name");
    return py::cast<bool>(pyinit(tparams));
  } catch (std::runtime_error e) {
    LOGE(PostprocPyObjects) << "pyclass_name : [" << pyclass_name_iter->second << "]. " << e.what();
    return false;
  }
  return true;
}

static
std::pair<std::vector<std::vector<int>>, std::vector<std::vector<int>>>
GetModelIOShapes(const std::shared_ptr<edk::ModelLoader> &model) {
  std::vector<std::vector<int>> input_shapes;
  for (uint32_t i = 0; i < model->InputNum(); ++i)
    input_shapes.emplace_back(model->InputShape(i).Vectorize());
  std::vector<std::vector<int>> output_shapes;
  for (uint32_t i = 0; i < model->OutputNum(); ++i)
    output_shapes.emplace_back(model->OutputShape(i).Vectorize());
  return std::make_pair(std::move(input_shapes), std::move(output_shapes));
}

static
std::vector<py::array> ToArray(const std::vector<float*> &vec, const std::vector<std::vector<int>> &shapes) {
  std::vector<py::array> ret;
  for (size_t i = 0; i < vec.size(); ++i) {
    auto sp_ignore_n = shapes[i];
    sp_ignore_n.erase(sp_ignore_n.begin());
    ret.emplace_back(py::array(sp_ignore_n, vec[i]));
  }
  return ret;
}

int PyPostproc::Execute(const std::vector<float*> &net_outputs,
                        const std::shared_ptr<edk::ModelLoader> &model,
                        const cnstream::CNFrameInfoPtr &finfo) {
  auto io_shapes = GetModelIOShapes(model);
  try {
    py::gil_scoped_acquire gil;
    pyobjs_.pyexecute(ToArray(net_outputs, io_shapes.second), io_shapes.first, finfo);
  } catch (std::runtime_error e) {
    LOGF(PyPostproc) << "[" << pyobjs_.pyclass_name << "] Call execute failed : " << e.what();
  }
  return 0;
}

int PyObjPostproc::Execute(const std::vector<float*> &net_outputs,
                           const std::shared_ptr<edk::ModelLoader> &model,
                           const cnstream::CNFrameInfoPtr &finfo,
                           const std::shared_ptr<CNInferObject> &obj) {
  auto io_shapes = GetModelIOShapes(model);
  try {
    py::gil_scoped_acquire gil;
    pyobjs_.pyexecute(ToArray(net_outputs, io_shapes.second), io_shapes.first, finfo, obj);
  } catch (std::runtime_error e) {
    LOGF(PyObjPostproc) << "[" << pyobjs_.pyclass_name << "] Call execute failed : " << e.what();
  }
  return 0;
}

int PyPostproc::Execute(const std::vector<void*> &net_outputs,
                        const std::shared_ptr<edk::ModelLoader> &model,
                        const std::vector<CNFrameInfoPtr> &finfos) {
  LOGF(PyPostproc) << "The parameter named [mem_on_mlu_for_postproc] of the Inferencer module "
                      "is not supported to be set to true when using python to implement the "
                      "preprocessing of the Inferencer module!";
  return 0;
}

int PyObjPostproc::Execute(const std::vector<void*> &net_outputs,
                        const std::shared_ptr<edk::ModelLoader> &model,
                        const std::vector<std::pair<CNFrameInfoPtr, std::shared_ptr<CNInferObject>>> &obj_infos) {
  LOGF(PyObjPostproc) << "The parameter named [mem_on_mlu_for_postproc] of the Inferencer module "
                      "is not supported to be set to true when using python to implement the "
                      "preprocessing of the Inferencer module!";
  return 0;
}

void PostprocWrapper(const py::module &m) {
  py::class_<detail::Pybind11Postproc, detail::Pybind11PostprocV>(m, "Postproc")
      .def(py::init<>())
      .def("init", &detail::Pybind11Postproc::Init)
      .def("execute", &detail::Pybind11Postproc::Execute);
  py::class_<detail::Pybind11ObjPostproc, detail::Pybind11ObjPostprocV>(m, "ObjPostproc")
      .def(py::init<>())
      .def("init", &detail::Pybind11ObjPostproc::Init)
      .def("execute", &detail::Pybind11ObjPostproc::Execute);
}

}  // namespace cnstream

