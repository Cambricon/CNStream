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
#include <stdio.h>
#include <sys/select.h>

#include <memory>
#include <string>
#include <thread>

#include "fps_stats.hpp"
#include "test_base.hpp"

namespace cnstream {

static const char *name = "fps_stats";

TEST(FpsStats, Construct) {
  std::shared_ptr<FpsStats> fps_stats = std::make_shared<FpsStats>(name);
  EXPECT_STREQ(fps_stats->GetName().c_str(), name);
}

TEST(FpsStats, CheckParamSet) {
  std::shared_ptr<FpsStats> fps_stats = std::make_shared<FpsStats>(name);
  ModuleParamSet param;
  EXPECT_TRUE(fps_stats->CheckParamSet(param));

  param["fake_key"] = "fake_value";
  EXPECT_TRUE(fps_stats->CheckParamSet(param));
}

TEST(FpsStats, Process) {
  std::shared_ptr<FpsStats> fps_stats = std::make_shared<FpsStats>(name);
  EXPECT_STREQ(fps_stats->GetName().c_str(), name);
  ModuleParamSet param;
  EXPECT_TRUE(fps_stats->Open(param));
  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  data->channel_idx = 0;
  auto data1 = cnstream::CNFrameInfo::Create(std::to_string(2));
  data1->channel_idx = GetMaxStreamNumber();
  EXPECT_EQ(fps_stats->Process(data), 0);
  EXPECT_EQ(fps_stats->Process(data1), -1);
  EXPECT_NO_THROW(fps_stats->ShowStatistics());
  fps_stats->Close();
}

}  // namespace cnstream
