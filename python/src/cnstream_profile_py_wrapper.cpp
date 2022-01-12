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

#include <string>

#include "profiler/module_profiler.hpp"
#include "profiler/pipeline_profiler.hpp"

namespace py = pybind11;

namespace cnstream {

void ProfileWrapper(const py::module &m) {
  py::class_<PipelineProfile>(m, "PipelineProfile")
      .def(py::init())
      .def_readwrite("pipeline_name", &PipelineProfile::pipeline_name)
      .def_readwrite("module_profiles", &PipelineProfile::module_profiles)
      .def_readwrite("overall_profile", &PipelineProfile::overall_profile);
  py::class_<ModuleProfile>(m, "ModuleProfile")
      .def(py::init())
      .def_readwrite("module_name", &ModuleProfile::module_name)
      .def_readwrite("process_profiles", &ModuleProfile::process_profiles);
  py::class_<ProcessProfile>(m, "ProcessProfile")
      .def(py::init())
      .def_readwrite("process_name", &ProcessProfile::process_name)
      .def_readwrite("counter", &ProcessProfile::counter)
      .def_readwrite("completed", &ProcessProfile::completed)
      .def_readwrite("dropped", &ProcessProfile::dropped)
      .def_readwrite("ongoing", &ProcessProfile::ongoing)
      .def_readwrite("latency", &ProcessProfile::latency)
      .def_readwrite("maximum_latency", &ProcessProfile::maximum_latency)
      .def_readwrite("minimum_latency", &ProcessProfile::minimum_latency)
      .def_readwrite("fps", &ProcessProfile::fps)
      .def_readwrite("stream_profiles", &ProcessProfile::stream_profiles);
  py::class_<StreamProfile>(m, "StreamProfile")
      .def(py::init())
      .def_readwrite("stream_name", &StreamProfile::stream_name)
      .def_readwrite("counter", &StreamProfile::counter)
      .def_readwrite("completed", &StreamProfile::completed)
      .def_readwrite("dropped", &StreamProfile::dropped)
      .def_readwrite("latency", &StreamProfile::latency)
      .def_readwrite("maximum_latency", &StreamProfile::maximum_latency)
      .def_readwrite("minimum_latency", &StreamProfile::minimum_latency)
      .def_readwrite("fps", &StreamProfile::fps);
}
}  // namespace cnstream
