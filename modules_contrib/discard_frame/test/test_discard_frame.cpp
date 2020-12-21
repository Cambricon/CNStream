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

#include <memory>

#include "discard_frame.hpp"
#include "test_base.hpp"

namespace cnstream {

static constexpr const char *gname = "discard_frame";

TEST(DiscardFrame, Constructor) {
  std::shared_ptr<Module> discard_frame = std::make_shared<DiscardFrame>(gname);
  EXPECT_STREQ(discard_frame->GetName().c_str(), gname);
  // bool variable
  EXPECT_TRUE(discard_frame->HasTransmit());
}

TEST(DiscardFrame, OpenClose) {
  std::shared_ptr<Module> discard_frame = std::make_shared<DiscardFrame>(gname);
  ModuleParamSet param;

  // open null param
  param.clear();
  EXPECT_TRUE(discard_frame->Open(param));

  param.clear();
  param["discard_int"] = std::to_string(12);
  EXPECT_TRUE(discard_frame->Open(param));

  param["discard_interval"] = std::to_string(4);
  EXPECT_TRUE(discard_frame->Open(param));

  // send the error frameid
  param["discard_interval"] = std::to_string(-1);
  EXPECT_FALSE(discard_frame->Open(param));

  discard_frame->Close();
}

TEST(DiscardFrame, Process) {
  std::shared_ptr<Module> discard_frame = std::make_shared<DiscardFrame>(gname);
  ModuleParamSet param;
  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  EXPECT_EQ(discard_frame->Process(data), 0);

  param["discard_interval"] = std::to_string(3);
  Pipeline Container("pipe");
  Container.Start();
  Container.AddModule(discard_frame);
  discard_frame->Open(param);
  EXPECT_EQ(discard_frame->Process(data), 1);
  Container.Stop();
}

}  // namespace cnstream
