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

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

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

PYBIND11_MODULE(cnstream_cpptest, m) {
  m.def("cpptest_pymodule", &TestPyModule);
  FrameTestWrapper(m);
  FrameVaTestWrapper(m);
  DataHanlderWrapper(m);
  PreprocTestWrapper(m);
  PostprocTestWrapper(m);
}
