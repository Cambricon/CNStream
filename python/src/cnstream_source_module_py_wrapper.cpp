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
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <string>

#include "cnstream_source.hpp"
#include "data_source.hpp"

namespace py = pybind11;

namespace cnstream {

namespace detail {

class PySourceHandler : public SourceHandler {
 public:
  using SourceHandler::SourceHandler;
  bool Open() override {
    PYBIND11_OVERRIDE_PURE(
      bool,
      SourceHandler,
      open);
  }
  void Close() override {
    PYBIND11_OVERRIDE_PURE(
      void,
      SourceHandler,
      close);
  }
};

class PySourceModule : public SourceModule {
 public:
  using SourceModule::SourceModule;
  bool Open(ModuleParamSet params) override {
    PYBIND11_OVERRIDE_PURE(
        bool,
        SourceModule,
        open,
        params);
  }
  void Close() override {
    PYBIND11_OVERRIDE_PURE(
        void,
        SourceModule,
        close);
  }
};  // class PySourceModule

}  // namespace detail

void SourceModuleWrapper(const py::module &m) {
  py::class_<SourceModule, std::shared_ptr<SourceModule>, detail::PySourceModule>(m, "SourceModule")
      .def(py::init<const std::string&>())
      .def("open", &SourceModule::Open)
      .def("close", &SourceModule::Close)
      .def("add_source", &SourceModule::AddSource)
      .def("get_source_handler", &SourceModule::GetSourceHandler)
      .def("remove_source",
           [](SourceModule* source, std::shared_ptr<SourceHandler> handler, bool force) {
             return source->RemoveSource(handler, force);
           },
           py::arg("handler"), py::arg("force") = false, py::call_guard<py::gil_scoped_release>())
      .def("remove_source",
           [](SourceModule* source, const std::string& stream_id, bool force) {
             return source->RemoveSource(stream_id, force);
           },
           py::arg("stream_id"), py::arg("force") = false, py::call_guard<py::gil_scoped_release>())
      .def("remove_sources", &SourceModule::RemoveSources, py::arg("force") = false,
          py::call_guard<py::gil_scoped_release>());

  py::class_<SourceHandler, std::shared_ptr<SourceHandler>, detail::PySourceHandler>(m, "SourceHandler")
      .def(py::init<SourceModule*, const std::string&>())
      .def("open", &SourceHandler::Open)
      .def("close", &SourceHandler::Close)
      .def("get_stream_id", &SourceHandler::GetStreamId)
      .def("create_frame_info", [](std::shared_ptr<SourceHandler> handler, bool eos) {
        return handler->CreateFrameInfo(eos);
        }, py::arg("eos") = false)
      .def("send_data", &SourceHandler::SendData);
}

}  // namespace cnstream
