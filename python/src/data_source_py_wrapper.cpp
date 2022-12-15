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
#include <vector>

#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "cnstream_source.hpp"
#include "data_source.hpp"

namespace py = pybind11;

namespace cnstream {

void DataHandlerWrapper(py::module &m) {  // NOLINT
  py::class_<Resolution, std::shared_ptr<Resolution>>(m, "Resolution")
      .def(py::init(
          [](int width, int height) {
            auto res = std::make_shared<Resolution>();
            res->width = width;
            res->height = height;
            return res;
          }), py::arg("width") = 0, py::arg("height") = 0)
      .def_readwrite("width", &Resolution::width)
      .def_readwrite("height", &Resolution::height);

  py::class_<FileSourceParam, std::shared_ptr<FileSourceParam>>(m, "FileSourceParam")
      .def(py::init())
      .def_readwrite("filename", &FileSourceParam::filename)
      .def_readwrite("framerate", &FileSourceParam::framerate)
      .def_readwrite("loop", &FileSourceParam::loop)
      .def_readwrite("max_res", &FileSourceParam::max_res)
      .def_readwrite("only_key_frame", &FileSourceParam::only_key_frame)
      .def_readwrite("out_res", &FileSourceParam::out_res);

  py::class_<RtspSourceParam, std::shared_ptr<RtspSourceParam>>(m, "RtspSourceParam")
      .def(py::init())
      .def_readwrite("url_name", &RtspSourceParam::url_name)
      .def_readwrite("max_res", &RtspSourceParam::max_res)
      .def_readwrite("use_ffmpeg", &RtspSourceParam::use_ffmpeg)
      .def_readwrite("reconnect", &RtspSourceParam::reconnect)
      .def_readwrite("interval", &RtspSourceParam::interval)
      .def_readwrite("only_key_frame", &RtspSourceParam::only_key_frame)
      .def_readwrite("callback", &RtspSourceParam::callback)
      .def_readwrite("out_res", &RtspSourceParam::out_res);

  py::enum_<ESMemSourceParam::DataType>(m, "ESMemSourceParamDataType")
      .value("INVALID", ESMemSourceParam::DataType::INVALID)
      .value("H264", ESMemSourceParam::DataType::H264)
      .value("H265", ESMemSourceParam::DataType::H265);

  py::class_<ESMemSourceParam, std::shared_ptr<ESMemSourceParam>>(m, "ESMemSourceParam")
      .def(py::init())
      .def_readwrite("max_res", &ESMemSourceParam::max_res)
      .def_readwrite("out_res", &ESMemSourceParam::out_res)
      .def_readwrite("data_type", &ESMemSourceParam::data_type)
      .def_readwrite("only_key_frame", &ESMemSourceParam::only_key_frame);

  py::class_<ESJpegMemSourceParam, std::shared_ptr<ESJpegMemSourceParam>>(m, "ESJpegMemSourceParam")
      .def(py::init())
      .def_readwrite("max_res", &ESJpegMemSourceParam::max_res)
      .def_readwrite("out_res", &ESJpegMemSourceParam::out_res);

  py::class_<ImageFrameSourceParam, std::shared_ptr<ImageFrameSourceParam>>(m, "ImageFrameSourceParam")
      .def(py::init())
      .def_readwrite("out_res", &ImageFrameSourceParam::out_res);

  m.def("create_source", [](DataSource *module, const std::string &stream_id, const FileSourceParam &param) {
      return CreateSource(module, stream_id, param);
  });

  m.def("create_source", [](DataSource *module, const std::string &stream_id, const RtspSourceParam &param) {
      return CreateSource(module, stream_id, param);
  });

  m.def("create_source", [](DataSource *module, const std::string &stream_id, const SensorSourceParam &param) {
      return CreateSource(module, stream_id, param);
  });

  m.def("create_source", [](DataSource *module, const std::string &stream_id, const ESMemSourceParam &param) {
      return CreateSource(module, stream_id, param);
  });

  m.def("create_source", [](DataSource *module, const std::string &stream_id, const ESJpegMemSourceParam &param) {
      return CreateSource(module, stream_id, param);
  });

  m.def("create_source", [](DataSource *module, const std::string &stream_id, const ImageFrameSourceParam &param) {
      return CreateSource(module, stream_id, param);
  });

  m.def("write_mem_package",
      [](std::shared_ptr<SourceHandler>handler, std::vector<unsigned char> data, int size,
         uint64_t pts, bool is_eos) {
        ESPacket pkt;
        pkt.data = data.data();
        pkt.size = size;
        pkt.pts = pts;
        if (is_eos) {
          pkt.flags = static_cast<size_t>(cnstream::ESPacket::FLAG::FLAG_EOS);
        }
        return Write(handler, &pkt);
      }, py::arg().noconvert(), py::arg().noconvert(), py::arg().noconvert(), py::arg().noconvert(),
         py::arg("is_eos") = false);

  m.def("write_jpeg_package",
      [](std::shared_ptr<SourceHandler>handler, std::vector<unsigned char> data, int size,
         uint64_t pts) {
        ESJpegPacket pkt;
        pkt.data = data.data();
        pkt.size = size;
        pkt.pts = pts;
        return Write(handler, &pkt);
      });

  m.def("write_image_frame",
      [](std::shared_ptr<SourceHandler>handler, cnedk::BufSurfWrapperPtr data) {
        ImageFrame pkt;
        pkt.data = data;
        return Write(handler, &pkt);
      });

  py::class_<DataSourceParam>(m, "DataSourceParam")
      .def(py::init())
      .def_readwrite("interval", &DataSourceParam::interval)
      .def_readwrite("device_id", &DataSourceParam::device_id)
      .def_readwrite("bufpool_size", &DataSourceParam::bufpool_size);

  py::class_<DataSource, std::shared_ptr<DataSource>, SourceModule>(m, "DataSource")
      .def(py::init<const std::string&>())
      .def("check_param_set", &DataSource::CheckParamSet)
      .def("get_source_param", &DataSource::GetSourceParam);
}

}  // namespace cnstream
