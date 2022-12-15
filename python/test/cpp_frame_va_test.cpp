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

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"

namespace py = pybind11;

namespace cnstream {

void SetDataFrame(std::shared_ptr<CNFrameInfo> frame, std::shared_ptr<CNDataFrame> dataframe) {
  // TODO(gaoyujia): create buf
  frame->collection.Add(kCNDataFrameTag, dataframe);
}

void SetCNInferobjs(std::shared_ptr<CNFrameInfo> frame, std::shared_ptr<CNInferObjs> objs_holder) {
  frame->collection.Add(kCNInferObjsTag, objs_holder);
}

}  // namespace cnstream

void FrameVaTestWrapper(py::module& m) {  // NOLINT
  m.def("set_data_frame", &cnstream::SetDataFrame);
  m.def("set_infer_objs", &cnstream::SetCNInferobjs);
}

