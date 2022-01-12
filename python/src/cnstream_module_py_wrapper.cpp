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

#include <cnstream_module.hpp>
#include <cnstream_pipeline.hpp>

#include <memory>
#include <string>
#include <utility>

#include "pymodule.h"

namespace py = pybind11;

namespace cnstream {

PyModule::~PyModule() {
  py::gil_scoped_acquire gil;
  pyon_eos_.release();
  pyprocess_.release();
  pyclose_.release();
  pyopen_.release();
  pyinstance_.release();
}

namespace detail {

template<typename ModuleBase>
class Pybind11ModuleV : public ModuleBase {
 public:
  using ModuleBase::ModuleBase;
  bool Open(ModuleParamSet params) override {
    PYBIND11_OVERRIDE_PURE(
        bool,
        Module,
        Open,
        params);
  }
  void Close() override {
    PYBIND11_OVERRIDE_PURE(
        void,
        Module,
        Close);
  }
  int Process(std::shared_ptr<CNFrameInfo> data) override {
    PYBIND11_OVERRIDE_PURE(
        int,
        Module,
        Process,
        data);
  }
  void OnEos(const std::string &stream_id) override {
    PYBIND11_OVERRIDE(
        void,
        Module,
        OnEos,
        stream_id);
  }
};  // class Pybind11ModuleV

class Pybind11Module : public Module {
 public:
  using Module::Module;
  PyModule* proxy_ = nullptr;
};  // class Pybind11Module

class Pybind11ModuleEx : public Pybind11Module {
 public:
  explicit Pybind11ModuleEx(const std::string &name) : Pybind11Module(name) { hasTransmit_.store(true); }
};  // class Pybind11ModuleEx

class Pybind11IModuleObserverV : public IModuleObserver {
 public:
  void notify(std::shared_ptr<CNFrameInfo> frame) override {
    PYBIND11_OVERRIDE_PURE(void, IModuleObserver, notify, frame);
  }
};  // class Pybind11IModuleObserverV

}  // namespace detail

PyModule::PyModule(const std::string& name) : ModuleEx(name) {
  param_register_.Register("pyclass_name", "Required. Module class name in python --- "
      "type : [string] --- "
      "default value : [\"\"]");
}

bool PyModule::CheckParamSet(const ModuleParamSet &params) const {
  if (params.find("pyclass_name") == params.end()) {
    LOGE(PyModule) << "pyclass_name must be set.";
    return false;
  }
  return true;
}

std::pair<std::string, std::string> SplitPyModuleAndClass(const std::string &fullname) {
  std::string pymodule_name = "";
  std::string pyclass_name = fullname;
  size_t last_point_pos = fullname.rfind('.');
  if (std::string::npos != last_point_pos) {
    pymodule_name = fullname.substr(0, last_point_pos);
    pyclass_name = fullname.substr(last_point_pos + 1, std::string::npos);
  }
  if (pymodule_name.empty()) pymodule_name = "__main__";
  return std::make_pair(pymodule_name, pyclass_name);
}

bool PyModule::Open(ModuleParamSet params) {
  auto pyclass_name_iter = params.find("pyclass_name");
  if (pyclass_name_iter == params.end()) {
    LOGE(PyModule) << "pyclass_name must be set.";
    return false;
  }

  py::gil_scoped_acquire gil;
  try {
    auto t = SplitPyModuleAndClass(pyclass_name_iter->second);
    std::string pymodule_name = std::move(t.first);
    std::string pyclass_name = std::move(t.second);
    py::module pymodule = py::module::import(pymodule_name.c_str());
    pyinstance_ = pymodule.attr(pyclass_name.c_str())(GetName());
    py::cast<detail::Pybind11Module*>(pyinstance_)->proxy_ = this;
    pyopen_ = pyinstance_.attr("open");
    pyclose_ = pyinstance_.attr("close");
    pyprocess_ = pyinstance_.attr("process");
    pyon_eos_ = pyinstance_.attr("on_eos");
    params.erase(pyclass_name_iter);
    instance_has_transmit_ = py::cast<bool>(pyinstance_.attr("has_transmit")());
    return py::cast<bool>(pyopen_(params));
  } catch (std::runtime_error e) {
    LOGE(PyModule) << "pyclass_name : [" << pyclass_name_iter->second << "]. " << e.what();
    return false;
  }

  return true;
}

void PyModule::Close() {
  py::gil_scoped_acquire gil;
  try {
    pyclose_();
  } catch (std::runtime_error e) {
    LOGF(PyModule) << GetName() << " call close failed : " << e.what();
  }
}

int PyModule::Process(std::shared_ptr<CNFrameInfo> data) {
  {
    py::gil_scoped_acquire gil;
    if (instance_has_transmit_) {
      return py::cast<int>(pyprocess_(data));
    } else {
      if (data->IsEos()) {
        pyon_eos_(data->stream_id);
      } else {
        try {
          int ret = py::cast<int>(pyprocess_(data));
          if (ret) return ret;
        } catch (std::runtime_error e) {
          LOGF(PyModule) << GetName() << " call process failed : " << e.what();
        }
      }
    }
  }
  // do not hold gil before calling TransmitData or a deadlock will occur
  TransmitData(data);
  return 0;
}

void ModuleWrapper(py::module &m) {  // NOLINT
  py::enum_<EventType>(m, "EventType")
      .value("event_invalid", EventType::EVENT_INVALID)
      .value("event_error", EventType::EVENT_ERROR)
      .value("event_warning", EventType::EVENT_WARNING)
      .value("event_stream_error", EventType::EVENT_STREAM_ERROR)
      .export_values();
  py::class_<detail::Pybind11Module, detail::Pybind11ModuleV<detail::Pybind11Module>>(m, "Module")
      .def(py::init<const std::string&>())
      .def("open", &detail::Pybind11Module::Open)
      .def("close", &detail::Pybind11Module::Close)
      .def("process", &detail::Pybind11Module::Process)
      .def("on_eos", &detail::Pybind11Module::OnEos)
      .def("get_name", &detail::Pybind11Module::GetName)
      .def("post_event", [] (detail::Pybind11Module* module, EventType type, const std::string &smsg) {
            return module->proxy_ ? module->proxy_->PostEvent(type, smsg) : false;
          })
      .def("transmit_data", [] (detail::Pybind11Module* module, std::shared_ptr<CNFrameInfo> data) {
            return module->HasTransmit() ? (module->proxy_ ? module->proxy_->TransmitData(data) : false) : false;
          }, py::call_guard<py::gil_scoped_release>())
      .def("has_transmit", &Module::HasTransmit)
      .def("get_container", [] (detail::Pybind11Module* module) {
            return module->proxy_ ? module->proxy_->GetContainer() : nullptr;
          });
  py::class_<detail::Pybind11ModuleEx, detail::Pybind11Module,
      detail::Pybind11ModuleV<detail::Pybind11ModuleEx>>(m, "ModuleEx")
      .def(py::init<const std::string&>());
  py::class_<IModuleObserver, detail::Pybind11IModuleObserverV>(m, "ModuleObserver")
      .def(py::init<>())
      .def("notify", &IModuleObserver::notify);
  py::class_<Module>(m, "CModule")
      .def("set_module_observer", &Module::SetObserver);
}

}  // namespace cnstream

