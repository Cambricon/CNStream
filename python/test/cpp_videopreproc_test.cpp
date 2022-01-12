#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <functional>
#include <utility>
#include <string>
#include <unordered_map>
#include <vector>

#include "pyvideopreproc.h"

namespace py = pybind11;

bool TestPyVideoPreproc(const std::unordered_map<std::string, std::string> &params) {
  int device_id = 0;
  auto engine = std::make_shared<infer_server::InferServer>(device_id);
  auto model_info = engine->LoadModel("data/test_model.cambricon", "subnet0");
  infer_server::ModelIO model_input;
  for (uint32_t i = 0; i < model_info->InputNum(); ++i) {
    infer_server::Buffer buffer(model_info->InputShape(i).DataCount()*sizeof(float));
    buffer.MutableData();
    model_input.buffers.emplace_back(std::move(buffer));
    model_input.shapes.emplace_back(model_info->InputShape(i));
  }

  cnstream::PyVideoPreproc pyvideopreproc;
  if (!pyvideopreproc.Init(params)) {
    return false;
  }

  infer_server::InferData input_data;
  return pyvideopreproc.Execute(&model_input, input_data, model_info.get());
}

void VideoPreprocTestWrapper(py::module &m) {  // NOLINT
  m.def("cpptest_pyvideopreproc", &TestPyVideoPreproc);
}
