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

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#if (CV_MAJOR_VERSION >= 3)
#include <opencv2/imgcodecs/imgcodecs.hpp>
#endif

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <vector>

#include "cnstream_pipeline.hpp"
#include "util.hpp"

namespace py = pybind11;

namespace cnstream {

py::dtype GetNpDType(int depth) {
  switch (depth) {
    case CV_8U:
      return py::dtype::of<uint8_t>();
    case CV_8S:
      return py::dtype::of<int8_t>();
    case CV_16U:
      return py::dtype::of<uint16_t>();
    case CV_16S:
      return py::dtype::of<int16_t>();
    case CV_32S:
      return py::dtype::of<int32_t>();
    case CV_32F:
      return py::dtype::of<float>();
    case CV_64F:
      return py::dtype::of<double>();
    default:
      throw std::invalid_argument("Data type is not supported.");
  }
}

std::vector<std::size_t> GetShape(cv::Mat& m) {  // NOLINT
  if (m.channels() == 1) {
    return {static_cast<size_t>(m.rows), static_cast<size_t>(m.cols)};
  }
  return {static_cast<size_t>(m.rows), static_cast<size_t>(m.cols), static_cast<size_t>(m.channels())};
}

py::capsule MakeCapsule(cv::Mat& m) {  // NOLINT
  return py::capsule(new cv::Mat(m), [](void *v) { delete reinterpret_cast<cv::Mat*>(v); });
}

py::array MatToArray(cv::Mat& m) {  // NOLINT
  if (!m.isContinuous()) {
    throw std::invalid_argument("Only continuous Mat supported.");
  }
  return py::array(GetNpDType(m.depth()), GetShape(m), m.data, MakeCapsule(m));
}


void PerfPrintWrapper(py::module &m) {  // NOLINT
  m.def("print_pipeline_performance", [](Pipeline *pipeline) {
    PrintPipelinePerformance("Whole", pipeline->GetProfiler()->GetProfile());
  });
  m.def("print_pipeline_performance", [](Pipeline *pipeline, int time_in_ms) {
    Duration duration(time_in_ms);
    PrintPipelinePerformance("Last two seconds",
                             pipeline->GetProfiler()->GetProfileBefore(Clock::now(), duration));
  });
}

}  // namespace cnstream
