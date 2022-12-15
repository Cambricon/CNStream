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
#include <memory>
#include <string>
#include <map>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "cnstream_module.hpp"
#include "data_source.hpp"

namespace py = pybind11;

namespace cnstream {

class TestIModuleObserver : public IModuleObserver {
 public:
  void Notify(std::shared_ptr<CNFrameInfo> data) override {
    cnstream::RwLockWriteGuard guard(lock_);
    if (stream_counts_.find(data->stream_id) != stream_counts_.end()) {
      stream_counts_[data->stream_id]++;
    } else {
      stream_counts_.insert({data->stream_id, 1});
    }
    // auto frame = data->collection.Get<std::shared_ptr<CNDataFrame>>(kCNDataFrameTag);
    // auto mat = frame->ImageBGR();
    // cv::imwrite("./output/" + data->stream_id + "_" + std::to_string(frame->frame_id) + ".jpg", mat);
  }
  int GetCount(std::string stream_id) {
    cnstream::RwLockReadGuard guard(lock_);
    if (stream_counts_.find(stream_id) != stream_counts_.end()) {
      return stream_counts_[stream_id];
    } else {
      return -1;
    }
  }

 private:
  int count_ = 0;
  cnstream::RwLock lock_;
  std::map<std::string, int> stream_counts_;
};

class CppDataHanlderTestHelper {
 public:
  CppDataHanlderTestHelper() {}
  void SetObserver(std::shared_ptr<DataSource> module) {
    observer_ = new TestIModuleObserver;
    module->SetObserver(observer_);
  }
  int GetCount(std::string stream_id) {
    if (observer_) {
      return observer_->GetCount(stream_id);
    }
    return -1;
  }
 private:
  TestIModuleObserver* observer_ = nullptr;
};  //  class CppDataHanlderTestHelper

}  // namespace cnstream

void DataHanlderWrapper(const py::module& m) {
  py::class_<cnstream::CppDataHanlderTestHelper>(m, "CppDataHanlderTestHelper")
    .def(py::init())
    .def("set_observer", &cnstream::CppDataHanlderTestHelper::SetObserver)
    .def("get_count", &cnstream::CppDataHanlderTestHelper::GetCount);
}
