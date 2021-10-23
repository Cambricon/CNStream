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

#include "cnstream_frame.hpp"
#include "cnstream_source.hpp"

namespace py = pybind11;

namespace cnstream {

std::shared_ptr<py::class_<CNFrameInfo, std::shared_ptr<CNFrameInfo>>> gPyframeRegister;

void CNFrameInfoWrapper(const py::module &m) {
  gPyframeRegister = std::make_shared<py::class_<CNFrameInfo, std::shared_ptr<CNFrameInfo>>>(m, "CNFrameInfo");

  (*gPyframeRegister)
      .def(py::init([](std::string stream_id, bool eos = false) {
        auto frame_info = CNFrameInfo::Create(stream_id, eos);
        return frame_info;
      }), py::arg().noconvert(), py::arg("eos") = false)
      .def("is_eos", &CNFrameInfo::IsEos)
      .def("is_removed", &CNFrameInfo::IsRemoved)
      .def("is_invalid", &CNFrameInfo::IsInvalid)
      .def("get_py_collection", [] (std::shared_ptr<CNFrameInfo> frame) -> py::dict {
          frame->collection.AddIfNotExists("py_collection", std::shared_ptr<py::dict>(new py::dict(), [] (py::dict* t) {
            // py::dict destruct in c++ thread without gil resource
            // this is important to get gil when delete a py::dict.
            py::gil_scoped_acquire gil;
            delete t;
          }));
          auto collection = *(frame->collection.Get<std::shared_ptr<py::dict>>("py_collection"));
          return collection;
      })
      .def_readwrite("stream_id", &CNFrameInfo::stream_id)
      .def_readwrite("timestamp", &CNFrameInfo::timestamp);
}

}  //  namespace cnstream
