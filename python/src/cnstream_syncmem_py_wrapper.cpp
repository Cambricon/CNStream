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

#include "cnstream_syncmem.hpp"

namespace py = pybind11;

namespace cnstream {

void CNSyncMemWrapper(const py::module &m) {
  py::class_<CNSyncedMemory>(m, "CNSyncedMemory")
      .def(py::init<size_t>())
      .def(py::init<size_t, int, int>(),
          py::arg().noconvert(),
          py::arg().noconvert(),
          py::arg("mlu_ddr_chn") = -1)
      .def("get_cpu_data", &CNSyncedMemory::GetCpuData)
      .def("get_mlu_data", &CNSyncedMemory::GetMluData)
      .def("set_mlu_dev_context", &CNSyncedMemory::SetMluDevContext)
      .def("get_mlu_dev_id", &CNSyncedMemory::GetMluDevId)
      .def("get_size", &CNSyncedMemory::GetSize);
}

}  //  namespace cnstream
