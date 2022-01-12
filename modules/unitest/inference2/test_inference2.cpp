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

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "easyinfer/mlu_memory_op.h"
#include "cnstream_logging.hpp"

#include "inferencer2.hpp"
#include "postproc.hpp"
#include "video_preproc.hpp"
#include "test_base.hpp"

namespace cnstream {

class FakeVideoPostproc : public cnstream::VideoPostproc {
 public:
  bool Execute(infer_server::InferData* result, const infer_server::ModelIO& output,
               const infer_server::ModelInfo* model) override {
    return true;
  }

 private:
  DECLARE_REFLEX_OBJECT_EX(FakeVideoPostproc, cnstream::VideoPostproc);
};  // class FakeVideoPostproc

IMPLEMENT_REFLEX_OBJECT_EX(FakeVideoPostproc, cnstream::VideoPostproc);

class FakeVideoPreproc : public cnstream::VideoPreproc {
 public:
  bool Execute(infer_server::ModelIO* model_input, const infer_server::InferData& input_data,
               const infer_server::ModelInfo* model_info) {
    return true;
  }

 private:
  DECLARE_REFLEX_OBJECT_EX(FakeVideoPreproc, cnstream::VideoPreproc);
};  // class FakeVideoPreproc

IMPLEMENT_REFLEX_OBJECT_EX(FakeVideoPreproc, cnstream::VideoPreproc);

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

static std::string GetModelPathMM() { return "../../data/models/yolov3_nhwc.model"; }

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
    void* frame_data = image.data;
    void* planes[CN_MAX_PLANES] = {nullptr, nullptr};
    edk::MluMemoryOp mem_op;
    frame_data = mem_op.AllocMlu(nbytes);
    planes[0] = frame_data;                                                                       // y plane
    planes[1] = reinterpret_cast<void*>(reinterpret_cast<int64_t>(frame_data) + width * height);  // uv plane
    void* ptr_mlu[2] = {planes[0], planes[1]};
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
    void* ptr_cpu[2] = {image.data, image.data + nbytes * 2 / 3};
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

TEST(Inferencer2, Open) {
  bool use_magicmind = infer_server::Predictor::Backend() == "magicmind";
  std::string exe_path = GetExePath();
  std::string infer_name = "detector";

  {  // open success but param is lack
    std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
    ModuleParamSet param;
    if (use_magicmind) {
      param["model_path"] = exe_path + GetModelPathMM();
      param["model_input_pixel_format"] = "RGB24";
    } else {
      param["model_path"] = exe_path + GetModelPath();
      param["model_input_pixel_format"] = "ARGB32";
    }
    param["preproc_name"] = "VideoPreprocCpu";
    param["postproc_name"] = "VideoPostprocSsd";
    EXPECT_TRUE(infer->Open(param));
  }

  {  // param is empty
    std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
    ModuleParamSet param;
    EXPECT_FALSE(infer->Open(param));
  }

  {  // param is no registered.
    std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
    ModuleParamSet param;
    if (use_magicmind) {
      param["model_path"] = exe_path + GetModelPathMM();
    } else {
      param["model_path"] = exe_path + GetModelPath();
    }
    param["preproc_name"] = "VideoPreprocCpu";
    param["postproc_name"] = "VideoPostprocSsd";
    param["no_such_key"] = "key";
    EXPECT_FALSE(infer->Open(param));
  }

  {  // preproc_name is error
    std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
    ModuleParamSet param;
    if (use_magicmind) {
      param["model_path"] = exe_path + GetModelPathMM();
    } else {
      param["model_path"] = exe_path + GetModelPath();
    }
    param["preproc_name"] = "no_such_preproc_class";
    param["postproc_name"] = "VideoPostprocSsd";
    EXPECT_FALSE(infer->Open(param));
  }

  {  // postproc_name is error
    std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
    ModuleParamSet param;
    if (use_magicmind) {
      param["model_path"] = exe_path + GetModelPathMM();
      param["preproc_name"] = "CNCV";
    } else {
      param["model_path"] = exe_path + GetModelPath();
      param["preproc_name"] = "RCOP";
    }
    param["postproc_name"] = "no_such_postproc_name";
    EXPECT_FALSE(infer->Open(param));
  }

  {  // postproc_name is empty
    std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
    ModuleParamSet param;
    if (use_magicmind) {
      param["model_path"] = exe_path + GetModelPathMM();
      param["preproc_name"] = "CNCV";
    } else {
      param["model_path"] = exe_path + GetModelPath();
      param["preproc_name"] = "RCOP";
    }
    param["postproc_name"] = "";
    EXPECT_FALSE(infer->Open(param));
  }

  {  // model_path is error
    std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
    ModuleParamSet param;
    if (use_magicmind) {
      param["model_path"] = "/home/no.model";
      param["preproc_name"] = "CNCV";
    } else {
      param["model_path"] = "/home/error_path";
      param["preproc_name"] = "RCOP";
    }
    param["postproc_name"] = "VideoPostprocSsd";
    EXPECT_FALSE(infer->Open(param));  // check model path in inference, infer_handler no check
  }
}

TEST(Inferencer2, Process) {
  bool use_magicmind = infer_server::Predictor::Backend() == "magicmind";
  std::string exe_path = GetExePath();
  std::string infer_name = "detector";
  std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
  ModuleParamSet param;
  if (use_magicmind) {
    param["model_path"] = exe_path + GetModelPathMM();
    param["model_input_pixel_format"] = "RGB24";
  } else {
    param["model_path"] = exe_path + GetModelPath();
    param["model_input_pixel_format"] = "ARGB32";
  }
  param["preproc_name"] = "FakeVideoPreproc";
  param["postproc_name"] = "FakeVideoPostproc";
  param["device_id"] = "0";

  {  // CNFrameInfo empty
    ASSERT_TRUE(infer->Open(param));
    std::shared_ptr<CNFrameInfo> data = nullptr;
    EXPECT_EQ(infer->Process(data), -1);
  }

  {  // CNFrameInfo is eos
    ASSERT_TRUE(infer->Open(param));
    std::string device_id = param["device_id"];
    bool is_eos = true;
    auto data = CreatData(device_id, is_eos);
    EXPECT_EQ(infer->Process(data), 0);
  }

  {  // CNFrameInfo no eos and data in mlu
    ASSERT_TRUE(infer->Open(param));
    std::string device_id = param["device_id"];
    bool is_eos = false;
    bool mlu_data = true;
    auto data = CreatData(device_id, is_eos, mlu_data);
    EXPECT_EQ(infer->Process(data), 0);
  }

  {  // CNFrameInfo no eos and data in cpu
    ASSERT_TRUE(infer->Open(param));
    std::string device_id = param["device_id"];
    bool is_eos = false;
    bool mlu_data = false;
    auto data = CreatData(device_id, is_eos, mlu_data);
    EXPECT_EQ(infer->Process(data), 0);
  }
  EXPECT_NO_THROW(infer->Close());
}

}  // namespace cnstream
