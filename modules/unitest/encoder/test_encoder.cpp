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

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "encoder.hpp"
#include "test_base.hpp"

namespace cnstream {
static constexpr const char *gname = "encoder";
TEST(EncoderModule, SetGetName) {
  Encoder module(gname);
  uint32_t seed = (uint32_t)time(0);
  srand(time(nullptr));
  int test_num = rand_r(&seed) % 11 + 10;

  while (test_num--) {
    std::string name = "testname" + std::to_string(rand_r(&seed));
    module.SetName(name);
    EXPECT_EQ(name, module.GetName()) << "Module name not equal";
  }
}

TEST(EncoderModule, OpenClose) {
  Encoder module(gname);
  ModuleParamSet params;
  EXPECT_FALSE(module.Open(params));
  params["dump_dir"] = GetExePath();
  module.Close();
  // module.Process(nullptr);
}

// TEST(EncoderModule, Process) {
//   std::shared_ptr<Module> ptr = std::make_shared<Encoder>(gname);
//   // prepare data
//   int width = 1920;
//   int height = 1080;
//   cv::Mat img(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
//   auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
//   CNDataFrame &frame = data->frame;
//   frame.frame_id = 1;
//   frame.timestamp = 1000;
//   frame.width = width;
//   frame.height = height;
//   frame.ptr[0] = img.data;
//   frame.stride[0] = width;
//   frame.ctx.dev_type = DevContext::DevType::CPU;
//   frame.fmt = CN_PIXEL_FORMAT_BGR24;
//   frame.CopyToSyncMem();
//   EXPECT_EQ(ptr->Process(data), 0);
// }

}  // namespace cnstream
