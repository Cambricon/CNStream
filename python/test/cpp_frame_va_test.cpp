#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"

namespace py = pybind11;

namespace cnstream {

void SetDataFrame(std::shared_ptr<CNFrameInfo> frame, std::shared_ptr<CNDataFrame> dataframe) {
  dataframe->data[0].reset(new CNSyncedMemory(dataframe->height * dataframe->stride[0]));
  dataframe->data[1].reset(new CNSyncedMemory(dataframe->height * dataframe->stride[1] / 2));
  frame->collection.Add(kCNDataFrameTag, dataframe);
}

void SetCNInferobjs(std::shared_ptr<CNFrameInfo> frame, std::shared_ptr<CNInferObjs> objs_holder) {
  frame->collection.Add(kCNInferObjsTag, objs_holder);
}

}  // namespace cnstream

void FrameVaTestWrapper(py::module& m) {  // NOLINT
  m.def("set_data_frame", &cnstream::SetDataFrame);
  m.def("set_infer_objs", &cnstream::SetCNInferobjs);
}

