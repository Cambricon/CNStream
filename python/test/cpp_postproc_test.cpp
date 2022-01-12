#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <functional>
#include <string>
#include <map>
#include <vector>

#include "pypostproc.h"

namespace py = pybind11;

bool TestPyPostproc(const std::map<std::string, std::string> &params) {
  auto model = std::make_shared<edk::ModelLoader>("data/test_model.cambricon", "subnet0");
  std::vector<float*> outputs;
  for (uint32_t i = 0; i < model->OutputNum(); ++i) {
    outputs.push_back(new float[model->OutputShape(i).DataCount()]);
  }
  auto free_outputs = [&outputs] () {
    for (auto ptr : outputs) delete[] ptr;
  };
  cnstream::PyPostproc pypostproc;
  if (!pypostproc.Init(params)) {
    free_outputs();
    return false;
  }
  bool ret = 0 == pypostproc.Execute(outputs, model, nullptr);
  free_outputs();
  return ret;
}

bool TestPyObjPostproc(const std::map<std::string, std::string> &params) {
  auto model = std::make_shared<edk::ModelLoader>("data/test_model.cambricon", "subnet0");
  std::vector<float*> outputs;
  for (uint32_t i = 0; i < model->OutputNum(); ++i) {
    outputs.push_back(new float[model->OutputShape(i).DataCount()]);
  }
  auto free_outputs = [&outputs] () {
    for (auto ptr : outputs) delete[] ptr;
  };
  cnstream::PyObjPostproc pyobjpostproc;
  if (!pyobjpostproc.Init(params)) {
    free_outputs();
    return false;
  }
  bool ret = 0 == pyobjpostproc.Execute(outputs, model, nullptr, nullptr);
  free_outputs();
  return ret;
}

void PostprocTestWrapper(py::module &m) {  // NOLINT
  m.def("cpptest_pypostproc", &TestPyPostproc);
  m.def("cpptest_pyobjpostproc", &TestPyObjPostproc);
}
