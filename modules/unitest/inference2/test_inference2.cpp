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

#include "cnstream_logging.hpp"

#include "inferencer2.hpp"
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

static std::string GetModelPathMM() { return "../../data/models/yolov3_v1.1.0_4b_rgb_uint8.magicmind"; }

// the data is related to model
extern cnstream::CNFrameInfoPtr CreatInferTestData(std::string device_id, bool is_eos = false, bool mlu_data = true);

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

  {  // preproc_name is empty
    std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
    ModuleParamSet param;
    if (use_magicmind) {
      param["model_path"] = exe_path + GetModelPathMM();
    } else {
      param["model_path"] = exe_path + GetModelPath();
    }
    param["preproc_name"] = "";
    param["postproc_name"] = "VideoPostprocSsd";
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

  {  // frame_filter_name is empty
    std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
    ModuleParamSet param;
    if (use_magicmind) {
      param["model_path"] = exe_path + GetModelPathMM();
      param["preproc_name"] = "CNCV";
    } else {
      param["model_path"] = exe_path + GetModelPath();
      param["preproc_name"] = "RCOP";
    }
    param["postproc_name"] = "VideoPostprocSsd";
    param["frame_filter_name"] = "no_such_frame_filter_name";
    EXPECT_FALSE(infer->Open(param));
  }

  {  // obj_filter_name is empty
    std::unique_ptr<Inferencer2> infer(new Inferencer2(infer_name));
    ModuleParamSet param;
    if (use_magicmind) {
      param["model_path"] = exe_path + GetModelPathMM();
      param["preproc_name"] = "CNCV";
    } else {
      param["model_path"] = exe_path + GetModelPath();
      param["preproc_name"] = "RCOP";
    }
    param["postproc_name"] = "VideoPostprocSsd";
    param["obj_filter_name"] = "no_such_obj_filter_name";
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
    auto data = CreatInferTestData(device_id, is_eos);
    EXPECT_EQ(infer->Process(data), 0);
  }

  {  // CNSyncedMemory data is on different MLU from the data this module needed, and SOURCE data is on MLU
    if (edk::MluContext::GetDeviceNum() >= 2) {
      ASSERT_TRUE(infer->Open(param));
      std::string device_id = "1";
      bool is_eos = false;
      bool mlu_data = true;
      auto data = CreatInferTestData(device_id, is_eos, mlu_data);
      EXPECT_EQ(infer->Process(data), 0);
    }
  }

  {  // CNSyncedMemory data is on different MLU from the data this module needed, and SOURCE data is on CPU
    ASSERT_TRUE(infer->Open(param));
    std::string device_id = "1";
    bool is_eos = false;
    bool mlu_data = false;
    auto data = CreatInferTestData(device_id, is_eos, mlu_data);
    EXPECT_EQ(infer->Process(data), 0);
  }

  {  // CNFrameInfo no eos and data in mlu
    ASSERT_TRUE(infer->Open(param));
    std::string device_id = param["device_id"];
    bool is_eos = false;
    bool mlu_data = true;
    auto data = CreatInferTestData(device_id, is_eos, mlu_data);
    EXPECT_EQ(infer->Process(data), 0);
  }

  {  // CNFrameInfo no eos and data in cpu
    ASSERT_TRUE(infer->Open(param));
    std::string device_id = param["device_id"];
    bool is_eos = false;
    bool mlu_data = false;
    auto data = CreatInferTestData(device_id, is_eos, mlu_data);
    EXPECT_EQ(infer->Process(data), 0);
  }
  EXPECT_NO_THROW(infer->Close());
}

}  // namespace cnstream
