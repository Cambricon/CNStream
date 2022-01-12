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

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cnstream_pipeline.hpp>
#include <cnstream_source.hpp>

#include <memory>
#include <string>

namespace py = pybind11;

namespace cnstream {

namespace detail {

class Pybind11StreamMsgObserverV : public StreamMsgObserver {
 public:
  void Update(const StreamMsg &msg) override { PYBIND11_OVERRIDE_PURE(void, StreamMsgObserver, update, msg); }
};  // class Pybind11StreamMsgObserverV

}  // namespace detail

void PipelineWrapper(py::module &m) {  // NOLINT
  py::enum_<StreamMsgType>(m, "StreamMsgType")
      .value("eos_msg", StreamMsgType::EOS_MSG)
      .value("error_msg", StreamMsgType::ERROR_MSG)
      .value("stream_err_msg", StreamMsgType::STREAM_ERR_MSG)
      .value("frame_err_msg", StreamMsgType::FRAME_ERR_MSG);
  py::class_<StreamMsg>(m, "StreamMsg")
      .def_readwrite("type", &StreamMsg::type)
      .def_readwrite("stream_id", &StreamMsg::stream_id)
      .def_readwrite("module_name", &StreamMsg::module_name)
      .def_readwrite("pts", &StreamMsg::pts);
  py::class_<StreamMsgObserver, detail::Pybind11StreamMsgObserverV>(m, "StreamMsgObserver")
      .def(py::init<>())
      .def("update", &StreamMsgObserver::Update);
  py::class_<Pipeline>(m, "Pipeline")
      .def(py::init<std::string>())
      .def("get_name", &Pipeline::GetName)
      .def("build_pipeline", static_cast<bool (Pipeline::*)(const CNGraphConfig &)>(&Pipeline::BuildPipeline))
      .def("build_pipeline_by_json_file", &Pipeline::BuildPipelineByJSONFile)
      .def("start", &Pipeline::Start)
      .def("stop", &Pipeline::Stop, py::call_guard<py::gil_scoped_release>())
      .def("is_running", &Pipeline::IsRunning)
      .def("get_source_module",
           [](Pipeline *pipeline, const std::string &module_name) {
             return dynamic_cast<SourceModule *>(pipeline->GetModule(module_name));
           },
           py::return_value_policy::reference)
      .def("get_module",
           [](Pipeline *pipeline, const std::string &module_name) {
             return dynamic_cast<Module *>(pipeline->GetModule(module_name));
           },
           py::return_value_policy::reference)
      .def("get_module_config", &Pipeline::GetModuleConfig)
      .def("is_profiling_enabled", &Pipeline::IsProfilingEnabled)
      .def("is_tracing_enabled", &Pipeline::IsTracingEnabled)
      .def("provide_data",
           [](Pipeline *pipeline, SourceModule *module, std::shared_ptr<CNFrameInfo> frame_info) {
             return pipeline->ProvideData(module, frame_info);
           })
      .def_property("stream_msg_observer",
                    py::cpp_function(&Pipeline::GetStreamMsgObserver, py::return_value_policy::reference),
                    py::cpp_function(&Pipeline::SetStreamMsgObserver, py::keep_alive<1, 2>()))
      .def("is_root_node", &Pipeline::IsRootNode)
      .def("register_frame_done_callback", &Pipeline::RegisterFrameDoneCallBack)
      .def("get_profile", [](Pipeline *pipeline) {
        return pipeline->GetProfiler()->GetProfile();
      })
      .def("get_profile_before", [](Pipeline *pipeline, int time_in_ms) {
        Duration duration(time_in_ms);
        return pipeline->GetProfiler()->GetProfileBefore(Clock::now(), duration);
      });
}

}  // namespace cnstream

