#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>

#include "cnstream_frame.hpp"

namespace py = pybind11;

namespace cnstream {

class CppCNFrameInfoTestHelper {
 public:
  CppCNFrameInfoTestHelper() {
    frame_info_ = cnstream::CNFrameInfo::Create("test_stream_id_0");
  }
  std::shared_ptr<CNFrameInfo> GetFrameInfo() {
    return frame_info_;
  }
 private:
  std::shared_ptr<CNFrameInfo> frame_info_;
};  //  class CppCNFrameInfoTestHelper

}  // namespace cnstream

void FrameTestWrapper(const py::module& m) {
  py::class_<cnstream::CppCNFrameInfoTestHelper>(m, "CppCNFrameInfoTestHelper")
    .def(py::init())
    .def("get_frame_info", &cnstream::CppCNFrameInfoTestHelper::GetFrameInfo);
}

