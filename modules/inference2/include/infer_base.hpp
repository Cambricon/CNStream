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

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "cnis/contrib/video_helper.h"
#include "cnis/infer_server.h"
#include "cnis/processor.h"
#include "cnstream_frame_va.hpp"
#include "frame_filter.hpp"
#include "obj_filter.hpp"
#include "video_postproc.hpp"
#include "video_preproc.hpp"

namespace cnstream {

using InferEngine = infer_server::InferServer;
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

using InferDataPtr = infer_server::InferDataPtr;
using InferBatchData = infer_server::BatchData;
using InferUserData = infer_server::any;
using InferShape = infer_server::Shape;
using InferCpuPreprocess = infer_server::PreprocessorHost;
using InferPostprocess = infer_server::Postprocessor;

/**
 * @brief The inference parameters used in Inferencer2 Module.
 */
struct Infer2Param {
  uint32_t device_id = 0;
  uint32_t priority = 0;
  uint32_t engine_num = 1;
  bool show_stats = false;
  InferBatchStrategy batch_strategy = InferBatchStrategy::DYNAMIC;
  uint32_t batching_timeout = 1000;   ///< only support in dynamic batch strategy
  bool keep_aspect_ratio = false;
  InferVideoPixelFmt model_input_pixel_format = InferVideoPixelFmt::RGBA;
  InferDimOrder data_order = InferDimOrder::NHWC;
  std::vector<float> mean_;
  std::vector<float> std_;
  std::string func_name = "";
  std::string model_path = "";
  std::string preproc_name = "";
  std::string postproc_name = "";
  std::string frame_filter_name = "";
  std::string obj_filter_name = "";
  bool normalize = false;
  bool object_infer = false;
  float threshold = 0.f;
  uint32_t infer_interval = 0;
  std::unordered_map<std::string, std::string> custom_preproc_params;
  std::unordered_map<std::string, std::string> custom_postproc_params;
};  // struct Infer2Param

class Inferencer2;
class InferHandler {
 public:
  explicit InferHandler(Inferencer2* module, const Infer2Param& infer_params,
                        std::shared_ptr<VideoPostproc> post_processor, std::shared_ptr<VideoPreproc> pre_processor,
                        std::shared_ptr<FrameFilter> frame_filter, std::shared_ptr<ObjFilter> obj_filter)
      : module_(module),
        params_(infer_params),
        postprocessor_(post_processor),
        preprocessor_(pre_processor),
        frame_filter_(frame_filter),
        obj_filter_(obj_filter) {}

  virtual ~InferHandler() {}

  virtual bool Open() = 0;
  virtual void Close() = 0;

  virtual int Process(CNFrameInfoPtr data, bool with_objs = false) = 0;
  virtual void WaitTaskDone(const std::string& stream_id) = 0;

  void TransmitData(const CNFrameInfoPtr& data);

 protected:
  Inferencer2* module_ = nullptr;
  Infer2Param params_;
  std::shared_ptr<VideoPostproc> postprocessor_ = nullptr;
  std::shared_ptr<VideoPreproc> preprocessor_ = nullptr;
  std::shared_ptr<FrameFilter> frame_filter_ = nullptr;
  std::shared_ptr<ObjFilter> obj_filter_ = nullptr;
};

}  // namespace cnstream
#endif
