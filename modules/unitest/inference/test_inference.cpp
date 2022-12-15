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
#include <vector>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "cnstream_logging.hpp"

#include "cnstream_postproc.hpp"
#include "cnstream_preproc.hpp"
#include "inferencer.hpp"
#include "test_base.hpp"

static constexpr const char *glabel_path = "../../modules/unitest/data/test_empty_label.txt";

namespace cnstream {

class FakeVideoPostproc : public cnstream::Postproc {
 public:
  int Execute(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
              const std::vector<cnstream::CNFrameInfoPtr>& packages,
              const cnstream::LabelStrings& labels) override {
    return 0;
  }

 private:
  DECLARE_REFLEX_OBJECT_EX(FakeVideoPostproc, cnstream::Postproc);
};  // class FakeVideoPostproc

IMPLEMENT_REFLEX_OBJECT_EX(FakeVideoPostproc, cnstream::Postproc);

class FakeVideoPreproc : public cnstream::Preproc {
 public:
  int Execute(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
              const std::vector<CnedkTransformRect> &src_rects) override {
    return 0;
  }

  int OnTensorParams(const infer_server::CnPreprocTensorParams *params) override {
    return 0;
  }

 private:
  DECLARE_REFLEX_OBJECT_EX(FakeVideoPreproc, cnstream::Preproc);
};  // class FakeVideoPreproc

IMPLEMENT_REFLEX_OBJECT_EX(FakeVideoPreproc, cnstream::Preproc);


class FakeVideoFilter : public cnstream::ObjectFilterVideoCategory {
 public:
  bool Filter(const cnstream::CNFrameInfoPtr package, const cnstream::CNInferObjectPtr object) override {
    return true;
  }

  DECLARE_REFLEX_OBJECT_EX(FakeVideoFilter, cnstream::ObjectFilterVideo);
};  // class FakeVideoFilter

IMPLEMENT_REFLEX_OBJECT_EX(FakeVideoFilter, cnstream::ObjectFilterVideo);

static std::string GetModelPath() { return "../../data/models/" + GetModelInfoStr("yolov3", "name"); }

// the data is related to model
static cnstream::CNFrameInfoPtr CreatData(int device_id, bool is_eos = false, bool mlu_data = true) {
  auto data = cnstream::CNFrameInfo::Create(std::to_string(0), is_eos);
  data->SetStreamIndex(0);
  data->timestamp = 1000;
  data->stream_id = "1";

  cv::Mat image = cv::imread(GetExePath() + "../../data/images/0.jpg");

  std::shared_ptr<CNDataFrame> frame = GenerateCNDataFrame(image, device_id);

  std::shared_ptr<CNInferObjs> objs(new (std::nothrow) CNInferObjs());
  data->collection.Add(kCNDataFrameTag, frame);
  data->collection.Add(kCNInferObjsTag, objs);
  return data;
}

TEST(Inferencer, Open) {
  std::string exe_path = GetExePath();
  std::string infer_name = "detector";

  {  // open success but param is lack
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    param["device_id"] = "0";
    param["model_path"] = exe_path + GetModelPath();
    param["model_input_pixel_format"] = "RGB24";
    param["preproc"] = "name=PreprocYolov3";
    param["postproc"] = "name=PostprocSSDLpd";
    EXPECT_TRUE(infer->Open(param));
  }

  {  // param is empty
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    EXPECT_FALSE(infer->Open(param));
  }

  {  // param is no registered.
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    param["device_id"] = "0";
    param["model_path"] = exe_path + GetModelPath();
    param["preproc"] = "name=PreprocYolov3;use_cpu=false";
    param["postproc"] = "name=PostprocYolov3";
    param["no_such_key"] = "key";
    EXPECT_FALSE(infer->Open(param));
  }

  {  // preproc is error
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    std::cout << "preproc is error" << std::endl;
    param["model_path"] = exe_path + GetModelPath();
    param["preproc"] = "name=no_such_preproc_class;use_cpu=false";
    param["postproc"] = "name=PostprocSSDLpd";
    EXPECT_FALSE(infer->Open(param));
  }

  {  // postproc is error
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    param["device_id"] = "0";
    param["model_path"] = exe_path + GetModelPath();
    param["preproc"] = "name=PreprocYolov3;use_cpu=false";
    param["postproc"] = "name=no_such_postproc";
    EXPECT_FALSE(infer->Open(param));
  }

  {  // preproc is empty
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    param["device_id"] = "0";
    param["model_path"] = exe_path + GetModelPath();
    param["preproc"] = "";
    param["postproc"] = "name=PostprocSSDLpd";
    EXPECT_FALSE(infer->Open(param));
  }

  {  // postproc is empty
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    param["device_id"] = "0";
    param["model_path"] = exe_path + GetModelPath();
    param["preproc"] = "name=PreprocYolov3;use_cpu=false";
    param["postproc"] = "";
    EXPECT_FALSE(infer->Open(param));
  }

  {  // frame_filter_name is empty
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    param["device_id"] = "0";
    param["model_path"] = exe_path + GetModelPath();
    param["postproc"] = "name=PostprocSSDLpd";
    param["filter"] = "name=no_such_frame_filter_name";
    EXPECT_FALSE(infer->Open(param));
  }

  {  // model_path is error
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    param["device_id"] = "0";
    param["model_path"] = "/home/no.model";
    param["preproc"] = "name=PreprocYolov3;use_cpu=false";
    param["postproc"] = "name=PostprocSSDLpd;threshold=0.6";
    EXPECT_FALSE(infer->Open(param));  // check model path in inference, infer_handler no check
  }

  {  // filter, only filter
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    param["device_id"] = "0";
    param["model_path"] = exe_path + GetModelPath();
    param["preproc"] = "name=PreprocYolov3;use_cpu=false";
    param["postproc"] = "name=PostprocSSDLpd";
    param["filter"] = "name=FakeVideoFilter";
    EXPECT_TRUE(infer->Open(param));
  }

  {  // filter, only categroites
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    param["device_id"] = "0";
    param["model_path"] = exe_path + GetModelPath();
    param["preproc"] = "name=PreprocYolov3;use_cpu=false";
    param["postproc"] = "name=PostprocSSDLpd";
    param["filter"] = "categroies=2";
    EXPECT_TRUE(infer->Open(param));
  }

  {  // filter, only categroites
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    param["device_id"] = "0";
    param["model_path"] = exe_path + GetModelPath();
    param["preproc"] = "name=PreprocYolov3;use_cpu=false";
    param["postproc"] = "name=PostprocSSDLpd";
    param["filter"] = "categroies=-1";
    EXPECT_TRUE(infer->Open(param));
  }

  {  // filter
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    param["device_id"] = "0";
    param["model_path"] = exe_path + GetModelPath();
    param["preproc"] = "name=PreprocYolov3;use_cpu=false";
    param["postproc"] = "name=PostprocSSDLpd";
    param["filter"] = "name=FakeVideoFilter;categroies=2";
    EXPECT_TRUE(infer->Open(param));
  }

  {  // label good
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    param["device_id"] = "0";
    param["model_path"] = exe_path + GetModelPath();
    param["label_path"] = GetExePath() + "../../data/models/label_map_coco.txt";
    param["preproc"] = "name=PreprocYolov3;use_cpu=false";
    param["postproc"] = "name=PostprocSSDLpd";

    EXPECT_TRUE(infer->Open(param));
  }

  {  // label bad
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    param["device_id"] = "0";
    param["model_path"] = exe_path + GetModelPath();
    param["label_path"] = "/fake/path";
    param["preproc"] = "name=PreprocYolov3;use_cpu=false";
    param["postproc"] = "name=PostprocSSDLpd";

    EXPECT_FALSE(infer->Open(param));
  }

  {  // empty label
    std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
    ModuleParamSet param;
    param["device_id"] = "0";
    param["model_path"] = exe_path + GetModelPath();
    param["label_path"] = GetExePath() + glabel_path;
    param["preproc"] = "name=PreprocYolov3;use_cpu=false";
    param["postproc"] = "name=PostprocSSDLpd";

    EXPECT_FALSE(infer->Open(param));
  }
}

TEST(Inferencer, Process) {
  std::string exe_path = GetExePath();
  std::string infer_name = "detector";
  std::unique_ptr<Inferencer> infer(new Inferencer(infer_name));
  ModuleParamSet param;

  param["model_path"] = exe_path + GetModelPath();
  param["preproc"] = "name=FakeVideoPreproc;use_cpu=false";
  param["postproc"] = "name=FakeVideoPostproc;threshold=0.6";
  param["device_id"] = "0";
  ASSERT_TRUE(infer->Open(param));

  {  // CNFrameInfo empty
    std::shared_ptr<CNFrameInfo> data = nullptr;
    EXPECT_EQ(infer->Process(data), -1);
  }

  {  // CNFrameInfo is eos
    int device_id = std::stoi(param["device_id"]);
    bool is_eos = true;
    auto data = CreatData(device_id, is_eos);
    EXPECT_EQ(infer->Process(data), 0);
  }

  {
    int device_id = 0;
    bool is_eos = false;
    bool mlu_data = true;
    auto data = CreatData(device_id, is_eos, mlu_data);
    EXPECT_EQ(infer->Process(data), 0);
  }
  EXPECT_NO_THROW(infer->Close());

  {  // interval
    param["interval"] = "2";
    ASSERT_TRUE(infer->Open(param));
    int device_id = 0;
    bool is_eos = false;
    bool mlu_data = true;
    for (int i = 0; i < 5; ++i) {
      auto data = CreatData(device_id, is_eos, mlu_data);
      EXPECT_EQ(infer->Process(data), 0);
    }
    is_eos = true;
    auto data = CreatData(device_id, is_eos);
    EXPECT_EQ(infer->Process(data), 0);
  }

  {  // CNFrameInfo no eos and data in mlu
    int device_id = std::stoi(param["device_id"]);

    bool is_eos = false;
    bool mlu_data = true;
    auto data = CreatData(device_id, is_eos, mlu_data);
    EXPECT_EQ(infer->Process(data), 0);
  }
  EXPECT_NO_THROW(infer->Close());
}

}  // namespace cnstream
