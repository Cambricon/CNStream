#include <glog/logging.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

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
  EXPECT_TRUE(module.Open(params));
  module.Close();

  // invalid window-width and window-height
  params["window-width"] = "-1920";
  params["window-height"] = "-1080";
  params["refresh-rate"] = "22";
  params["max-channels"] = "16";
  params["show"] = "false";
  EXPECT_FALSE(module.Open(params));

  params["window-width"] = "1920";
  params["window-height"] = "1080";
  params["refresh-rate"] = "22";
  params["max-channels"] = "16";
  params["show"] = "false";
  EXPECT_TRUE(module.Open(params));
  module.Close();
}

TEST(Display, Process) {
  std::shared_ptr<Module> display = std::make_shared<Displayer>(gname);
  int width = 1920;
  int height = 1080;
  ModuleParamSet params;
  params["window-width"] = std::to_string(width);
  params["window-height"] = std::to_string(height);
  params["refresh-rate"] = "22";
  params["max-channels"] = "16";
  params["show"] = "false";
  ASSERT_TRUE(display->Open(params));
  cv::Mat img(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  CNDataFrame &frame = data->frame;
  data->channel_idx = 0;
  frame.frame_id = 1;
  frame.timestamp = 1000;
  frame.width = width;
  frame.height = height;
  frame.ptr[0] = img.data;
  frame.stride[0] = width;
  frame.ctx.dev_type = DevContext::DevType::CPU;
  frame.fmt = CN_PIXEL_FORMAT_BGR24;
  frame.CopyToSyncMem();
  EXPECT_EQ(display->Process(data), 0);
  display->Close();
}

}  // namespace cnstream
