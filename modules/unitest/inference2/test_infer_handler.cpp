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

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "easyinfer/mlu_memory_op.h"
#include "cnstream_logging.hpp"

#include "inferencer2.hpp"
#include "video_postproc.hpp"
#include "video_preproc.hpp"
#include "test_base.hpp"
#include "infer_handler.hpp"
#include "infer_params.hpp"

namespace cnstream {

static std::string GetModelPath() {
  edk::MluContext ctx;
  edk::CoreVersion core_ver = ctx.GetCoreVersion();
  std::string model_path = "";
  switch (core_ver) {
    case edk::CoreVersion::MLU220:
      model_path = "../../data/models/yolov3_b4c4_argb_mlu220.cambricon";
      break;
    case edk::CoreVersion::MLU270:
    default:
      model_path = "../../data/models/yolov3_b4c4_argb_mlu270.cambricon";
      break;
  }
  return model_path;
}

static std::string GetModelPathMM() { return "../../data/models/resnet50_nhwc.model"; }

// the data is related to model
static cnstream::CNFrameInfoPtr CreatData(std::string device_id, bool is_eos = false, bool mlu_data = true) {
  auto data = cnstream::CNFrameInfo::Create(device_id, is_eos);
  cv::Mat image = cv::imread(GetExePath() + "../../data/images/0.jpg");
  int width = image.cols;
  int height = image.rows;
  size_t nbytes = width * height * sizeof(uint8_t) * 3;

  data->stream_id = "1";
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  if (mlu_data) {
    void *frame_data = image.data;
    void *planes[CN_MAX_PLANES] = {nullptr, nullptr};
    edk::MluMemoryOp mem_op;
    frame_data = mem_op.AllocMlu(nbytes);
    planes[0] = frame_data;                                                                        // y plane
    planes[1] = reinterpret_cast<void *>(reinterpret_cast<int64_t>(frame_data) + width * height);  // uv plane
    void *ptr_mlu[2] = {planes[0], planes[1]};
    frame->ctx.dev_type = DevContext::DevType::MLU;
    frame->ctx.ddr_channel = std::stoi(device_id);
    frame->ctx.dev_id = std::stoi(device_id);
    frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12;
    frame->dst_device_id = std::stoi(device_id);
    frame->frame_id = 1;
    data->timestamp = 1000;
    frame->width = width;
    frame->height = height;
    frame->stride[0] = frame->stride[1] = width;
    frame->CopyToSyncMem(ptr_mlu, true);
    std::shared_ptr<CNInferObjs> objs(new (std::nothrow) CNInferObjs());
    data->collection.Add(kCNDataFrameTag, frame);
    data->collection.Add(kCNInferObjsTag, objs);
    return data;
  } else {
    frame->frame_id = 1;
    data->timestamp = 1000;
    frame->width = width;
    frame->height = height;
    void *ptr_cpu[2] = {image.data, image.data + nbytes * 2 / 3};
    frame->stride[0] = frame->stride[1] = width;
    frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12;
    frame->ctx.dev_type = DevContext::DevType::CPU;
    frame->dst_device_id = std::stoi(device_id);
    frame->ctx.dev_id = std::stoi(device_id);
    frame->CopyToSyncMem(ptr_cpu, true);

    std::shared_ptr<CNInferObjs> objs(new (std::nothrow) CNInferObjs());
    data->collection.Add(kCNDataFrameTag, frame);
    data->collection.Add(kCNInferObjsTag, objs);
    return data;
  }
  return nullptr;
}

TEST(Inferencer2, InferHandlerOpen) {
  std::string exe_path = GetExePath();
  std::string infer_name = "detector";
  std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
  Inferencer2 *Infer2;
  Infer2 = infer.get();
  std::string preproc_name = "VideoPreprocCpu";
  std::string postproc_name = "VideoPostprocSsd";
  std::shared_ptr<VideoPreproc> pre_processor(VideoPreproc::Create(preproc_name));
  std::shared_ptr<VideoPostproc> post_processor(VideoPostproc::Create(postproc_name));

  bool use_magicmind = infer_server::Predictor::Backend() == "magicmind";
  Infer2Param param;
  if (use_magicmind) {
    param.model_path = exe_path + GetModelPathMM();
    param.model_input_pixel_format = InferVideoPixelFmt::RGB24;
  } else {
    param.model_path = exe_path + GetModelPath();
    param.func_name = "subnet0";
    param.model_input_pixel_format = InferVideoPixelFmt::ARGB;
  }
  param.device_id = 0;
  param.batch_strategy = InferBatchStrategy::STATIC;
  param.batching_timeout = 300;
  param.priority = 0;
  param.show_stats = false;
  param.engine_num = 2;
  param.object_infer = false;

  {  // open sucess, preproc = VideoPreprocCpu
    param.preproc_name = "VideoPreprocCpu";
    std::shared_ptr<InferHandler> infer_handler =
        std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor, nullptr, nullptr);
    EXPECT_TRUE(infer_handler->Open());
  }

  {  // preproc_name = RCOP
    if (!use_magicmind) {
      param.preproc_name = "RCOP";
      std::shared_ptr<InferHandler> infer_handler =
          std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor, nullptr, nullptr);
      EXPECT_TRUE(infer_handler->Open());
    } else {
      param.preproc_name = "CNCV";
      std::shared_ptr<InferHandler> infer_handler =
          std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor, nullptr, nullptr);
      EXPECT_TRUE(infer_handler->Open());
    }
  }

  {  // preproc_name = SCALER
    if (!use_magicmind) {
      edk::MluContext ctx;
      edk::CoreVersion core_ver = ctx.GetCoreVersion();
      if (core_ver == edk::CoreVersion::MLU220) {
        param.preproc_name = "SCALER";
        std::shared_ptr<InferHandler> infer_handler =
            std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor, nullptr, nullptr);
        EXPECT_TRUE(infer_handler->Open());
      }
    }
  }
}

class HasObjectsFilter : public cnstream::FrameFilter {
 public:
  bool Filter(const cnstream::CNFrameInfoPtr& finfo) override {
    CNInferObjsPtr objs_holder = finfo->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
    std::lock_guard<std::mutex> lg(objs_holder->mutex_);
    return !objs_holder->objs_.empty();
  }

  DECLARE_REFLEX_OBJECT_EX(HasObjectsFilter, cnstream::FrameFilter)
};  // classd HasObjectsFilter

IMPLEMENT_REFLEX_OBJECT_EX(HasObjectsFilter, cnstream::FrameFilter)

TEST(Inferencer2, InferHandlerProcess) {
  std::string exe_path = GetExePath();
  std::string infer_name = "detector";
  std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
  Inferencer2 *Infer2;
  Infer2 = infer.get();
  std::string preproc_name = "VideoPreprocCpu";
  std::string postproc_name = "VideoPostprocSsd";
  std::string frame_filter_name = "HasObjectsFilter";
  std::string obj_filter_name = "VehicleFilter";
  std::shared_ptr<VideoPreproc> pre_processor(VideoPreproc::Create(preproc_name));
  std::shared_ptr<VideoPostproc> post_processor(VideoPostproc::Create(postproc_name));
  std::shared_ptr<FrameFilter> frame_filter(FrameFilter::Create(frame_filter_name));
  std::shared_ptr<ObjFilter> obj_filter(ObjFilter::Create(obj_filter_name));
  bool use_magicmind = infer_server::Predictor::Backend() == "magicmind";

  Infer2Param param;
  if (use_magicmind) {
    param.model_path = exe_path + GetModelPathMM();
    param.model_input_pixel_format = InferVideoPixelFmt::RGB24;
  } else {
    param.model_path = exe_path + GetModelPath();
    param.func_name = "subnet0";
    param.model_input_pixel_format = InferVideoPixelFmt::ARGB;
  }
  param.device_id = 0;
  param.batch_strategy = InferBatchStrategy::STATIC;
  param.batching_timeout = 300;
  param.priority = 0;
  param.show_stats = false;
  param.engine_num = 2;
  param.object_infer = false;

  {  // data is eos
    param.preproc_name = "VideoPreprocCpu";
    std::shared_ptr<InferHandler> infer_handler =
        std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor, nullptr, nullptr);
    ASSERT_TRUE(infer_handler->Open());
    bool is_eos = true;
    auto data = CreatData(std::to_string(param.device_id), is_eos);
    EXPECT_EQ(infer_handler->Process(data, param.object_infer), -1);
  }

  {  // preproc name = rcop
    if (!use_magicmind) {
      param.preproc_name = "RCOP";
      std::shared_ptr<InferHandler> infer_handler =
          std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor, nullptr, nullptr);
      ASSERT_TRUE(infer_handler->Open());
      bool is_eos = false;
      auto data = CreatData(std::to_string(param.device_id), is_eos);
      EXPECT_EQ(infer_handler->Process(data, param.object_infer), 0);
      infer_handler->WaitTaskDone(data->stream_id);
    } else {
      param.preproc_name = "CNCV";
      std::shared_ptr<InferHandler> infer_handler =
          std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor, nullptr, nullptr);
      ASSERT_TRUE(infer_handler->Open());
      bool is_eos = false;
      auto data = CreatData(std::to_string(param.device_id), is_eos);
      EXPECT_EQ(infer_handler->Process(data, param.object_infer), 0);
      infer_handler->WaitTaskDone(data->stream_id);
    }
  }

  {  // preproc name = SCALER
    if (!use_magicmind) {
      edk::MluContext ctx;
      edk::CoreVersion core_ver = ctx.GetCoreVersion();
      if (core_ver == edk::CoreVersion::MLU220) {
        param.preproc_name = "SCALER";
        std::shared_ptr<InferHandler> infer_handler =
            std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor, nullptr, nullptr);
        ASSERT_TRUE(infer_handler->Open());
        bool is_eos = false;
        auto data = CreatData(std::to_string(param.device_id), is_eos);
        EXPECT_EQ(infer_handler->Process(data, param.object_infer), 0);
        infer_handler->WaitTaskDone(data->stream_id);
      }
    }
  }

  {  // preproc name = usertype
    param.preproc_name = "VideoPreprocCpu";
    std::shared_ptr<InferHandler> infer_handler =
        std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor, nullptr, nullptr);
    ASSERT_TRUE(infer_handler->Open());
    bool is_eos = false;
    auto data = CreatData(std::to_string(param.device_id), is_eos);
    EXPECT_EQ(infer_handler->Process(data, param.object_infer), 0);
    infer_handler->WaitTaskDone(data->stream_id);
  }

  {  // frame filter
    if (use_magicmind) {
      param.preproc_name = "CNCV";
    } else {
      param.preproc_name = "RCOP";
    }
    bool is_eos = false;
    auto data_no_obj = CreatData(std::to_string(param.device_id), is_eos);

    auto data_has_obj = CreatData(std::to_string(param.device_id), is_eos);
    // make objs
    cnstream::CNObjsVec objs;
    std::shared_ptr<cnstream::CNInferObject> object = std::make_shared<cnstream::CNInferObject>();
    object->id = std::to_string(2);
    object->bbox.x = 0.2;
    object->bbox.y = 0.2;
    object->bbox.w = 0.3;
    object->bbox.h = 0.3;
    object->score = 0.8;
    objs.push_back(object);
    CNInferObjsPtr objs_holder = data_has_obj->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
    objs_holder->objs_.insert(objs_holder->objs_.end(), objs.begin(), objs.end());

    std::shared_ptr<InferHandler> infer_handler =
        std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor, frame_filter, nullptr);
    ASSERT_TRUE(infer_handler->Open());
    EXPECT_EQ(infer_handler->Process(data_no_obj, param.object_infer), 0);
    EXPECT_EQ(infer_handler->Process(data_has_obj, param.object_infer), 0);

    infer_handler->WaitTaskDone(data_no_obj->stream_id);
  }

  {  // object_infer = true, for secondary
    if (use_magicmind) {
      param.preproc_name = "CNCV";
    } else {
      param.preproc_name = "RCOP";
    }
    param.object_infer = true;
    bool is_eos = false;
    auto data = CreatData(std::to_string(param.device_id), is_eos);

    // make objs
    cnstream::CNObjsVec objs;
    std::shared_ptr<cnstream::CNInferObject> object = std::make_shared<cnstream::CNInferObject>();
    object->id = std::to_string(2);
    object->bbox.x = 0.2;
    object->bbox.y = 0.2;
    object->bbox.w = 0.3;
    object->bbox.h = 0.3;
    object->score = 0.8;
    objs.push_back(object);
    CNInferObjsPtr objs_holder = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
    objs_holder->objs_.insert(objs_holder->objs_.end(), objs.begin(), objs.end());

    std::shared_ptr<InferHandler> infer_handler =
        std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor, nullptr, obj_filter);
    ASSERT_TRUE(infer_handler->Open());
    EXPECT_EQ(infer_handler->Process(data, param.object_infer), 0);
    infer_handler->WaitTaskDone(data->stream_id);
  }
}

}  // namespace cnstream
