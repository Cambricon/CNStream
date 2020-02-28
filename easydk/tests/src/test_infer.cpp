#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "easyinfer/easy_infer.h"
#include "easyinfer/mlu_context.h"
#include "easyinfer/mlu_memory_op.h"
#include "easyinfer/model_loader.h"
#include "test_base.h"

bool err_occured = false;
constexpr const char *gmodel_path_220 = "../../samples/data/models/MLU220/inceptionv3/inception-v3_int8_scale_dense_4batch_4core.cambricon";
constexpr const char *gmodel_path_270 = "../../samples/data/models/MLU270/resnet50_offline.cambricon";

bool test_context(int channel_id, bool multi_thread) {
  try {
    edk::MluContext context;
    int dev_id = 0;
    context.SetDeviceId(dev_id);
    context.SetChannelId(channel_id);
    context.ConfigureForThisThread();
    if (dev_id != context.DeviceId() || channel_id != context.ChannelId()) {
      throw edk::MluContextError("unmatched params (device id or channel id)");
    }
  } catch (edk::MluContextError &err) {
    if (multi_thread) {
      std::cout << "set context failed:\nchannel_id " + std::to_string(channel_id) << std::endl;
      err_occured = true;
      return false;
    } else {
      std::cout << err.what() << std::endl;
      return false;
    }
  }
  return true;
}

TEST(Easyinfer, MluContext) {
  edk::MluContext ctx;
  ASSERT_TRUE(ctx.CheckDeviceId(0)) << "Cannot find device 0";
  ASSERT_TRUE(test_context(0, false));
  ASSERT_TRUE(test_context(3, false));
  ASSERT_FALSE(test_context(4, false));
  ASSERT_FALSE(test_context(100, false));
  std::vector<std::thread> threads;
  for (int i = 0; i < 100; ++i) {
    threads.push_back(std::thread(&test_context, i % 4, true));
  }
  for (auto &it : threads) {
    it.join();
  }
  ASSERT_FALSE(err_occured);
}

TEST(Easyinfer, Shape) {
  uint32_t n = 1, c = 3, h = 124, w = 82, stride = 128;
  edk::Shape shape(n, h, w, c, stride);
  EXPECT_EQ(n, shape.n);
  EXPECT_EQ(c, shape.c);
  EXPECT_EQ(h, shape.h);
  EXPECT_EQ(w, shape.w);
  EXPECT_EQ(stride, shape.Stride());
  EXPECT_EQ(c * stride, shape.Step());
  EXPECT_EQ(h * w, shape.hw());
  EXPECT_EQ(h * w * c, shape.hwc());
  EXPECT_EQ(n * c * h * w, shape.nhwc());
  EXPECT_EQ(n * c * h * stride, shape.DataCount());
  std::cout << shape << std::endl;

  // stride should be equal to w, while set stride less than w
  n = 4, c = 1, h = 20, w = 96, stride = 64;
  shape.n = n;
  shape.c = c;
  shape.h = h;
  shape.w = w;
  shape.SetStride(stride);
  EXPECT_EQ(w, shape.Stride());

  edk::Shape another_shape(n, h, w, c, stride);
  EXPECT_TRUE(another_shape == shape);
  another_shape.c = c + 1;
  EXPECT_TRUE(another_shape != shape);
}

TEST(Easyinfer, ModelLoader) {
  std::string function_name = "subnet0";
  edk::MluContext context;
  context.SetDeviceId(0);
  context.ConfigureForThisThread();
  auto version = context.GetCoreVersion();
  std::string model_path = GetExePath();
  if (version == edk::CoreVersion::MLU220) {
      std::cout << "220 model" << std::endl;
    model_path += gmodel_path_220;
  } else if (version == edk::CoreVersion::MLU270) {
      std::cout << "270 model" << std::endl;
    model_path += gmodel_path_270;
  } else {
    ASSERT_TRUE(false) << "Unsupport core version" << static_cast<int>(version);
  }
  auto model_loader = std::make_shared<edk::ModelLoader>(model_path, function_name);
  model_loader->InitLayout();
  edk::DataLayout layout;
  layout.dtype = edk::DataType::FLOAT32;
  layout.order = edk::DimOrder::NHWC;
  model_loader->SetCpuInputLayout(layout, 0);
  layout.dtype = edk::DataType::FLOAT32;
  layout.order = edk::DimOrder::NCHW;
  model_loader->SetCpuOutputLayout(layout, 0);
  ASSERT_FALSE(model_loader->InputShapes().empty());
  EXPECT_GT(model_loader->InputShapes()[0].nhwc(), static_cast<uint32_t>(0));
  ASSERT_FALSE(model_loader->OutputShapes().empty());
  EXPECT_GT(model_loader->OutputShapes()[0].nhwc(), static_cast<uint32_t>(0));
}

TEST(Easyinfer, MluMemoryOp) {
  try {
    constexpr size_t kStrSize = 20;
    uint32_t batch_size = 1;
    char str[kStrSize] = "test memcpy";
    char str_out[kStrSize];
    void *in = reinterpret_cast<void *>(str);
    void *out = reinterpret_cast<void *>(str_out);
    edk::MluContext context;
    context.SetDeviceId(0);
    context.SetChannelId(0);
    context.ConfigureForThisThread();
    edk::MluMemoryOp mem_op;
    void *mlu_ptr = mem_op.AllocMlu(kStrSize, batch_size);
    mem_op.MemcpyH2D(mlu_ptr, in, kStrSize, batch_size);
    mem_op.MemcpyD2H(out, mlu_ptr, kStrSize, batch_size);
    mem_op.FreeMlu(mlu_ptr);
    EXPECT_STREQ(str, str_out);
  } catch (edk::Exception &err) {
    EXPECT_TRUE(false) << err.what();
  }
}

TEST(Easyinfer, Infer) {
  std::string function_name = "subnet0";
  int batch_size = 1;
  try {
    edk::MluContext context;
    context.SetDeviceId(0);
    context.ConfigureForThisThread();
    auto version = context.GetCoreVersion();
    std::string model_path = GetExePath();
    if (version == edk::CoreVersion::MLU220) {
      model_path += gmodel_path_220;
      std::cout << "220 model" << std::endl;
    } else if (version == edk::CoreVersion::MLU270) {
      model_path += gmodel_path_270;
      std::cout << "270 model" << std::endl;
    } else {
      ASSERT_TRUE(false) << "Unsupport core version" << static_cast<int>(version);
    }
    auto model_loader = std::make_shared<edk::ModelLoader>(model_path, function_name);
    model_loader->InitLayout();

    edk::MluMemoryOp mem_op;
    mem_op.SetLoader(model_loader);
    edk::EasyInfer infer;
    infer.Init(model_loader, batch_size, 0);
    EXPECT_EQ(infer.Loader(), model_loader);
    EXPECT_EQ(infer.BatchSize(), batch_size);

    void **mlu_input = mem_op.AllocMluInput(batch_size);
    void **mlu_output = mem_op.AllocMluOutput(batch_size);
    void **cpu_output = mem_op.AllocCpuOutput(batch_size);
    void **cpu_input = mem_op.AllocCpuInput(batch_size);

    mem_op.MemcpyInputH2D(mlu_input, cpu_input, batch_size);
    infer.Run(mlu_input, mlu_output);
    mem_op.MemcpyOutputD2H(cpu_output, mlu_output, batch_size);

    mem_op.FreeArrayMlu(mlu_input, model_loader->InputNum());
    mem_op.FreeArrayMlu(mlu_output, model_loader->OutputNum());
    mem_op.FreeCpuOutput(cpu_output);
    mem_op.FreeCpuInput(cpu_input);
  } catch (edk::Exception &err) {
    EXPECT_TRUE(false) << err.what();
  }
}
