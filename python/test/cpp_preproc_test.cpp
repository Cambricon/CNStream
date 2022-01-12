#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <functional>
#include <string>
#include <map>
#include <vector>

#include "pypreproc.h"

namespace py = pybind11;

bool TestPyPreproc(const std::map<std::string, std::string> &params) {
  auto model = std::make_shared<edk::ModelLoader>("data/test_model.cambricon", "subnet0");
  std::vector<float*> inputs;
  for (uint32_t i = 0; i < model->InputNum(); ++i) {
    inputs.push_back(new float[model->InputShape(i).DataCount()]);
  }
  auto free_inputs = [&inputs] () {
    for (auto ptr : inputs) delete[] ptr;
  };
  cnstream::PyPreproc pypreproc;
  if (!pypreproc.Init(params)) {
    free_inputs();
    return false;
  }
  bool ret = 0 == pypreproc.Execute(inputs, model, nullptr);
  free_inputs();
  return ret;
}

void PreprocTestWrapper(py::module &m) {  // NOLINT
  m.def("cpptest_pypreproc", &TestPyPreproc);
}
