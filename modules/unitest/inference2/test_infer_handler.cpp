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
      model_path = "../../data/models/MLU220/Primary_Detector/YOLOv3/yolov3/yolov3_4c4b_argb_220_v1.5.0.cambricon";
      break;
    case edk::CoreVersion::MLU270:
    default:
      model_path = "../../data/models/MLU270/yolov3/yolov3_4c4b_argb_270_v1.5.0.cambricon";
      break;
  }
  return model_path;
}

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
    frame->ptr_mlu[0] = planes[0];
    frame->ptr_mlu[1] = planes[1];
    frame->ctx.dev_type = DevContext::DevType::MLU;
    frame->ctx.ddr_channel = std::stoi(device_id);
    frame->ctx.dev_id = std::stoi(device_id);
    frame->fmt = CN_PIXEL_FORMAT_YUV420_NV12;
    frame->dst_device_id = std::stoi(device_id);
    frame->frame_id = 1;
    data->timestamp = 1000;
    frame->width = width;
    frame->height = height;
    frame->stride[0] = frame->stride[1] = width;
    frame->CopyToSyncMem();
    data->datas[CNDataFramePtrKey] = frame;
    std::shared_ptr<CNInferObjs> objs(new (std::nothrow) CNInferObjs());
    data->datas[CNInferObjsPtrKey] = objs;
    return data;
  } else {
    frame->frame_id = 1;
    data->timestamp = 1000;
    frame->width = width;
    frame->height = height;
    frame->ptr_cpu[0] = image.data;
    frame->ptr_cpu[1] = image.data + nbytes * 2 / 3;
    frame->stride[0] = frame->stride[1] = width;
    frame->fmt = CN_PIXEL_FORMAT_YUV420_NV12;
    frame->ctx.dev_type = DevContext::DevType::CPU;
    frame->dst_device_id = std::stoi(device_id);
    frame->ctx.dev_id = std::stoi(device_id);
    frame->CopyToSyncMem();

    data->datas[CNDataFramePtrKey] = frame;
    std::shared_ptr<CNInferObjs> objs(new (std::nothrow) CNInferObjs());
    data->datas[CNInferObjsPtrKey] = objs;
    return data;
  }
  return nullptr;
}

TEST(Inferencer2, InferHandlerOpen) {
  std::string resnet_model_path = GetExePath() + GetModelPath();
  std::string infer_name = "detector";
  std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
  Inferencer2 *Infer2;
  Infer2 = infer.get();
  std::string preproc_name = "VideoPreprocCpu";
  std::string postproc_name = "VideoPostprocSsd";
  std::shared_ptr<VideoPreproc> pre_processor(VideoPreproc::Create(preproc_name));
  std::shared_ptr<VideoPostproc> post_processor(VideoPostproc::Create(postproc_name));

  Infer2Param param;
  param.model_path = resnet_model_path;
  param.device_id = 0;
  param.func_name = "subnet0";
  param.batch_strategy = InferBatchStrategy::STATIC;
  param.batching_timeout = 300;
  param.priority = 0;
  param.show_stats = false;
  param.engine_num = 2;
  param.object_infer = false;
  param.model_input_pixel_format = InferVideoPixelFmt::ARGB;

  {  // open sucess, preproc = VideoPreprocCpu
    std::shared_ptr<InferHandler> infer_handler =
        std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor);
    EXPECT_TRUE(infer_handler->Open());
  }

  {  // preproc_name = RCOP
    param.model_path = resnet_model_path;
    param.preproc_name = "RCOP";
    std::shared_ptr<InferHandler> infer_handler =
        std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor);
    EXPECT_TRUE(infer_handler->Open());
  }

  {  // preproc_name = SCALER
    edk::MluContext ctx;
    edk::CoreVersion core_ver = ctx.GetCoreVersion();
    if (core_ver == edk::CoreVersion::MLU220) {
      param.preproc_name = "SCALER";
      std::shared_ptr<InferHandler> infer_handler =
          std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor);
      EXPECT_TRUE(infer_handler->Open());
    }
  }
}

TEST(Inferencer2, InferHandlerProcess) {
  std::string resnet_model_path = GetExePath() + GetModelPath();
  std::string infer_name = "detector";
  std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
  Inferencer2 *Infer2;
  Infer2 = infer.get();
  std::string preproc_name = "VideoPreprocCpu";
  std::string postproc_name = "VideoPostprocSsd";
  std::shared_ptr<VideoPreproc> pre_processor(VideoPreproc::Create(preproc_name));
  std::shared_ptr<VideoPostproc> post_processor(VideoPostproc::Create(postproc_name));

  Infer2Param param;
  param.model_path = resnet_model_path;
  param.device_id = 0;
  param.func_name = "subnet0";
  param.batch_strategy = InferBatchStrategy::STATIC;
  param.batching_timeout = 300;
  param.priority = 0;
  param.show_stats = false;
  param.engine_num = 2;
  param.object_infer = false;
  param.model_input_pixel_format = InferVideoPixelFmt::ARGB;

  {  // data is eos
    std::shared_ptr<InferHandler> infer_handler =
        std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor);
    ASSERT_TRUE(infer_handler->Open());
    bool is_eos = true;
    auto data = CreatData(std::to_string(param.device_id), is_eos);
    EXPECT_EQ(infer_handler->Process(data, param.object_infer), -1);
  }

  {  // preproc name = rcop
    param.preproc_name = "RCOP";
    std::shared_ptr<InferHandler> infer_handler =
        std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor);
    ASSERT_TRUE(infer_handler->Open());
    bool is_eos = false;
    auto data = CreatData(std::to_string(param.device_id), is_eos);
    EXPECT_EQ(infer_handler->Process(data, param.object_infer), 0);
    infer_handler->WaitTaskDone(data->stream_id);
  }

  {  // preproc name = SCALER
    edk::MluContext ctx;
    edk::CoreVersion core_ver = ctx.GetCoreVersion();
    if (core_ver == edk::CoreVersion::MLU220) {
      param.preproc_name = "SCALER";
      std::shared_ptr<InferHandler> infer_handler =
          std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor);
      ASSERT_TRUE(infer_handler->Open());
      bool is_eos = false;
      auto data = CreatData(std::to_string(param.device_id), is_eos);
      EXPECT_EQ(infer_handler->Process(data, param.object_infer), 0);
      infer_handler->WaitTaskDone(data->stream_id);
    }
  }

  {  // preproc name = usertype
    param.preproc_name = "VideoPreprocCpu";
    std::shared_ptr<InferHandler> infer_handler =
        std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor);
    ASSERT_TRUE(infer_handler->Open());
    bool is_eos = false;
    auto data = CreatData(std::to_string(param.device_id), is_eos);
    EXPECT_EQ(infer_handler->Process(data, param.object_infer), 0);
    infer_handler->WaitTaskDone(data->stream_id);
  }

  {  // object_infer = true, for secondary
    param.preproc_name = "RCOP";
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
    CNInferObjsPtr objs_holder = GetCNInferObjsPtr(data);
    objs_holder->objs_.insert(objs_holder->objs_.end(), objs.begin(), objs.end());

    std::shared_ptr<InferHandler> infer_handler =
        std::make_shared<InferHandlerImpl>(Infer2, param, post_processor, pre_processor);
    ASSERT_TRUE(infer_handler->Open());
    EXPECT_EQ(infer_handler->Process(data, param.object_infer), 0);
    infer_handler->WaitTaskDone(data->stream_id);
  }
}

}  // namespace cnstream
