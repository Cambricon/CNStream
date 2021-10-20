/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
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

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "device/mlu_context.h"
#include "easyinfer/mlu_memory_op.h"
#include "easyinfer/model_loader.h"

#include "cnstream_frame_va.hpp"
#include "inferencer.hpp"
#include "obj_filter.hpp"
#include "postproc.hpp"
#include "preproc.hpp"
#include "test_base.hpp"

namespace cnstream {

static bool gcalled_execute_v1 = false;
static bool gcalled_execute_v2 = false;
static std::atomic<bool> gpostproc_done {false};

static void ResetGlobal() {
  gcalled_execute_v1 = false;
  gcalled_execute_v2 = false;
  gpostproc_done.store(false);
}

std::string gTestPerfDir = "./test_perf_tmp/";  // NOLINT

class FakePostproc : public Postproc, virtual public ReflexObjectEx<Postproc> {
 public:
  int Execute(const std::vector<float *> &net_outputs, const std::shared_ptr<edk::ModelLoader> &model,
              const CNFrameInfoPtr &package) override {
    gcalled_execute_v1 = true;
    gpostproc_done.store(true);
    return 0;
  }

  int Execute(const std::vector<void*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const std::vector<CNFrameInfoPtr> &packages) override {
    gcalled_execute_v2 = true;
    gpostproc_done.store(true);
    return 0;
  }

  DECLARE_REFLEX_OBJECT_EX(FakePostproc, Postproc);
};  // class FakePostproc

IMPLEMENT_REFLEX_OBJECT_EX(FakePostproc, Postproc);

class FakePreproc : public Preproc, virtual public ReflexObjectEx<Preproc> {
 public:
  int Execute(const std::vector<float *> &net_inputs, const std::shared_ptr<edk::ModelLoader> &model,
              const CNFrameInfoPtr &package) override {
    return 0;
  }

  DECLARE_REFLEX_OBJECT_EX(FakePreproc, Preproc);
};  // class FakePreproc

IMPLEMENT_REFLEX_OBJECT_EX(FakePreproc, Preproc);

class FakeObjPostproc : public ObjPostproc {
 public:
  int Execute(const std::vector<float *> &net_outputs, const std::shared_ptr<edk::ModelLoader> &model,
              const CNFrameInfoPtr &package, const CNInferObjectPtr &obj) override {
    gcalled_execute_v1 = true;
    gpostproc_done.store(true);
    return 0;
  }

  int Execute(const std::vector<void*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const std::vector<std::pair<CNFrameInfoPtr, std::shared_ptr<CNInferObject>>> &obj_infos) override {
    gcalled_execute_v2 = true;
    gpostproc_done.store(true);
    return 0;
  }

  DECLARE_REFLEX_OBJECT_EX(FakeObjPostproc, ObjPostproc);
};  // class FakeObjPostproc

IMPLEMENT_REFLEX_OBJECT_EX(FakeObjPostproc, ObjPostproc);

class FakeObjPreproc : public ObjPreproc {
 public:
  int Execute(const std::vector<float *> &net_inputs, const std::shared_ptr<edk::ModelLoader> &model,
              const CNFrameInfoPtr &package, const CNInferObjectPtr &obj) override {
    return 0;
  }

  DECLARE_REFLEX_OBJECT_EX(FakeObjPreproc, ObjPreproc);
};  // class FakeObjPreproc

IMPLEMENT_REFLEX_OBJECT_EX(FakeObjPreproc, ObjPreproc);

class FakeObjFilter : public ObjFilter {
 public:
  bool Filter(const CNFrameInfoPtr &finfo, const CNInferObjectPtr &obj) override { return true; }

  DECLARE_REFLEX_OBJECT_EX(FakeObjFilter, ObjFilter);
};  // class FakeObjFilter

IMPLEMENT_REFLEX_OBJECT_EX(FakeObjFilter, ObjFilter);

static const char *name = "test-infer";
static const char *g_image_path = "../../data/images/3.jpg";
static const char *g_func_name = "subnet0";
static const char *g_postproc_name = "FakePostproc";

static constexpr int g_dev_id = 0;
static constexpr int g_channel_id = 0;

static std::string GetModelPath() {
  edk::MluContext ctx;
  edk::CoreVersion core_ver = ctx.GetCoreVersion();
  std::string model_path = "";
  switch (core_ver) {
    case edk::CoreVersion::MLU220:
      model_path = "../../data/models/resnet18_b4c4_bgra_mlu220.cambricon";
      break;
    case edk::CoreVersion::MLU270:
    default:
      model_path = "../../data/models/resnet50_b16c16_bgra_mlu270.cambricon";
      break;
  }
  return model_path;
}

TEST(Inferencer, Construct) {
  std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
  EXPECT_STREQ(infer->GetName().c_str(), name);
}

TEST(Inferencer, CheckParamSet) {
  std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
  ModuleParamSet param;

  // the type of batching_timeout is uint32_t
  param["batching_timeout"] = std::to_string(std::numeric_limits<uint64_t>::max());
  EXPECT_FALSE(infer->CheckParamSet(param));
  param.clear();

  param["model_path"] = GetExePath() + GetModelPath();
  param["func_name"] = g_func_name;
  param["postproc_name"] = g_postproc_name;
  EXPECT_TRUE(infer->CheckParamSet(param));

  param["batching_timeout"] = "30";
  param["threshold"] = "0.3";
  param["device_id"] = "fake_value";
  EXPECT_FALSE(infer->CheckParamSet(param));

  param["device_id"] = "0";
  EXPECT_TRUE(infer->CheckParamSet(param));

  param["data_order"] = "NCHW";
  param["infer_interval"] = "1";
  param["threshold"] = "0.3";
  EXPECT_TRUE(infer->CheckParamSet(param));
  EXPECT_TRUE(infer->Open(param));
}

TEST(Inferencer, Open) {
  std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
  ModuleParamSet param;
  EXPECT_FALSE(infer->Open(param));
  param["model_path"] = "test-infer";
  param["func_name"] = g_func_name;
  EXPECT_FALSE(infer->Open(param));

  param["model_path"] = GetExePath() + GetModelPath();
  param["func_name"] = g_func_name;
  param["device_id"] = std::to_string(g_dev_id);

  param["postproc_name"] = "test-postproc-name";
  EXPECT_FALSE(infer->Open(param));

  param["postproc_name"] = g_postproc_name;
  EXPECT_TRUE(infer->Open(param));

  param["use_scaler"] = "true";
  EXPECT_TRUE(infer->Open(param));

  param["preproc_name"] = "test-preproc-name";
  EXPECT_FALSE(infer->Open(param));

  infer->Close();
}

TEST(Inferencer, ProcessFrame) {
  std::string model_path = GetExePath() + GetModelPath();
  std::string image_path = GetExePath() + g_image_path;

  // test with MLU preproc (resize & convert)
  {
    std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
    ModuleParamSet param;
    param["model_path"] = model_path;
    param["func_name"] = g_func_name;
    param["postproc_name"] = g_postproc_name;
    param["device_id"] = std::to_string(g_dev_id);
    param["batching_timeout"] = "30";
    ASSERT_TRUE(infer->Open(param));

    const int width = 1280, height = 720;
    size_t nbytes = width * height * sizeof(uint8_t) * 3;
    size_t boundary = 1 << 16;
    nbytes = (nbytes + boundary - 1) & ~(boundary - 1);  // align to 64kb

    // fake data
    void *frame_data = nullptr;
    void *planes[CN_MAX_PLANES] = {nullptr, nullptr};
    edk::MluMemoryOp mem_op;
    frame_data = mem_op.AllocMlu(nbytes);
    planes[0] = frame_data;                                                                        // y plane
    planes[1] = reinterpret_cast<void *>(reinterpret_cast<int64_t>(frame_data) + width * height);  // uv plane

    // test nv12
    {
      auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
      std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
      frame->frame_id = 1;
      data->timestamp = 1000;
      frame->width = width;
      frame->height = height;
      void *ptr_mlu[2] = {planes[0], planes[1]};
      frame->stride[0] = frame->stride[1] = width;
      frame->ctx.ddr_channel = g_channel_id;
      frame->ctx.dev_id = g_dev_id;
      frame->ctx.dev_type = DevContext::DevType::MLU;
      frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12;
      frame->dst_device_id = g_dev_id;
      frame->CopyToSyncMem(ptr_mlu, true);
      data->collection.Add(kCNDataFrameTag, frame);
      int ret = infer->Process(data);
      EXPECT_EQ(ret, 1);
      // create eos frame for clearing stream idx
      cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);
    }

    ASSERT_NO_THROW(infer->Close());
    ASSERT_TRUE(infer->Open(param));

    // test nv21
    {
      auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
      std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
      frame->frame_id = 1;
      data->timestamp = 1000;
      frame->width = width;
      frame->height = height;
      void *ptr_mlu[2] = {planes[0], planes[1]};
      frame->stride[0] = frame->stride[1] = width;
      frame->ctx.ddr_channel = g_channel_id;
      frame->ctx.dev_id = g_dev_id;
      frame->ctx.dev_type = DevContext::DevType::MLU;
      frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
      frame->dst_device_id = g_dev_id;
      frame->CopyToSyncMem(ptr_mlu, true);
      data->collection.Add(kCNDataFrameTag, frame);
      int ret = infer->Process(data);
      EXPECT_EQ(ret, 1);
      // create eos frame for clearing stream idx
      cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);
    }

    ASSERT_NO_THROW(infer->Close());
    param["keep_aspect_ratio"] = "true";
    ASSERT_TRUE(infer->Open(param));

    // test nv21 with keep_aspect_ratio
    {
      auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
      std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
      frame->frame_id = 1;
      data->timestamp = 1000;
      frame->width = width;
      frame->height = height;
      void *ptr_mlu[2] = {planes[0], planes[1]};
      frame->stride[0] = frame->stride[1] = width;
      frame->ctx.ddr_channel = g_channel_id;
      frame->ctx.dev_id = g_dev_id;
      frame->ctx.dev_type = DevContext::DevType::MLU;
      frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
      frame->dst_device_id = g_dev_id;
      frame->CopyToSyncMem(ptr_mlu, true);
      data->collection.Add(kCNDataFrameTag, frame);
      int ret = infer->Process(data);
      EXPECT_EQ(ret, 1);
      // create eos frame for clearing stream idx
      cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);
    }

    ASSERT_NO_THROW(infer->Close());
    mem_op.FreeMlu(frame_data);
  }

  // test with CPU preproc
  {
    std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
    ModuleParamSet param;
    param["model_path"] = model_path;
    param["func_name"] = g_func_name;
    param["preproc_name"] = "FakePreproc";
    param["postproc_name"] = g_postproc_name;
    param["device_id"] = std::to_string(g_dev_id);
    param["batching_timeout"] = "30";
    ASSERT_TRUE(infer->Open(param));

    const int width = 1920, height = 1080;
    size_t nbytes = width * height * sizeof(uint8_t) * 3 / 2;
    uint8_t *frame_data = new uint8_t[nbytes];

    auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
    std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
    frame->frame_id = 1;
    data->timestamp = 1000;
    frame->width = width;
    frame->height = height;
    void *ptr_cpu[2];
    ptr_cpu[0] = frame_data;
    ptr_cpu[1] = frame_data + nbytes * 2 / 3;
    frame->stride[0] = frame->stride[1] = width;
    frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
    frame->ctx.dev_type = DevContext::DevType::CPU;
    frame->dst_device_id = g_dev_id;
    frame->CopyToSyncMem(ptr_cpu, true);
    data->collection.Add(kCNDataFrameTag, frame);

    int ret = infer->Process(data);
    EXPECT_EQ(ret, 1);
    delete[] frame_data;
    // create eos frame for clearing stream idx
    cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);
    ASSERT_NO_THROW(infer->Close());
  }

  // test mem_on_mlu_for_postproc
  {
    std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
    ModuleParamSet param;
    param["model_path"] = model_path;
    param["func_name"] = g_func_name;
    param["preproc_name"] = "FakePreproc";
    param["postproc_name"] = g_postproc_name;
    param["mem_on_mlu_for_postproc"] = "true";
    param["device_id"] = std::to_string(g_dev_id);
    param["batching_timeout"] = "30";
    ASSERT_TRUE(infer->Open(param));

    const int width = 1920, height = 1080;
    size_t nbytes = width * height * sizeof(uint8_t) * 3 / 2;
    uint8_t *frame_data = new uint8_t[nbytes];

    auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
    std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
    frame->frame_id = 1;
    data->timestamp = 1000;
    frame->width = width;
    frame->height = height;
    void *ptr_cpu[2] = {frame_data, frame_data + nbytes * 2 / 3};
    frame->stride[0] = frame->stride[1] = width;
    frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
    frame->ctx.dev_type = DevContext::DevType::CPU;
    frame->dst_device_id = g_dev_id;
    frame->CopyToSyncMem(ptr_cpu, true);
    data->collection.Add(kCNDataFrameTag, frame);

    ResetGlobal();

    int ret = infer->Process(data);
    EXPECT_EQ(ret, 1);

    while (!gpostproc_done.load()) usleep(20 * 1000);

    EXPECT_TRUE(gcalled_execute_v2);

    // create eos frame for clearing stream idx
    cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);
    ASSERT_NO_THROW(infer->Close());

    param["mem_on_mlu_for_postproc"] = "false";
    ASSERT_TRUE(infer->Open(param));

    ResetGlobal();

    ret = infer->Process(data);
    EXPECT_EQ(ret, 1);

    while (!gpostproc_done.load()) usleep(20 * 1000);

    EXPECT_TRUE(gcalled_execute_v1);

    // create eos frame for clearing stream idx
    cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);
    ASSERT_NO_THROW(infer->Close());

    delete[] frame_data;
  }
}

TEST(Inferencer, ProcessObject) {
  std::string model_path = GetExePath() + GetModelPath();
  std::string image_path = GetExePath() + g_image_path;

  auto obj = std::make_shared<CNInferObject>();
  obj->id = "1";
  obj->score = 0.8;
  obj->bbox.x = 0.1;
  obj->bbox.y = 0.1;
  obj->bbox.w = 0.3;
  obj->bbox.h = 0.3;

  // test with MLU preproc (resize & convert)
  {
    std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
    ModuleParamSet param;
    param["model_path"] = model_path;
    param["func_name"] = g_func_name;
    param["postproc_name"] = "FakeObjPostproc";
    param["device_id"] = std::to_string(g_dev_id);
    param["batching_timeout"] = "30";
    param["object_infer"] = "true";
    param["obj_filter_name"] = "FakeObjFilter";
    ASSERT_TRUE(infer->Open(param));

    const int width = 1280, height = 720;
    size_t nbytes = width * height * sizeof(uint8_t) * 3;
    size_t boundary = 1 << 16;
    nbytes = (nbytes + boundary - 1) & ~(boundary - 1);  // align to 64kb

    // fake data
    void *frame_data = nullptr;
    void *planes[CN_MAX_PLANES] = {nullptr, nullptr};
    edk::MluMemoryOp mem_op;
    frame_data = mem_op.AllocMlu(nbytes);
    planes[0] = frame_data;                                                                        // y plane
    planes[1] = reinterpret_cast<void *>(reinterpret_cast<int64_t>(frame_data) + width * height);  // uv plane

    // test nv12
    {
      auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
      std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
      frame->frame_id = 1;
      data->timestamp = 1000;
      frame->width = width;
      frame->height = height;
      void *ptr_mlu[2] = {planes[0], planes[1]};
      frame->stride[0] = frame->stride[1] = width;
      frame->ctx.ddr_channel = g_channel_id;
      frame->ctx.dev_id = g_dev_id;
      frame->ctx.dev_type = DevContext::DevType::MLU;
      frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12;
      frame->dst_device_id = g_dev_id;
      frame->CopyToSyncMem(ptr_mlu, true);
      std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
      objs_holder->objs_.push_back(obj);
      data->collection.Add(kCNDataFrameTag, frame);
      data->collection.Add(kCNInferObjsTag, objs_holder);
      int ret = infer->Process(data);
      EXPECT_EQ(ret, 1);
      // create eos frame for clearing stream idx
      cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);
    }

    ASSERT_NO_THROW(infer->Close());
    ASSERT_TRUE(infer->Open(param));

    // test nv21
    {
      auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
      std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
      frame->frame_id = 1;
      data->timestamp = 1000;
      frame->width = width;
      frame->height = height;
      void *ptr_mlu[2] = {planes[0], planes[1]};
      frame->stride[0] = frame->stride[1] = width;
      frame->ctx.ddr_channel = g_channel_id;
      frame->ctx.dev_id = g_dev_id;
      frame->ctx.dev_type = DevContext::DevType::MLU;
      frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
      frame->dst_device_id = g_dev_id;
      frame->CopyToSyncMem(ptr_mlu, true);
      std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
      objs_holder->objs_.push_back(obj);
      data->collection.Add(kCNDataFrameTag, frame);
      data->collection.Add(kCNInferObjsTag, objs_holder);
      int ret = infer->Process(data);
      EXPECT_EQ(ret, 1);
      // create eos frame for clearing stream idx
      cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);
    }

    ASSERT_NO_THROW(infer->Close());
    param["mem_on_mlu_for_postproc"] = "true";
    ASSERT_TRUE(infer->Open(param));
    ResetGlobal();

    // test mem_on_mlu_for_postproc (true)
    {
      auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
      std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
      frame->frame_id = 1;
      data->timestamp = 1000;
      frame->width = width;
      frame->height = height;
      void *ptr_mlu[2] = {planes[0], planes[1]};
      frame->stride[0] = frame->stride[1] = width;
      frame->ctx.ddr_channel = g_channel_id;
      frame->ctx.dev_id = g_dev_id;
      frame->ctx.dev_type = DevContext::DevType::MLU;
      frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
      frame->dst_device_id = g_dev_id;
      frame->CopyToSyncMem(ptr_mlu, true);
      std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
      objs_holder->objs_.push_back(obj);
      data->collection.Add(kCNDataFrameTag, frame);
      data->collection.Add(kCNInferObjsTag, objs_holder);
      int ret = infer->Process(data);
      EXPECT_EQ(ret, 1);

      while (!gpostproc_done.load()) usleep(20 * 1000);

      EXPECT_TRUE(gcalled_execute_v2);

      // create eos frame for clearing stream idx
      cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);
    }

    ASSERT_NO_THROW(infer->Close());
    param["mem_on_mlu_for_postproc"] = "false";
    ASSERT_TRUE(infer->Open(param));
    ResetGlobal();

    // test mem_on_mlu_for_postproc (false)
    {
      auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
      std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
      frame->frame_id = 1;
      data->timestamp = 1000;
      frame->width = width;
      frame->height = height;
      void *ptr_mlu[2] = {planes[0], planes[1]};
      frame->stride[0] = frame->stride[1] = width;
      frame->ctx.ddr_channel = g_channel_id;
      frame->ctx.dev_id = g_dev_id;
      frame->ctx.dev_type = DevContext::DevType::MLU;
      frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
      frame->dst_device_id = g_dev_id;
      frame->CopyToSyncMem(ptr_mlu, true);
      std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
      objs_holder->objs_.push_back(obj);
      data->collection.Add(kCNDataFrameTag, frame);
      data->collection.Add(kCNInferObjsTag, objs_holder);
      int ret = infer->Process(data);
      EXPECT_EQ(ret, 1);

      while (!gpostproc_done.load()) usleep(20 * 1000);

      EXPECT_TRUE(gcalled_execute_v1);

      // create eos frame for clearing stream idx
      cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);
    }

    ASSERT_NO_THROW(infer->Close());
    mem_op.FreeMlu(frame_data);
  }

  // test with CPU preproc
  {
    std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
    ModuleParamSet param;
    param["model_path"] = model_path;
    param["func_name"] = g_func_name;
    param["preproc_name"] = "FakeObjPreproc";
    param["postproc_name"] = "FakeObjPostproc";
    param["device_id"] = std::to_string(g_dev_id);
    param["batching_timeout"] = "30";
    param["object_infer"] = "true";
    param["obj_filter_name"] = "FakeObjFilter";
    ASSERT_TRUE(infer->Open(param));

    const int width = 1920, height = 1080;
    size_t nbytes = width * height * sizeof(uint8_t) * 3 / 2;
    uint8_t *frame_data = new uint8_t[nbytes];

    auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
    std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
    frame->frame_id = 1;
    data->timestamp = 1000;
    frame->width = width;
    frame->height = height;
    void *ptr_cpu[2] = {frame_data, frame_data + nbytes * 2 / 3};

    frame->stride[0] = frame->stride[1] = width;
    frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
    frame->ctx.dev_type = DevContext::DevType::CPU;
    frame->dst_device_id = g_dev_id;
    frame->CopyToSyncMem(ptr_cpu, true);
    std::shared_ptr<CNInferObjs> objs_holder = std::make_shared<CNInferObjs>();
    objs_holder->objs_.push_back(obj);
    data->collection.Add(kCNDataFrameTag, frame);
    data->collection.Add(kCNInferObjsTag, objs_holder);

    int ret = infer->Process(data);
    EXPECT_EQ(ret, 1);
    delete[] frame_data;
    // create eos frame for clearing stream idx
    cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);
    ASSERT_NO_THROW(infer->Close());
  }
}

TEST(Inferencer, ProcessPerf) {
  std::string model_path = GetExePath() + GetModelPath();
  std::string image_path = GetExePath() + g_image_path;

  std::shared_ptr<Module> infer = std::make_shared<Inferencer>(name);
  ModuleParamSet param;
  param["model_path"] = model_path;
  param["func_name"] = g_func_name;
  param["postproc_name"] = g_postproc_name;
  param["device_id"] = std::to_string(g_dev_id);
  param["batching_timeout"] = "30";
  ASSERT_TRUE(infer->Open(param));

  const int width = 1280, height = 720;
  size_t nbytes = width * height * sizeof(uint8_t) * 3;
  size_t boundary = 1 << 16;
  nbytes = (nbytes + boundary - 1) & ~(boundary - 1);  // align to 64kb
  edk::MluMemoryOp mem_op;

  std::vector<void *> frame_data_vec;
  for (int i = 0; i < 32; i++) {
    // fake data
    void *frame_data = nullptr;
    void *planes[CN_MAX_PLANES] = {nullptr, nullptr};
    frame_data = mem_op.AllocMlu(nbytes);
    planes[0] = frame_data;                                                                        // y plane
    planes[1] = reinterpret_cast<void *>(reinterpret_cast<int64_t>(frame_data) + width * height);  // uv plane
    frame_data_vec.push_back(frame_data);

    auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
    std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
    data->collection.Add(kCNDataFrameTag, frame);
    frame->frame_id = i;
    data->timestamp = 1000;
    frame->width = width;
    frame->height = height;
    void *ptr_mlu[2] = {planes[0], planes[1]};
    frame->stride[0] = frame->stride[1] = width;
    frame->ctx.ddr_channel = g_channel_id;
    frame->ctx.dev_id = g_dev_id;
    frame->ctx.dev_type = DevContext::DevType::MLU;
    frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12;
    frame->dst_device_id = g_dev_id;
    frame->CopyToSyncMem(ptr_mlu, true);
    int ret = infer->Process(data);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(ret, 1);
  }
  // create eos frame for clearing stream idx
  cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);

  ASSERT_NO_THROW(infer->Close());
  for (auto it : frame_data_vec) {
    mem_op.FreeMlu(it);
  }
}

TEST(Inferencer, Postproc_set_threshold) {
  auto postproc = Postproc::Create(std::string(g_postproc_name));
  ASSERT_NE(postproc, nullptr);

  postproc->SetThreshold(0.6);
}

}  // namespace cnstream
