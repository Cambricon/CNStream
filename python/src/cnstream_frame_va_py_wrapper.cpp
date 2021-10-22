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
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

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
      .def("get_planes", &CNDataFrame::GetPlanes)
      .def("get_plane_bytes", &CNDataFrame::GetPlaneBytes)
      .def("get_bytes", &CNDataFrame::GetBytes)
      .def("image_bgr", [](std::shared_ptr<CNDataFrame> data_frame) {
        cv::Mat bgr_img = data_frame->ImageBGR();
        return MatToArray(bgr_img);
      })
      .def("has_bgr_image", &CNDataFrame::HasBGRImage)
      .def("data", [](const CNDataFrame& data_frame, int plane_idx) {
          return data_frame.data[plane_idx].get();
      }, py::return_value_policy::reference_internal)
      .def_readwrite("frame_id", &CNDataFrame::frame_id)
      .def_readwrite("fmt", &CNDataFrame::fmt)
      .def_readwrite("width", &CNDataFrame::width)
      .def_readwrite("height", &CNDataFrame::height)
      .def_property("stride", [](const CNDataFrame& data_frame) {
          return py::array_t<int>({CN_MAX_PLANES}, {sizeof(int)}, data_frame.stride);
      }, [](std::shared_ptr<CNDataFrame> data_frame, py::array_t<int> strides) {
          py::buffer_info strides_buf = strides.request();
          int size = std::min(static_cast<int>(strides_buf.size), CN_MAX_PLANES);
          memcpy(data_frame->stride, strides_buf.ptr, size * sizeof(int));
      })
      .def_readwrite("ctx", &CNDataFrame::ctx)
      .def_property("dst_device_id", [](const CNDataFrame& data_frame) {
          return data_frame.dst_device_id.load();
      }, [](std::shared_ptr<CNDataFrame> data_frame, int dev_id) {
          data_frame->dst_device_id.store(dev_id);
      });

  py::enum_<CNDataFormat>(m, "CNDataFormat")
      .value("CN_INVALID", CNDataFormat::CN_INVALID)
      .value("CN_PIXEL_FORMAT_YUV420_NV21", CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21)
      .value("CN_PIXEL_FORMAT_YUV420_NV12", CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12)
      .value("CN_PIXEL_FORMAT_BGR24", CNDataFormat::CN_PIXEL_FORMAT_BGR24)
      .value("CN_PIXEL_FORMAT_RGB24", CNDataFormat::CN_PIXEL_FORMAT_RGB24)
      .value("CN_PIXEL_FORMAT_ARGB32", CNDataFormat::CN_PIXEL_FORMAT_ARGB32)
      .value("CN_PIXEL_FORMAT_ABGR32", CNDataFormat::CN_PIXEL_FORMAT_ABGR32)
      .value("CN_PIXEL_FORMAT_RGBA32", CNDataFormat::CN_PIXEL_FORMAT_RGBA32)
      .value("CN_PIXEL_FORMAT_BGRA32", CNDataFormat::CN_PIXEL_FORMAT_BGRA32);

  py::class_<DevContext>(m, "DevContext")
      .def(py::init())
      .def_readwrite("dev_type", &DevContext::dev_type)
      .def_readwrite("dev_id", &DevContext::dev_id);

  py::enum_<DevContext::DevType>(m, "DevType")
    .value("INVALID", DevContext::DevType::INVALID)
    .value("CPU", DevContext::DevType::CPU)
    .value("MLU", DevContext::DevType::MLU);
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


  py::class_<CNInferBoundingBox, std::shared_ptr<CNInferBoundingBox>>(m, "CNInferBoundingBox")
      .def(py::init([]() {
        return std::make_shared<CNInferBoundingBox>();
      }))
      .def(py::init([](float x, float y, float w, float h) {
        auto bbox = std::make_shared<CNInferBoundingBox>();
        bbox->x = x;
        bbox->y = y;
        bbox->w = w;
        bbox->h = h;
        return bbox;
      }))
      .def_readwrite("x", &CNInferBoundingBox::x)
      .def_readwrite("y", &CNInferBoundingBox::y)
      .def_readwrite("w", &CNInferBoundingBox::w)
      .def_readwrite("h", &CNInferBoundingBox::h);


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
