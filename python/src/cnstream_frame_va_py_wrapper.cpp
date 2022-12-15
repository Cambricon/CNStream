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
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"

#include "common_wrapper.hpp"

namespace py = pybind11;

namespace cnstream {

extern std::shared_ptr<py::class_<CNFrameInfo, std::shared_ptr<CNFrameInfo>>> gPyframeRegister;

std::shared_ptr<CNDataFrame> GetCNDataFrame(std::shared_ptr<CNFrameInfo> frame) {
  if (!frame->collection.HasValue(kCNDataFrameTag)) {
    return nullptr;
  }
  return frame->collection.Get<std::shared_ptr<CNDataFrame>>(kCNDataFrameTag);
}

std::shared_ptr<CNInferObjs> GetCNInferObjects(std::shared_ptr<CNFrameInfo> frame) {
  if (!frame->collection.HasValue(kCNInferObjsTag)) {
    return nullptr;
  }
  return frame->collection.Get<std::shared_ptr<CNInferObjs>>(kCNInferObjsTag);
}

void CNDataFrameWrapper(const py::module &m) {
  py::class_<CNDataFrame, std::shared_ptr<CNDataFrame>>(m, "CNDataFrame")
      .def(py::init([]() {
        return std::make_shared<CNDataFrame>();
      }))
      .def("image_bgr", [](std::shared_ptr<CNDataFrame> data_frame) {
        cv::Mat bgr_img = data_frame->ImageBGR();
        return MatToArray(bgr_img);
      })
      .def("has_bgr_image", &CNDataFrame::HasBGRImage)
      .def_readwrite("buf_surf", &CNDataFrame::buf_surf)
      .def_readwrite("frame_id", &CNDataFrame::frame_id);
}

void CNInferObjsWrapper(const py::module &m) {
  py::class_<CNInferObjs, std::shared_ptr<CNInferObjs>>(m, "CNInferObjs")
      .def(py::init([]() {
        return std::make_shared<CNInferObjs>();
      }))
      .def_property("objs", [](std::shared_ptr<CNInferObjs> objs_holder) {
          std::lock_guard<std::mutex> lk(objs_holder->mutex_);
          return objs_holder->objs_;
      }, [](std::shared_ptr<CNInferObjs> objs_holder, std::vector<std::shared_ptr<CNInferObject>> objs) {
          std::lock_guard<std::mutex> lk(objs_holder->mutex_);
          objs_holder->objs_ = objs;
      })
      .def("push_back", [](std::shared_ptr<CNInferObjs> objs_holder, std::shared_ptr<CNInferObject> obj) {
          std::lock_guard<std::mutex> lk(objs_holder->mutex_);
          objs_holder->objs_.push_back(obj);
      });


  py::class_<CNInferAttr, std::shared_ptr<CNInferAttr>>(m, "CNInferAttr")
      .def(py::init([]() {
        return std::make_shared<CNInferAttr>();
      }))
      .def(py::init([](int id, int value, float score) {
        auto attr = std::make_shared<CNInferAttr>();
        attr->id = id;
        attr->value = value;
        attr->score = score;
        return attr;
      }))
      .def_readwrite("id", &CNInferAttr::id)
      .def_readwrite("value", &CNInferAttr::value)
      .def_readwrite("score", &CNInferAttr::score);


  py::class_<CNInferObject, std::shared_ptr<CNInferObject>>(m, "CNInferObject")
      .def(py::init([]() {
        return std::make_shared<CNInferObject>();
      }))
      .def_readwrite("id", &CNInferObject::id)
      .def_readwrite("track_id", &CNInferObject::track_id)
      .def_readwrite("score", &CNInferObject::score)
      .def_readwrite("bbox", &CNInferObject::bbox)
      .def("get_py_collection", [](std::shared_ptr<CNInferObject> obj) {
          if (!obj->collection.HasValue("py_collection")) {
            obj->collection.Add("py_collection", py::dict());
          }
          return obj->collection.Get<py::dict>("py_collection");
      })
      .def("add_attribute", [](std::shared_ptr<CNInferObject> obj, const std::string& key, const CNInferAttr& attr) {
        obj->AddAttribute(key, attr);
      })
      .def("get_attribute", &CNInferObject::GetAttribute)
      .def("add_extra_attribute", &CNInferObject::AddExtraAttribute)
      .def("add_extra_attributes", &CNInferObject::AddExtraAttributes)
      .def("get_extra_attribute", &CNInferObject::GetExtraAttribute)
      .def("remove_extra_attribute", &CNInferObject::RemoveExtraAttribute)
      .def("get_extra_attributes", &CNInferObject::GetExtraAttributes)
      .def("add_feature", &CNInferObject::AddFeature)
      .def("get_feature", &CNInferObject::GetFeature)
      .def("get_features", &CNInferObject::GetFeatures);
}

void CNFrameVaWrapper(const py::module &m) {
  CNDataFrameWrapper(m);
  CNInferObjsWrapper(m);

  gPyframeRegister->def("get_cn_data_frame", &GetCNDataFrame);
  gPyframeRegister->def("get_cn_infer_objects", &GetCNInferObjects);
}

}  //  namespace cnstream
