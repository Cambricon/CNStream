#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <functional>
#include <utility>
#include <string>
#include <unordered_map>
#include <vector>

#include "pyvideopostproc.h"

namespace py = pybind11;

bool TestPyVideoPostproc(const std::unordered_map<std::string, std::string> &params) {
  int device_id = 0;
  auto engine = std::make_shared<infer_server::InferServer>(device_id);
  auto model_info = engine->LoadModel("data/test_model.cambricon", "subnet0");

  infer_server::ModelIO model_output;
  for (uint32_t i = 0; i < model_info->OutputNum(); ++i) {
    infer_server::Buffer buffer(model_info->OutputShape(i).DataCount()*sizeof(float));
    buffer.MutableData();
    model_output.buffers.emplace_back(std::move(buffer));
    model_output.shapes.emplace_back(model_info->OutputShape(i));
  }

  cnstream::PyVideoPostproc pyvideopostproc;
  if (!pyvideopostproc.Init(params)) {
    return false;
  }

  return pyvideopostproc.Execute(nullptr, model_output, model_info.get());;
}

void VideoPostprocTestWrapper(py::module &m) {  // NOLINT
  m.def("cpptest_pyvideopostproc", &TestPyVideoPostproc);
}
