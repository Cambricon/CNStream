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

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <map>
#include <vector>

#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"

namespace cnstream {

const EventType T_type = EventType::EVENT_WARNING;
const char *T_messgge = "test_post_event";

class TestModuleBase : public Module {
 public:
  TestModuleBase() : Module("test-module-base") {}
  ~TestModuleBase() {}

  bool Open(ModuleParamSet set) { return true; }
  void Close() {}
  int Process(std::shared_ptr<CNFrameInfo> data) { return 0; }
};

class TestModuleBaseEx : public ModuleEx {
 public:
  TestModuleBaseEx() : ModuleEx("test-module-base") {}
  ~TestModuleBaseEx() {}

  bool Open(ModuleParamSet set) { return true; }
  void Close() {}
  int Process(std::shared_ptr<CNFrameInfo> data) { return 0; }
};

TEST(CoreModule, OpenCloseProcess) {
  TestModuleBase module;
  ModuleParamSet params;
  EXPECT_TRUE(module.Open(params));
  module.Close();
  module.Process(nullptr);
}

TEST(CoreModule, TransmitAttr) {
  TestModuleBase module;
  EXPECT_FALSE(module.HasTransmit());
  TestModuleBaseEx module_ex;
  EXPECT_TRUE(module_ex.HasTransmit());
}

TEST(CoreModule, postevent) {
  Pipeline pipe("pipe");
  std::shared_ptr<TestModuleBase> ptr(new (TestModuleBase));
  ModuleParamSet parames;
  ASSERT_TRUE(ptr->Open(parames));
  ptr->SetContainer(&pipe);
  pipe.Start();
  EXPECT_TRUE(ptr->PostEvent(T_type, T_messgge));
  pipe.Stop();
}

}  // namespace cnstream
