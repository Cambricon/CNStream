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

#include <cnstream_config.hpp>

#include <string>

namespace py = pybind11;

namespace cnstream {

namespace detail {

class Pybind11ConfigV : public CNConfigBase {
 public:
  bool ParseByJSONStr(const std::string &jstr) override {
    PYBIND11_OVERRIDE_PURE(bool, CNConfigBase, ParseByJSONStr, jstr);
  }
};  // class Pybind11ConfigV

}  // namespace detail

void ConfigWrapper(py::module &m) {  // NOLINT
  py::class_<CNConfigBase, detail::Pybind11ConfigV>(m, "CNConfigBase")
      .def(py::init())
      .def("parse_by_json_file", &CNConfigBase::ParseByJSONFile)
      .def("parse_by_json_str", &CNConfigBase::ParseByJSONStr)
      .def_readwrite("config_root_dir", &CNConfigBase::config_root_dir);
  py::class_<ProfilerConfig, CNConfigBase>(m, "ProfilerConfig")
      .def(py::init())
      .def("parse_by_json_str", &ProfilerConfig::ParseByJSONStr)
      .def_readwrite("enable_profiling", &ProfilerConfig::enable_profiling)
      .def_readwrite("enable_tracing", &ProfilerConfig::enable_tracing)
      .def_readwrite("trace_event_capacity", &ProfilerConfig::trace_event_capacity);
  py::class_<CNModuleConfig, CNConfigBase>(m, "CNModuleConfig")
      .def(py::init())
      .def("parse_by_json_str", &CNModuleConfig::ParseByJSONStr)
      .def_readwrite("name", &CNModuleConfig::name)
      .def_readwrite("parameters", &CNModuleConfig::parameters)
      .def_readwrite("parallelism", &CNModuleConfig::parallelism)
      .def_readwrite("max_input_queue_size", &CNModuleConfig::maxInputQueueSize)
      .def_readwrite("class_name", &CNModuleConfig::className)
      .def_readwrite("next", &CNModuleConfig::next);
  py::class_<CNSubgraphConfig, CNConfigBase>(m, "CNSubgraphConfig")
      .def(py::init())
      .def("parse_by_json_str", &CNSubgraphConfig::ParseByJSONStr)
      .def_readwrite("name", &CNSubgraphConfig::name)
      .def_readwrite("config_path", &CNSubgraphConfig::config_path)
      .def_readwrite("next", &CNSubgraphConfig::next);
  py::class_<CNGraphConfig, CNConfigBase>(m, "CNGraphConfig")
      .def(py::init())
      .def("parse_by_json_str", &CNGraphConfig::ParseByJSONStr)
      .def_readwrite("name", &CNGraphConfig::name)
      .def_readwrite("profiler_config", &CNGraphConfig::profiler_config)
      .def_readwrite("module_configs", &CNGraphConfig::module_configs)
      .def_readwrite("subgraph_configs", &CNGraphConfig::subgraph_configs);
  m.def("get_path_relative_to_config_file", &GetPathRelativeToTheJSONFile);
}

}  // namespace cnstream

