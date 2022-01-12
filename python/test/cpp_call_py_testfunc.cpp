#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "pymodule.h"

namespace py = pybind11;

bool TestPyModule(const cnstream::ModuleParamSet &params) {
  cnstream::PyModule pymodule("test_module");
  bool ret = pymodule.Open(params);
  if (!ret) return false;
  pymodule.Process(cnstream::CNFrameInfo::Create("test_stream"));
  // eos
  pymodule.Process(cnstream::CNFrameInfo::Create("test_stream", true));
  pymodule.Close();
  return true;
}

void FrameVaTestWrapper(py::module&);
void FrameTestWrapper(const py::module&);
void DataHanlderWrapper(const py::module&);
void PreprocTestWrapper(py::module&);
void PostprocTestWrapper(py::module&);
void VideoPreprocTestWrapper(py::module&);
void VideoPostprocTestWrapper(py::module&);

PYBIND11_MODULE(cnstream_cpptest, m) {
  m.def("cpptest_pymodule", &TestPyModule);
  FrameTestWrapper(m);
  FrameVaTestWrapper(m);
  DataHanlderWrapper(m);
  PreprocTestWrapper(m);
  PostprocTestWrapper(m);
  VideoPreprocTestWrapper(m);
  VideoPostprocTestWrapper(m);
}
