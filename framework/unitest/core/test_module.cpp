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
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"
#include "perf_manager.hpp"

namespace cnstream {

const EventType T_type = EVENT_WARNING;
const char *T_messgge = "test_post_event";
extern std::string gTestPerfDir;

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
  EXPECT_FALSE(module.HasTransmit());
  TestModuleBaseEx module_ex;
  EXPECT_TRUE(module_ex.HasTransmit());
}

TEST(CoreModule, postevent) {
  Pipeline pipe("pipe");
  std::shared_ptr<TestModuleBase> ptr(new (TestModuleBase));
  TestModuleBase module;
  ModuleParamSet parames;
  ASSERT_TRUE(ptr->Open(parames));
  ASSERT_TRUE(pipe.AddModule(ptr));
  pipe.Start();
  EXPECT_TRUE(ptr->PostEvent(T_type, T_messgge));
  pipe.Stop();
}

#if 0
TEST(CoreModule, SetAndGetPerfManager) {
  Pipeline pipe("pipe");
  std::shared_ptr<TestModuleBase> ptr(new (TestModuleBase));
  TestModuleBase module;
  ModuleParamSet parames;
  ASSERT_TRUE(ptr->Open(parames));
  ASSERT_TRUE(pipe.AddModule(ptr));
  pipe.Start();
  std::vector<std::string> stream_ids = {"1", "2"};
  std::vector<std::string> m_names = {"m1", "m2"};

  std::vector<std::string> keys = PerfManager::GetKeys(m_names, {PerfManager::GetStartTimeSuffix(),
      PerfManager::GetEndTimeSuffix(), PerfManager::GetThreadSuffix()});

  std::unordered_map<std::string, std::shared_ptr<PerfManager>> managers;
  for (auto it : stream_ids) {
    managers[it] = std::make_shared<PerfManager>();
    EXPECT_TRUE(managers[it]->Init(gTestPerfDir + "test_" + it + ".db"));
    managers[it]->RegisterPerfType(PerfManager::GetDefaultType(), PerfManager::GetPrimaryKey(), keys);
  }
  ptr->SetPerfManagers(managers);

  for (auto it : stream_ids) {
    EXPECT_TRUE(ptr->GetPerfManager(it) != nullptr);
  }
  EXPECT_TRUE(ptr->GetPerfManager("wrong_stream") == nullptr);
  pipe.Stop();
}
#endif

}  // namespace cnstream
