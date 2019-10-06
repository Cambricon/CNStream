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
#include <string>
#include <vector>

#include "cnstream_module.hpp"

namespace cnstream {

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

TEST(CoreModule, SetGetName) {
  TestModuleBase module;

  uint32_t seed = (uint32_t)time(0);
  srand(time(nullptr));
  int test_num = rand_r(&seed) % 11 + 10;

  while (test_num--) {
    std::string name = "testname" + std::to_string(rand_r(&seed));
    module.SetName(name);
    EXPECT_EQ(name, module.GetName()) << "Module name not equal";
  }
}

TEST(CoreModule, OpenCloseProcess) {
  TestModuleBase module;
  ModuleParamSet params;
  EXPECT_TRUE(module.Open(params));
  module.Close();
  module.Process(nullptr);
}

TEST(CoreModule, ModuleMask) {
  uint32_t seed = (uint32_t)time(0);
  const uint32_t mask_len = 32;
  TestModuleBase module;
  uint64_t mask = 0;

  ModuleParamSet params;
  ASSERT_TRUE(module.Open(params));
  EXPECT_EQ(module.GetId(), (uint32_t)0);
  for (uint32_t i = 0; i < mask_len; ++i) {
    module.SetParentId(rand_r(&seed) % mask_len);
  }
  std::vector<size_t> p_ids = module.GetParentIds();
  for (auto &id : p_ids) {
    mask |= (uint64_t)1 << id;
  }
  EXPECT_EQ(module.GetModulesMask(), mask);
  module.Close();
}

TEST(CoreModule, TransmitAttr) {
  TestModuleBase module;
  EXPECT_FALSE(module.hasTranmit());
  TestModuleBaseEx module_ex;
  EXPECT_TRUE(module_ex.hasTranmit());
}

}  // namespace cnstream
