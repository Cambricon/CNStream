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

#ifndef MODULES_INFER_BASE_HPP_
#define MODULES_INFER_BASE_HPP_

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "cnstream_core.hpp"
#include "cnstream_frame_va.hpp"
#include "reflex_object.h"
#include "infer_server.h"
#include "preproc.hpp"
#include "processor.h"
#include "video_helper.h"

namespace cnstream {

using InferEngine = infer_server::video::VideoInferServer;
using InferVideoPixelFmt = infer_server::video::PixelFmt;
using InferVideoFrame = infer_server::video::VideoFrame;
using VFrameBoundingBox = infer_server::video::BoundingBox;
using InferPreprocessType = infer_server::video::PreprocessType;
using InferMluPreprocess = infer_server::video::PreprocessorMLU;

using InferDataType = infer_server::DataType;
using InferDimOrder = infer_server::DimOrder;
using InferStatus = infer_server::Status;
using InferBatchStrategy = infer_server::BatchStrategy;

using InferModelInfoPtr = infer_server::ModelPtr;
using InferEngineSession = infer_server::Session_t;
using InferSessionDesc = infer_server::SessionDesc;
using InferEngineDataObserver = infer_server::Observer;
using InferPackagePtr = infer_server::PackagePtr;
using InferBuffer = infer_server::Buffer;

// using InferData = infer_server::InferData;
using InferDataPtr = infer_server::InferDataPtr;
using InferBatchData = infer_server::BatchData;
using InferUserData = infer_server::any;
using InferShape = infer_server::Shape;
using InferCpuPreprocess = infer_server::PreprocessorHost;
using InferPostprocess = infer_server::Postprocessor;

using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;

/**
 * @brief for inference parameters used in Inferencer2 Module.
 */
struct InferParamerters {
  uint32_t device_id = 0;  ///< device_id is set to 0 by default
  int priority = 0;
  uint32_t engine_num = 1;
  bool show_stats = false;                                          ///< show_stats is set to false by default
  InferBatchStrategy batch_strategy = InferBatchStrategy::DYNAMIC;  ///< batch_type is set to dynamic by default
  uint32_t batch_timeout = 1000;  ///< batching_timeout invalid when batch type is static, default 1000
  bool keep_aspect_ratio = false;  ///< mlu preprocessing, keep aspect ratio
  CNDataFormat model_input_pixel_format = CN_PIXEL_FORMAT_RGBA32;
  InferDimOrder data_order = InferDimOrder::NHWC;
  std::string func_name;
  std::string model_path;
  std::string preproc_name = "";
  bool object_infer = false;
};  // struct InferParams

class Inferencer2;
class Preproc;
class VideoPostproc;
class InferHandler {
 public:
  explicit InferHandler(Inferencer2* module, InferParamerters infer_params,
                        std::shared_ptr<VideoPostproc> post_processor, std::shared_ptr<Preproc> pre_processor) {
    module_ = module;
    params_ = infer_params;
    preprocessor_ = pre_processor;
    postprocessor_ = post_processor;
  }

  virtual ~InferHandler() {}

  virtual bool Open() = 0;
  virtual void Close() = 0;

  virtual int Process(CNFrameInfoPtr data, bool with_objs = false) = 0;
  virtual void WaitTaskDone(const std::string& stream_id) = 0;

  void TransmitData(const CNFrameInfoPtr& data);

 protected:
  Inferencer2* module_ = nullptr;
  InferParamerters params_;
  std::shared_ptr<VideoPostproc> postprocessor_ = nullptr;
  std::shared_ptr<Preproc> preprocessor_ = nullptr;
};

}  // namespace cnstream
#endif
