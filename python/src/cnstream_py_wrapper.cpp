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

namespace py = pybind11;

namespace cnstream {

void CNFrameInfoWrapper(const py::module&);
void CNFrameVaWrapper(const py::module&);
void CNSyncMemWrapper(const py::module&);
void ModuleWrapper(py::module &);
void SourceModuleWrapper(const py::module &);
void ConfigWrapper(py::module &);
void PipelineWrapper(py::module &);
void DataHandlerWrapper(const py::module &);
void PerfPrintWrapper(py::module &);
void PreprocWrapper(const py::module &);
void PostprocWrapper(const py::module &m);
void VideoPreprocWrapper(const py::module &);
void VideoPostprocWrapper(const py::module &);
void ProfileWrapper(const py::module &);

PYBIND11_MODULE(cnstream, m) {
  m.doc() = "cnstream python api";
  CNFrameInfoWrapper(m);
  CNFrameVaWrapper(m);
  CNSyncMemWrapper(m);
  ModuleWrapper(m);
  SourceModuleWrapper(m);
  ConfigWrapper(m);
  PipelineWrapper(m);
  DataHandlerWrapper(m);
  PerfPrintWrapper(m);
  PreprocWrapper(m);
  PostprocWrapper(m);
  VideoPreprocWrapper(m);
  VideoPostprocWrapper(m);
  ProfileWrapper(m);
}

}  // namespace cnstream

