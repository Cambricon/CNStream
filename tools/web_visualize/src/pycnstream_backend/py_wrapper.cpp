/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
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
#ifdef MAKE_PYTHONAPI

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "cnstype.h"
#include "pycnservice.hpp"

namespace py = pybind11;

PYBIND11_MODULE(pycnservice, m) {
  m.doc() = "cnstream service python wrapper";

  py::class_<CNSFrameInfo>(m, "CNSFrameInfo")
      .def(py::init())
      .def_readwrite("eos_flag", &CNSFrameInfo::eos_flag)
      .def_readwrite("frame_id", &CNSFrameInfo::frame_id)
      .def_readwrite("width", &CNSFrameInfo::width)
      .def_readwrite("height", &CNSFrameInfo::height);

  py::class_<CNServiceInfo>(m, "CNServiceInfo")
      .def(py::init())
      .def_readwrite("register_data", &CNServiceInfo::register_data)
      .def_readwrite("loop", &CNServiceInfo::loop)
      .def_readwrite("fps", &CNServiceInfo::fps)
      .def_readwrite("cache_size", &CNServiceInfo::cache_size)
      .def_readwrite("dst_width", &CNServiceInfo::dst_width)
      .def_readwrite("dst_height", &CNServiceInfo::dst_height);

  py::class_<PyCNService>(m, "PyCNService")
      .def(py::init())
      .def("init_service", &PyCNService::InitService, "init cnservice with parameters")
      .def("start", &PyCNService::Start, "start with config file and stream url")
      .def("stop", &PyCNService::Stop, "stop")
      .def("is_registered_data", &PyCNService::IsRegisteredData, "get whether register data callback")
      .def("is_running", &PyCNService::IsRunning, "is running or not")
      .def("read_one_frame", &PyCNService::ReadOneFrame, "get one frame");
}

#endif
