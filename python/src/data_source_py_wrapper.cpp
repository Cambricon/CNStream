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

#include <memory>
#include <string>
#include <vector>

#include "cnstream_source.hpp"
#include "data_source.hpp"

namespace py = pybind11;

namespace cnstream {

void DataHandlerWrapper(const py::module &m) {
  py::class_<MaximumVideoResolution>(m, "MaximumVideoResolution")
      .def(py::init())
      .def_readwrite("enable_variable_resolutions", &MaximumVideoResolution::enable_variable_resolutions)
      .def_readwrite("maximum_width", &MaximumVideoResolution::maximum_width)
      .def_readwrite("maximum_height", &MaximumVideoResolution::maximum_height);
  py::enum_<OutputType>(m, "OutputType")
      .value("output_cpu", OutputType::OUTPUT_CPU)
      .value("output_mlu", OutputType::OUTPUT_MLU);
  py::enum_<DecoderType>(m, "DecoderType")
      .value("decoder_cpu", DecoderType::DECODER_CPU)
      .value("decoder_mlu", DecoderType::DECODER_MLU);
  py::class_<DataSourceParam>(m, "DataSourceParam")
      .def(py::init())
      .def_readwrite("output_type", &DataSourceParam::output_type_)
      .def_readwrite("interval", &DataSourceParam::interval_)
      .def_readwrite("decoder_type", &DataSourceParam::decoder_type_)
      .def_readwrite("reuse_cndec_buf", &DataSourceParam::reuse_cndec_buf)
      .def_readwrite("device_id", &DataSourceParam::device_id_)
      .def_readwrite("input_buf_number", &DataSourceParam::input_buf_number_)
      .def_readwrite("output_buf_number", &DataSourceParam::output_buf_number_)
      .def_readwrite("apply_stride_align_for_scaler", &DataSourceParam::apply_stride_align_for_scaler_);
  py::class_<DataSource, std::shared_ptr<DataSource>, SourceModule>(m, "DataSource")
      .def(py::init<const std::string&>())
      .def("open", &DataSource::Open)
      .def("close", &DataSource::Close)
      .def("check_param_set", &DataSource::CheckParamSet)
      .def("get_source_param", &DataSource::GetSourceParam);

  py::class_<FileHandler, std::shared_ptr<FileHandler>, SourceHandler>(m, "FileHandler")
      .def(py::init([](DataSource *module, const std::string &stream_id,
                       const std::string &filename, int framerate, bool loop,
                       const MaximumVideoResolution& maximum_resolution) {
        auto file_handler = FileHandler::Create(module, stream_id, filename, framerate,
                                                loop, maximum_resolution);
        return std::dynamic_pointer_cast<FileHandler>(file_handler);
      }), py::arg("module"), py::arg("stream_id"), py::arg("filename"),
      py::arg("framerate"), py::arg("loop") = false,
      py::arg("maximum_resolution") = MaximumVideoResolution{})
      .def("open", &FileHandler::Open)
      .def("close", &FileHandler::Close);

  py::class_<RtspHandler, std::shared_ptr<RtspHandler>, SourceHandler>(m, "RtspHandler")
      .def(py::init([](DataSource *module, const std::string &stream_id,
                       const std::string &url_name, bool use_ffmpeg,
                       int reconnect, const MaximumVideoResolution& maximum_resolution) {
        auto rtsp_handler = RtspHandler::Create(module, stream_id, url_name, use_ffmpeg,
                                                reconnect, maximum_resolution);
        return std::dynamic_pointer_cast<RtspHandler>(rtsp_handler);
      }), py::arg("module"), py::arg("stream_id"), py::arg("url_name"),
      py::arg("use_ffmpeg") = false, py::arg("reconnect") = 10,
      py::arg("maximum_resolution") = MaximumVideoResolution{})
      .def("open", &RtspHandler::Open)
      .def("close", &RtspHandler::Close);

  py::class_<RawImgMemHandler, std::shared_ptr<RawImgMemHandler>, SourceHandler>(m, "RawImgMemHandler")
      .def(py::init([](DataSource *module, const std::string &stream_id) {
        auto handler = RawImgMemHandler::Create(module, stream_id);
        return std::dynamic_pointer_cast<RawImgMemHandler>(handler);
      }))
      .def("open", &RawImgMemHandler::Open)
      .def("close", &RawImgMemHandler::Close)
      .def("write", [](std::shared_ptr<RawImgMemHandler> handler, const py::array_t<uint8_t>& data_array,
          const uint64_t pts, const CNDataFormat pixel_fmt = CNDataFormat::CN_PIXEL_FORMAT_BGR24) {
        py::buffer_info buf = data_array.request();
        switch (pixel_fmt) {
          case CNDataFormat::CN_PIXEL_FORMAT_BGR24:
          case CNDataFormat::CN_PIXEL_FORMAT_RGB24:
            if (buf.ndim != 3) {
              std::cout << "For RGB24/BGR24 data, the dim should be 3, but dim = " << buf.ndim << std::endl;
              return -1;
            }
            break;
          case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
          case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
            if (buf.ndim != 2) {
              std::cout << "For YUVNV21/12 data, the dim should be 2, but dim = " << buf.ndim << std::endl;
              return -1;
            }
            break;
          default:
            std::cout << "Only support pixel format RGB24/BGR24/NV12/NV21" << std::endl;
            return -1;
        }
        int width, height;
        if (buf.ndim == 3) {
          width = buf.shape[1];
          height = buf.shape[0];
        } else {
          width = buf.shape[1];
          height = buf.shape[0] * 2 / 3;
        }
        return handler->Write(reinterpret_cast<uint8_t*>(buf.ptr), buf.size, pts, width, height, pixel_fmt);
      }, py::arg().noconvert(), py::arg().noconvert(), py::arg("pixel_fmt") = CNDataFormat::CN_PIXEL_FORMAT_BGR24);
  py::class_<ESJpegMemHandler, std::shared_ptr<ESJpegMemHandler>, SourceHandler>(m, "ESJpegMemHandler")
      .def(py::init([](DataSource *module, const std::string &stream_id,
                       int max_width, int max_height) {
        auto handler = ESJpegMemHandler::Create(module, stream_id, max_width, max_height);
        return std::dynamic_pointer_cast<ESJpegMemHandler>(handler);
      }), py::arg("module"), py::arg("stream_id"),
      py::arg("max_width") = 7680, py::arg("max_height") = 4320)
      .def("open", &ESJpegMemHandler::Open)
      .def("close", &ESJpegMemHandler::Close)
      .def("write", [](std::shared_ptr<ESJpegMemHandler> handler, std::vector<unsigned char> data,
          int size, uint64_t pts, bool is_eos) {
        cnstream::ESPacket pkt;
        pkt.data = data.data();
        pkt.size = size;
        pkt.pts = pts;
        if (is_eos) {
          pkt.flags = static_cast<size_t>(cnstream::ESPacket::FLAG::FLAG_EOS);
        }
        return handler->Write(&pkt);
      }, py::arg().noconvert(), py::arg().noconvert(), py::arg().noconvert(), py::arg("is_eos") = false);
}

}  // namespace cnstream
