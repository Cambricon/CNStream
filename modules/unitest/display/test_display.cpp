#include <gtest/gtest.h>

#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "displayer.hpp"

namespace cnstream {

static constexpr const char *gname = "display";

TEST(Display, OpenClose) {
  Displayer module(gname);
  ModuleParamSet params;
  // empty param
  EXPECT_FALSE(module.Open(params));

  params["window-width"] = "1920";
  params["window-height"] = "1080";
  params["refresh-rate"] = "22";
  params["max-channels"] = "16";
  params["show"] = "false";
  params["full-screen"] = "false";
  EXPECT_TRUE(module.CheckParamSet(params));
  EXPECT_TRUE(module.Open(params));
  module.Close();

  // invalid window-width and window-height
  params["window-width"] = "-1920";
  params["window-height"] = "-1080";
  params["refresh-rate"] = "22";
  params["max-channels"] = "16";
  params["show"] = "false";
  EXPECT_FALSE(module.CheckParamSet(params));
  EXPECT_FALSE(module.Open(params));

  // invalid full-screen and show
  params["full-screen"] = "aaa";
  params["show"] = "bbb";
  EXPECT_FALSE(module.CheckParamSet(params));

  params["full-screen"] = "false";
  params["show"] = "false";
  params["window-width"] = "1920";
  params["window-height"] = "1080";
  params["refresh-rate"] = "22";
  params["max-channels"] = "16";
  params["show"] = "false";
  EXPECT_TRUE(module.CheckParamSet(params));
  EXPECT_TRUE(module.Open(params));
  module.Close();
}

TEST(Display, Process) {
  std::shared_ptr<Displayer> display = std::make_shared<Displayer>(gname);
  int width = 1920;
  int height = 1080;
  ModuleParamSet params;
  params["window-width"] = std::to_string(width);
  params["window-height"] = std::to_string(height);
  params["refresh-rate"] = "22";
  params["max-channels"] = "16";
  params["show"] = "false";
  ASSERT_TRUE(display->Open(params));

  cv::Mat img(height, width, CV_8UC3, cv::Scalar(0, 127, 0));
  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  data->SetStreamIndex(0);
  frame->frame_id = 1;
  data->timestamp = 1000;
  frame->width = width;
  frame->height = height;
  void* ptr_cpu[1] = {img.data};
  frame->stride[0] = width;
  frame->ctx.dev_type = DevContext::DevType::CPU;
  frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_BGR24;
  frame->CopyToSyncMem(ptr_cpu, false);
  data->collection.Add(kCNDataFrameTag, frame);
  EXPECT_EQ(display->Process(data), 0);
  auto thread_loop = [&display]() { display->GUILoop(nullptr); };
  std::thread thread_ = std::thread(thread_loop);
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  display->Close();
  if (thread_.joinable()) {
    thread_.join();
  }
}

}  // namespace cnstream
