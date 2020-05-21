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
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <future>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "cnstream_frame.hpp"
#include "cnstream_pipeline.hpp"
#include "test_base.hpp"

namespace cnstream {

extern std::string gTestPerfDir;

/*
  1. check frame count after processing
  2. check frame.flags do not have EOS mask during processing
  3. check process return -1
  4. check process after Open and process before Close
  5. Pipeline in different situations
  6. check case multiple video streams have different frame count
  7. check the same eos message will not sent twice
  8. check modules with different threads
 */

class MsgObserver : StreamMsgObserver {
 public:
  enum StopFlag { STOP_BY_EOS = 0, STOP_BY_ERROR };
  MsgObserver(int chn_cnt, std::shared_ptr<Pipeline> pipeline) : chn_cnt_(chn_cnt), pipeline_(pipeline) {}

  void Update(const StreamMsg& smsg) override {
    if (stop_) return;
    if (smsg.type == StreamMsgType::EOS_MSG) {
      LOG(INFO) << "[Observer] received EOS_MSG from channel " << smsg.chn_idx << "stream_id: " << smsg.stream_id;
      EXPECT_EQ(std::find(eos_stream_id_.begin(), eos_stream_id_.end(), smsg.stream_id), eos_stream_id_.end())
          << smsg.chn_idx << " " << eos_stream_id_.size() << " " << chn_cnt_;
      eos_stream_id_.insert(smsg.stream_id);
      if (static_cast<int>(eos_stream_id_.size()) == chn_cnt_) {
        stop_ = true;
        wakener_.set_value(STOP_BY_EOS);
      }
    } else if (smsg.type == StreamMsgType::ERROR_MSG) {
      LOG(INFO) << "[Observer] received ERROR_MSG";
      stop_ = true;
      wakener_.set_value(STOP_BY_ERROR);
    }
  }

  StopFlag WaitForStop() {
    StopFlag stop_flag = wakener_.get_future().get();

    pipeline_->Stop();

    return stop_flag;
  }

 private:
  int chn_cnt_ = 0;
  std::shared_ptr<Pipeline> pipeline_ = nullptr;
  std::set<std::string> eos_stream_id_;
  bool stop_ = false;
  std::promise<StopFlag> wakener_;
};

static const int __MIN_CHN_CNT__ = 1;
static const int __MAX_CHN_CNT__ = 64;

static const int __MIN_FRAME_CNT__ = 200;
static const int __MAX_FRAME_CNT__ = 1200;

static const std::vector<std::vector<std::list<int>>> g_neighbor_lists = {
    /*
      0 ---> 1 ---> 2
     */
    {{1}, {2}, {}},
    /*
      0 ---> 1 ---> 2
             |
               ---> 3
     */
    {{1}, {2, 3}, {}, {}},
    /*
      0 ---> 1 ---> 2
             |
               ---> 3 ---> 4
                    |
                      ---> 5
     */
    {{1}, {2, 3}, {}, {4, 5}, {}, {}},
    /*
      0 ---> 1 ---> 2
             |
               ---> 3 ---> 4 ---|
                    |           |
                    |            ---> 6
                    |           |
                      ---> 5 ---|
     */
    {{1}, {2, 3}, {}, {4, 5}, {6}, {6}, {}},
    /*
      0 ---> 1 ---> 2 ----------|
             |                  |
             |                   ---> 6
             |                  |
               ---> 3 ---> 4 ---|
                    |
                      ---> 5
     */
    {{1}, {2, 3}, {6}, {4, 5}, {6}, {}, {}}};

class TestProcessor : public Module {
 public:
  explicit TestProcessor(const std::string& name, int chns) : Module(name) { cnts_.resize(chns); }
  bool Open(ModuleParamSet param_set) override {
    opened_ = true;
    return true;
  }
  void Close() override { opened_ = false; }
  int Process(std::shared_ptr<CNFrameInfo> data) override {
    EXPECT_EQ(true, opened_);
    EXPECT_NE((uint32_t)1, CNFrameFlag::CN_FRAME_FLAG_EOS & data->frame.flags);
    uint32_t chn_idx = data->channel_idx;
    cnts_[chn_idx]++;
    return 0;
  }
  std::vector<uint64_t> GetCnts() const { return cnts_; }

 private:
  bool opened_ = false;
  std::vector<uint64_t> cnts_;
  static std::atomic<int> id_;
};  // class TestProcessor

class TestProcessorFailure : public TestProcessor {
 public:
  explicit TestProcessorFailure(int chns, int failure_ret_num)
      : TestProcessor("TestProcessorFailure", chns), e_(time(NULL)), failure_ret_num_(failure_ret_num) {
    std::uniform_int_distribution<> failure_frame_randomer(0, __MIN_FRAME_CNT__ - 1);
    std::uniform_int_distribution<> failure_chn_randomer(0, chns - 1);
    failure_chn_ = failure_chn_randomer(e_);
    failure_frame_ = failure_frame_randomer(e_);
  }
  int Process(std::shared_ptr<CNFrameInfo> data) override {
    uint32_t chn_idx = data->channel_idx;
    int64_t frame_idx = data->frame.frame_id;
    if (static_cast<int>(chn_idx) == failure_chn_ && frame_idx == failure_frame_) {
      return failure_ret_num_;
    }
    TestProcessor::Process(data);
    return 0;
  }
  void SetFailureFrameIdx(int idx) { failure_frame_ = idx; }

 private:
  std::default_random_engine e_;
  int failure_chn_ = -1;
  int failure_frame_ = -1;
  int failure_ret_num_ = -1;
};  // class TestProcessorFailure

class TestProvider : public TestProcessor {
 public:
  explicit TestProvider(int chns, Pipeline* pipeline) : TestProcessor("TestProvider", chns), pipeline_(pipeline) {
    EXPECT_TRUE(nullptr != pipeline);
    EXPECT_GT(chns, 0);
    std::default_random_engine e(time(NULL));
    std::uniform_int_distribution<> randomer(__MIN_FRAME_CNT__, __MAX_FRAME_CNT__);
    frame_cnts_.clear();
    for (int i = 0; i < chns; ++i) {
      frame_cnts_.push_back(randomer(e));
    }
  }

  void StartSendData() {
    threads_.clear();
    for (size_t i = 0; i < frame_cnts_.size(); ++i) {
      threads_.push_back(std::thread(&TestProvider::ThreadFunc, this, i));
    }
  }

  void StopSendData() {
    for (auto& it : threads_) {
      if (it.joinable()) {
        it.join();
      }
    }
  }

  std::vector<uint64_t> GetFrameCnts() const { return frame_cnts_; }

 private:
  void ThreadFunc(int chn_idx) {
    int64_t frame_idx = 0;
    uint64_t frame_cnt = frame_cnts_[chn_idx];
    while (frame_cnt--) {
      auto data = CNFrameInfo::Create(std::to_string(chn_idx));
      data->channel_idx = chn_idx;
      data->frame.frame_id = frame_idx++;
      if (!pipeline_->ProvideData(this, data)) {
        return;
      }
      if (frame_cnt == 0) {
        data = CNFrameInfo::Create(std::to_string(chn_idx), true);
        data->channel_idx = chn_idx;
        pipeline_->ProvideData(this, data);
        LOG(INFO) << "Send EOS:" << chn_idx << " frame id :" << frame_idx;
      }
    }
  }
  std::vector<std::thread> threads_;
  std::vector<uint64_t> frame_cnts_;
  Pipeline* pipeline_ = nullptr;
};  // class TestProvider

struct FailureDesc {
  FailureDesc(int failure_midx, int ret) : failure_module_idx(failure_midx), process_ret(ret) {}
  int failure_module_idx = -1;
  int process_ret = -1;
};

std::pair<std::vector<std::shared_ptr<Module>>, std::shared_ptr<Pipeline>> CreatePipelineByNeighborList(
    const std::vector<std::list<int>>& neighbor_list, FailureDesc fdesc = {-1, -1}) {
  std::default_random_engine e(time(NULL));
  std::uniform_int_distribution<> chns_randomer(__MIN_CHN_CNT__, __MAX_CHN_CNT__);
  auto chns = chns_randomer(e);
  auto pipeline = std::make_shared<Pipeline>("pipeline");
  std::vector<std::shared_ptr<Module>> modules;
  int processors_cnt = static_cast<int>(neighbor_list.size());
  modules.push_back(std::make_shared<TestProvider>(chns, pipeline.get()));
  for (int i = 1; i < processors_cnt; ++i) {
    if (fdesc.failure_module_idx != i) {
      auto module = std::make_shared<TestProcessor>("TestProcessor" + std::to_string(i), chns);
      modules.push_back(module);
    } else {
      modules.push_back(std::make_shared<TestProcessorFailure>(chns, fdesc.process_ret));
    }
  }

  // add modules
  for (auto module : modules) pipeline->AddModule(module);

  // set thread number
  std::uniform_int_distribution<> ths_randomer(1, dynamic_cast<TestProcessor*>(modules[0].get())->GetCnts().size());
  std::vector<uint32_t> thread_nums;
  pipeline->SetModuleAttribute(modules[0], 0);
  thread_nums.push_back(0);
  for (size_t i = 1; i < modules.size(); i++) {
    uint32_t thread_num = ths_randomer(e);
    EXPECT_TRUE(pipeline->SetModuleAttribute(modules[i], thread_num));
    // EXPECT_TRUE(pipeline->SetModuleAttribute(modules[i],
    // dynamic_cast<TestProcessor*>(modules[0].get())->GetCnts().size()));
    thread_nums.push_back(thread_num);
  }

  // check thread number
  for (size_t i = 0; i < modules.size(); i++) {
    EXPECT_TRUE(pipeline->GetModuleParallelism(modules[i]) == thread_nums[i]);
  }

  // link modules
  LOG(INFO) << "Graph:";
  for (size_t i = 0; i < modules.size(); ++i) {
    for (auto it : neighbor_list[i]) {
      EXPECT_TRUE("" != pipeline->LinkModules(modules[i], modules[it]));
      LOG(INFO) << i << " ---> " << it;
    }
  }

  return {modules, pipeline};
}

void TestProcess(const std::vector<std::list<int>>& neighbor_list) {
  auto pipeline_and_modules = CreatePipelineByNeighborList(neighbor_list);
  auto pipeline = pipeline_and_modules.second;
  auto modules = pipeline_and_modules.first;
  auto provider = dynamic_cast<TestProvider*>(modules[0].get());
  EXPECT_TRUE(nullptr != provider);

  MsgObserver msg_observer(provider->GetCnts().size(), pipeline);
  pipeline->SetStreamMsgObserver(reinterpret_cast<StreamMsgObserver*>(&msg_observer));

  pipeline->Start();
  provider->StartSendData();

  EXPECT_EQ(MsgObserver::STOP_BY_EOS, msg_observer.WaitForStop());
  provider->StopSendData();

  for (size_t i = 1; i < modules.size(); ++i) {
    auto processor = dynamic_cast<TestProcessor*>(modules[i].get());
    EXPECT_TRUE(nullptr != processor);
    for (size_t j = 0; j < processor->GetCnts().size(); ++j) {
      EXPECT_EQ(provider->GetFrameCnts()[j], processor->GetCnts()[j]);
    }
  }
}

void TestProcessFailure(const std::vector<std::list<int>>& neighbor_list, int process_ret) {
  std::default_random_engine e(time(NULL));
  std::uniform_int_distribution<> randomer(1, neighbor_list.size() - 1);
  int failure_module_idx = randomer(e);
  auto pipeline_and_modules = CreatePipelineByNeighborList(neighbor_list, {failure_module_idx, process_ret});
  auto pipeline = pipeline_and_modules.second;
  auto modules = pipeline_and_modules.first;
  auto provider = dynamic_cast<TestProvider*>(modules[0].get());
  EXPECT_TRUE(nullptr != provider);

  MsgObserver msg_observer(provider->GetCnts().size(), pipeline);
  pipeline->SetStreamMsgObserver(reinterpret_cast<StreamMsgObserver*>(&msg_observer));

  pipeline->Start();
  provider->StartSendData();

  EXPECT_EQ(MsgObserver::STOP_BY_ERROR, msg_observer.WaitForStop());
  provider->StopSendData();
}

TEST(CorePipeline, Pipeline_TestProcess0) { TestProcess(g_neighbor_lists[0]); }

TEST(CorePipeline, Pipeline_TestProcess1) { TestProcess(g_neighbor_lists[1]); }

TEST(CorePipeline, Pipeline_TestProcess2) { TestProcess(g_neighbor_lists[2]); }

TEST(CorePipeline, Pipeline_TestProcess3) { TestProcess(g_neighbor_lists[3]); }

TEST(CorePipeline, Pipeline_TestProcess4) { TestProcess(g_neighbor_lists[4]); }

TEST(CorePipeline, Pipeline_TestProcessFailure0) { TestProcessFailure(g_neighbor_lists[0], -1); }

TEST(CorePipeline, Pipeline_TestProcessFailure1) { TestProcessFailure(g_neighbor_lists[1], -1); }

TEST(CorePipeline, Pipeline_TestProcessFailure2) { TestProcessFailure(g_neighbor_lists[2], -1); }

TEST(CorePipeline, Pipeline_TestProcessFailure3) { TestProcessFailure(g_neighbor_lists[3], -1); }

TEST(CorePipeline, Pipeline_TestProcessFailure4) { TestProcessFailure(g_neighbor_lists[4], -1); }
/*************************************************************************************************
                                        unit test for each function
**************************************************************************************************/
class TestModule : public Module {
 public:
  explicit TestModule(const std::string& name) : Module(name) {}
  TestModule(const std::string& name, int return_val) : Module(name), return_val_(return_val) {}
  TestModule(const std::string& name, int return_val, bool has_transmit) : Module(name), return_val_(return_val) {
    if (has_transmit) {
      hasTransmit_.store(1);
    }
  }
  bool Open(ModuleParamSet paramSet) {
    (void)paramSet;
    return true;
  }
  void Close() {}
  int Process(std::shared_ptr<CNFrameInfo> data) { return return_val_; }

  int return_val_ = 0;
};  // class TestModule

class TestFailedModule : public Module {
 public:
  explicit TestFailedModule(const std::string& name) : Module(name) {}
  bool Open(ModuleParamSet paramSet) {
    (void)paramSet;
    return false;
  }
  void Close() {}
  int Process(std::shared_ptr<CNFrameInfo> data) { return -1; }
};  // class TestFailedModule

class TestObserver : public StreamMsgObserver {
 public:
  void Update(const StreamMsg& msg) override {
    EXPECT_EQ(msg.type, ERROR_MSG);
    EXPECT_EQ(msg.chn_idx, 0);
    EXPECT_EQ(msg.stream_id, "0");
  }
};  // class TestObserver

TEST(CorePipeline, ParseByJSONStr) {
  CNModuleConfig m_cfg;
  std::string source =
      "{\"class_name\":\"cnstream::DataSource\",\"parallelism\":0,\"max_input_queue_size\":30,"
      "\"next_modules\":[\"detector\"],\"custom_params\":{\"source_type\":\"ffmpeg\","
      "\"output_type\":\"mlu\",\"decoder_type\":\"mlu\",\"device_id\":0}}";
  m_cfg.ParseByJSONStr(source);
  EXPECT_EQ(m_cfg.className, "cnstream::DataSource");
  EXPECT_EQ(m_cfg.parallelism, 0);
  EXPECT_EQ(m_cfg.maxInputQueueSize, 30);
  EXPECT_EQ(m_cfg.next.size(), (unsigned int)1);
  EXPECT_EQ(m_cfg.next[0], "detector");
  EXPECT_EQ(m_cfg.parameters.size(), (unsigned int)4);
  EXPECT_EQ(m_cfg.parameters["source_type"], "ffmpeg");
  EXPECT_EQ(m_cfg.parameters["output_type"], "mlu");
  EXPECT_EQ(m_cfg.parameters["decoder_type"], "mlu");
  EXPECT_EQ(m_cfg.parameters["device_id"], "0");
}

TEST(CorePipeline, ParseByJSONStrDefault) {
  CNModuleConfig m_cfg;
  std::string json_str = "{\"class_name\":\"test\"}";
  m_cfg.ParseByJSONStr(json_str);
  EXPECT_EQ(m_cfg.className, "test");
  EXPECT_EQ(m_cfg.parallelism, 1);
  EXPECT_EQ(m_cfg.maxInputQueueSize, 20);
  EXPECT_EQ(m_cfg.next.size(), (unsigned int)0);
  EXPECT_EQ(m_cfg.parameters.size(), (unsigned int)0);
}

TEST(CorePipeline, ParseByJSONStrNextModule) {
  CNModuleConfig m_cfg;
  std::string json_str = "{\"class_name\":\"test\",\"next_modules\":[\"next1\",\"next2\",\"next3\"]}";
  m_cfg.ParseByJSONStr(json_str);
  EXPECT_EQ(m_cfg.className, "test");
  EXPECT_EQ(m_cfg.parallelism, 1);
  EXPECT_EQ(m_cfg.maxInputQueueSize, 20);
  EXPECT_EQ(m_cfg.next.size(), (unsigned int)3);
  EXPECT_EQ(m_cfg.next[0], "next1");
  EXPECT_EQ(m_cfg.next[1], "next2");
  EXPECT_EQ(m_cfg.next[2], "next3");
  EXPECT_EQ(m_cfg.parameters.size(), (unsigned int)0);
}

TEST(CorePipeline, ParseByJSONStrParseError) {
  CNModuleConfig m_cfg;
  std::string json_str = "";
  EXPECT_FALSE(m_cfg.ParseByJSONStr(json_str));
}

TEST(CorePipeline, ParseByJSONStrClassNameError) {
  CNModuleConfig m_cfg;
  // there is no class name, throw error
  std::string json_str = "{}";
  EXPECT_FALSE(m_cfg.ParseByJSONStr(json_str));
  // class name must be string type
  json_str = "{\"class_name\":0}";
  EXPECT_FALSE(m_cfg.ParseByJSONStr(json_str));
}

TEST(CorePipeline, ParseByJSONStrParallelismError) {
  CNModuleConfig m_cfg;
  // parallelism must be uint type
  std::string json_str = "{\"class_name\":\"test\",\"parallelism\":\"0\"}";
  EXPECT_FALSE(m_cfg.ParseByJSONStr(json_str));
}

TEST(CorePipeline, ParseByJSONStrMaxInputQueueError) {
  CNModuleConfig m_cfg;
  // max input queue size must be uint type
  std::string json_str = "{\"class_name\":\"test\",\"max_input_queue_size\":-1}";
  EXPECT_FALSE(m_cfg.ParseByJSONStr(json_str));
}

TEST(CorePipeline, ParseByJSONStrNextModuleError) {
  CNModuleConfig m_cfg;
  // next module must be array
  std::string json_str = "{\"class_name\":\"test\",\"next_modules\":\"next\"}";
  EXPECT_FALSE(m_cfg.ParseByJSONStr(json_str));
  // next module must be array of string
  json_str = "{\"class_name\":\"test\",\"next_modules\":[\"next\", 0]}";
  EXPECT_FALSE(m_cfg.ParseByJSONStr(json_str));
}

TEST(CorePipeline, ParseByJSONStrCustomParamsError) {
  CNModuleConfig m_cfg;
  // custom params must be an object
  std::string json_str = "{\"class_name\":\"test\",\"custom_params\":\"wrong\"}";
  EXPECT_FALSE(m_cfg.ParseByJSONStr(json_str));
}

TEST(CorePipeline, ParseByJSONFile) {
  std::string file_path = GetExePath() + "../../modules/unitest/core/data/";
  std::string file_name = file_path + "config.json";
  CNModuleConfig m_cfg;
  // valid file path
  m_cfg.ParseByJSONFile(file_name);
  EXPECT_EQ(m_cfg.className, "test");
  EXPECT_EQ(m_cfg.parallelism, 1);
  EXPECT_EQ(m_cfg.maxInputQueueSize, 20);
  EXPECT_EQ(m_cfg.next.size(), (unsigned int)0);
  EXPECT_EQ(m_cfg.parameters.size(), (unsigned int)1);
  EXPECT_EQ(m_cfg.parameters[CNS_JSON_DIR_PARAM_NAME], file_path);
}

TEST(CorePipeline, ParseByJSONFileError) {
  CNModuleConfig m_cfg;
  // invaid file path
  EXPECT_FALSE(m_cfg.ParseByJSONFile(""));
}

TEST(CorePipeline, OpenCloseProcess) {
  Pipeline pipeline("test pipeline");
  ModuleParamSet param_set;
  EXPECT_TRUE(pipeline.Open(param_set));
  EXPECT_EQ(pipeline.Process(nullptr), 0);
  EXPECT_NO_THROW(pipeline.Close());
}

TEST(CorePipeline, DefaultBusWatch) {
  Pipeline pipeline("test pipeline");
  TestModule module("test_moudle");
  std::thread::id thread_id = std::this_thread::get_id();
  Event event = {EVENT_ERROR, "test event", &module, thread_id};
  EventHandleFlag e_handle_flag;
  e_handle_flag = pipeline.DefaultBusWatch(event, &module);
  EXPECT_EQ(e_handle_flag, EVENT_HANDLE_STOP);

  event.type = EVENT_WARNING;
  e_handle_flag = pipeline.DefaultBusWatch(event, &module);
  EXPECT_EQ(e_handle_flag, EVENT_HANDLE_SYNCED);

  event.type = EVENT_STOP;
  e_handle_flag = pipeline.DefaultBusWatch(event, &module);
  EXPECT_EQ(e_handle_flag, EVENT_HANDLE_STOP);

  event.type = EVENT_EOS;
  e_handle_flag = pipeline.DefaultBusWatch(event, &module);
  EXPECT_EQ(e_handle_flag, EVENT_HANDLE_SYNCED);

  event.type = EVENT_INVALID;
  e_handle_flag = pipeline.DefaultBusWatch(event, &module);
  EXPECT_EQ(e_handle_flag, EVENT_HANDLE_NULL);
}

TEST(CorePipeline, ProvideData) {
  Pipeline pipeline("test pipeline");
  auto module = std::make_shared<TestModule>("test_module");
  // create eos frame
  std::shared_ptr<CNFrameInfo> data = CNFrameInfo::Create("0", true);
  // add module
  EXPECT_TRUE(pipeline.AddModule(module));
  // provide data
  EXPECT_TRUE(pipeline.ProvideData(module.get(), data));
}

TEST(CorePipeline, ProvideDataFailed) {
  Pipeline pipeline("test pipeline");
  auto module = std::make_shared<TestModule>("test_module");
  // create eos frame
  std::shared_ptr<CNFrameInfo> data = CNFrameInfo::Create("0", true);
  // provide data, module is not found
  EXPECT_FALSE(pipeline.ProvideData(module.get(), data));
}

TEST(CorePipeline, AddModule) {
  Pipeline pipeline("test pipeline");
  // pipeline is a special module, so id of pipeline 0
  EXPECT_EQ(pipeline.GetId(), (uint32_t)0);

  uint32_t seed = (uint32_t)time(0);
  // except pipeline itself, the pipeline could contain other (GetMaxModuleNumber - 1) modules
  uint32_t module_num = rand_r(&seed) % (GetMaxModuleNumber() - 1) + 1;
  for (uint32_t i = 0; i < module_num; i++) {
    auto module = std::make_shared<TestModule>("test_module" + std::to_string(i));
    EXPECT_EQ(module->GetName(), "test_module" + std::to_string(i));
    EXPECT_TRUE(pipeline.AddModule(module));
    EXPECT_EQ(module->GetId(), i + 1);
  }
}

TEST(CorePipeline, AddModuleTwiceToPipeline) {
  Pipeline pipeline("test pipeline");
  auto module = std::make_shared<TestModule>("test_module");

  // add module
  EXPECT_TRUE(pipeline.AddModule(module));
  EXPECT_EQ(module->GetId(), (unsigned int)1);
  // add the same module twice
  EXPECT_FALSE(pipeline.AddModule(module));
  EXPECT_EQ(module->GetId(), (unsigned int)1);
}

TEST(CorePipeline, AddModuleExcessPipelineCapacity) {
  Pipeline pipeline("test pipeline");
  uint32_t module_num = GetMaxModuleNumber() - 1;
  for (uint32_t i = 0; i < module_num; i++) {
    auto module = std::make_shared<TestModule>("test_module" + std::to_string(i));
    EXPECT_EQ(module->GetName(), "test_module" + std::to_string(i));
    EXPECT_TRUE(pipeline.AddModule(module));
    EXPECT_EQ(module->GetId(), i + 1);
  }
  // there are already GetMaxModuleNumber() modules, can not get id for this new module
  auto module = std::make_shared<TestModule>("test_module" + std::to_string(module_num));
  EXPECT_EQ(module->GetName(), "test_module" + std::to_string(module_num));
  EXPECT_FALSE(pipeline.AddModule(module));
  EXPECT_EQ(module->GetId(), (size_t)-1);
}

TEST(CorePipeline, SetAndGetModuleParallelism) {
  Pipeline pipeline("test pipeline");
  auto module = std::make_shared<TestModule>("test_module");
  EXPECT_TRUE(pipeline.AddModule(module));
  uint32_t paral = 32;
  EXPECT_TRUE(pipeline.SetModuleAttribute(module, paral));
  EXPECT_EQ(pipeline.GetModuleParallelism(module), paral);
}

TEST(CorePipeline, SetModuleAttributeFailed) {
  Pipeline pipeline("test pipeline");
  auto module = std::make_shared<TestModule>("test_module");
  // can not find module in the pipeline
  EXPECT_FALSE(pipeline.SetModuleAttribute(module, 32));
  EXPECT_EQ(pipeline.GetModuleParallelism(module), (uint32_t)0);
}

TEST(CorePipeline, LinkModules) {
  Pipeline pipeline("test pipeline");
  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node_1 = std::make_shared<TestModule>("down_node_1");
  EXPECT_TRUE(pipeline.AddModule(up_node));
  EXPECT_TRUE(pipeline.AddModule(down_node_1));
  /* up_node ---- down_node_1 */
  std::string link_id = up_node->GetName() + "-->" + down_node_1->GetName();
  EXPECT_EQ(pipeline.LinkModules(up_node, down_node_1), link_id);
  EXPECT_EQ(down_node_1->GetParentIds().size(), (uint32_t)1);
  EXPECT_EQ(down_node_1->GetParentIds()[0], up_node->GetId());

  /* up_node ---- down_node_1
             |
             ---- down_node_2 */
  auto down_node_2 = std::make_shared<TestModule>("down_node_2");
  EXPECT_TRUE(pipeline.AddModule(down_node_2));
  link_id = up_node->GetName() + "-->" + down_node_2->GetName();
  EXPECT_EQ(pipeline.LinkModules(up_node, down_node_2), link_id);
  EXPECT_EQ(down_node_2->GetParentIds().size(), (uint32_t)1);
  EXPECT_EQ(down_node_2->GetParentIds()[0], up_node->GetId());

  /* up_node ---- down_node_1 ----
             |                    |---down_down_node
             ---- down_node_2 ----                    */
  auto down_down_node = std::make_shared<TestModule>("down_down_node");
  EXPECT_TRUE(pipeline.AddModule(down_down_node));
  link_id = down_node_1->GetName() + "-->" + down_down_node->GetName();
  EXPECT_EQ(pipeline.LinkModules(down_node_1, down_down_node), link_id);
  EXPECT_EQ(down_down_node->GetParentIds().size(), (uint32_t)1);
  EXPECT_EQ(down_down_node->GetParentIds()[0], down_node_1->GetId());

  link_id = down_node_2->GetName() + "-->" + down_down_node->GetName();
  EXPECT_EQ(pipeline.LinkModules(down_node_2, down_down_node), link_id);
  EXPECT_EQ(down_down_node->GetParentIds().size(), (uint32_t)2);
  EXPECT_EQ(down_down_node->GetParentIds()[0], down_node_1->GetId());
  EXPECT_EQ(down_down_node->GetParentIds()[1], down_node_2->GetId());
}

TEST(CorePipeline, LinkModulesTwice) {
  Pipeline pipeline("test pipeline");
  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node = std::make_shared<TestModule>("down_node");
  EXPECT_TRUE(pipeline.AddModule(up_node));
  EXPECT_TRUE(pipeline.AddModule(down_node));
  /* up_node ---- down_node */
  std::string link_id = up_node->GetName() + "-->" + down_node->GetName();
  EXPECT_EQ(pipeline.LinkModules(up_node, down_node), link_id);

  // link twice, will log error and return link id
  EXPECT_EQ(pipeline.LinkModules(up_node, down_node), link_id);
}

TEST(CorePipeline, LinkModulesFailed) {
  Pipeline pipeline("test pipeline");
  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node = std::make_shared<TestModule>("down_node");
  // up and down modules are not added to pipeline, link module failed
  EXPECT_EQ(pipeline.LinkModules(up_node, down_node), "");

  // down modules is not added to pipeline, link module failed
  EXPECT_TRUE(pipeline.AddModule(up_node));
  EXPECT_EQ(pipeline.LinkModules(up_node, down_node), "");
}

TEST(CorePipeline, QueryLinkStatus) {
  Pipeline pipeline("test pipeline");
  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node = std::make_shared<TestModule>("down_node");
  EXPECT_TRUE(pipeline.AddModule(up_node));
  EXPECT_TRUE(pipeline.AddModule(down_node));
  /* up_node ---- down_node */
  std::string link_id = up_node->GetName() + "-->" + down_node->GetName();
  EXPECT_EQ(pipeline.LinkModules(up_node, down_node), link_id);

  LinkStatus status;
  EXPECT_TRUE(pipeline.QueryLinkStatus(&status, link_id));
  // default false
  EXPECT_FALSE(status.stopped);
  // default parallelism is 1
  EXPECT_EQ(status.cache_size.size(), (uint32_t)1);
  // data queue has 0 element
  EXPECT_EQ(status.cache_size[0], (uint32_t)0);

  auto down_node_2 = std::make_shared<TestModule>("down_node_2");
  uint32_t seed = (uint32_t)time(0);
  // parallelism between [1, 64]
  uint32_t paral = rand_r(&seed) % 64 + 1;
  EXPECT_TRUE(pipeline.AddModule(down_node_2));
  pipeline.SetModuleAttribute(down_node_2, paral);

  /* up_node ---- down_node
             |
             ---- down_node_2 */
  link_id = up_node->GetName() + "-->" + down_node_2->GetName();
  EXPECT_EQ(pipeline.LinkModules(up_node, down_node_2), link_id);

  status.cache_size.clear();
  EXPECT_TRUE(pipeline.QueryLinkStatus(&status, link_id));
  // default false
  EXPECT_FALSE(status.stopped);
  // parallelism is set to paral
  EXPECT_EQ(status.cache_size.size(), paral);
  // data queue has 0 element
  for (auto iter : status.cache_size) {
    EXPECT_EQ(iter, (uint32_t)0);
  }
}

TEST(CorePipeline, QueryLinkStatusFailed) {
  Pipeline pipeline("test pipeline");
  LinkStatus status;
  std::string wrong_link_id = "up-->down";
  // can not find link id
  EXPECT_FALSE(pipeline.QueryLinkStatus(&status, wrong_link_id));

  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node = std::make_shared<TestModule>("down_node");
  EXPECT_TRUE(pipeline.AddModule(up_node));
  EXPECT_TRUE(pipeline.AddModule(down_node));
  /* up_node ---- down_node */
  std::string link_id = up_node->GetName() + "-->" + down_node->GetName();
  EXPECT_EQ(pipeline.LinkModules(up_node, down_node), link_id);

  // pointer to LinkStatus is nullptr
  EXPECT_FALSE(pipeline.QueryLinkStatus(nullptr, link_id));
}

TEST(CorePipeline, StartStopPipeline) {
  Pipeline pipeline("test pipeline");

  // no modules is added to pipeline
  EXPECT_TRUE(pipeline.Start());
  EXPECT_TRUE(pipeline.GetEventBus()->running_);
  EXPECT_TRUE(pipeline.IsRunning());
  EXPECT_TRUE(pipeline.Stop());

  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node = std::make_shared<TestModule>("down_node");
  EXPECT_TRUE(pipeline.AddModule(up_node));
  EXPECT_TRUE(pipeline.AddModule(down_node));
  std::string link_id = pipeline.LinkModules(up_node, down_node);
  EXPECT_NE(link_id, "");

  // two linked modules are added to the pipeline
  EXPECT_TRUE(pipeline.Start());

  EXPECT_TRUE(pipeline.GetEventBus()->running_);
  EXPECT_TRUE(pipeline.IsRunning());

  EXPECT_TRUE(pipeline.Stop());

  LinkStatus status;
  EXPECT_TRUE(pipeline.QueryLinkStatus(&status, link_id));
  EXPECT_TRUE(status.stopped);
  EXPECT_FALSE(pipeline.GetEventBus()->running_);
  EXPECT_FALSE(pipeline.IsRunning());
}

TEST(CorePipeline, StartPipelineFailed) {
  Pipeline pipeline("test pipeline");
  auto module = std::make_shared<TestFailedModule>("test_module");
  EXPECT_TRUE(pipeline.AddModule(module));

  // Open module failed
  EXPECT_FALSE(pipeline.Start());

  EXPECT_FALSE(pipeline.GetEventBus()->running_);
  EXPECT_FALSE(pipeline.IsRunning());

  // pipeline is not runnning, return true directly
  EXPECT_TRUE(pipeline.Stop());
}

TEST(CorePipeline, EventLoop) {
  Pipeline pipeline("test pipeline");
  auto module = std::make_shared<TestModule>("test_module");
  EXPECT_TRUE(pipeline.AddModule(module));

  // 1 Error event
  EXPECT_TRUE(pipeline.Start());
  // jump out the event loop
  EventType type = EVENT_ERROR;
  EXPECT_TRUE(module->PostEvent(type, "post error event for test"));

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_TRUE(pipeline.Stop());

  // 2 Warning and Eos event
  EXPECT_TRUE(pipeline.Start());
  type = EVENT_WARNING;
  // still in the event loop
  EXPECT_TRUE(module->PostEvent(type, "post warning event for test"));

  type = EVENT_EOS;
  // still in the event loop
  EXPECT_TRUE(module->PostEvent(type, "post eos event for test"));

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  // jump out of the event loop and stop pipeline
  EXPECT_TRUE(pipeline.Stop());

  // 3 Stop event
  EXPECT_TRUE(pipeline.Start());
  type = EVENT_STOP;
  // jump out of the event loop
  EXPECT_TRUE(module->PostEvent(type, "post stop event for test"));

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_TRUE(pipeline.Stop());

  // 5 Invalid event
  EXPECT_TRUE(pipeline.Start());
  type = EVENT_INVALID;
  // jump out of the event loop
  EXPECT_TRUE(module->PostEvent(type, "post invalid event for test"));

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_TRUE(pipeline.Stop());
}

TEST(CorePipeline, TransmitData) {
  Pipeline pipeline("test pipeline");

  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node = std::make_shared<TestModule>("down_node");
  EXPECT_TRUE(pipeline.AddModule(up_node));
  EXPECT_TRUE(pipeline.AddModule(down_node));
  uint32_t seed = (uint32_t)time(0);
  uint32_t paral = rand_r(&seed) % 64 + 1;
  pipeline.SetModuleAttribute(down_node, paral);
  /* up_node ---- down_node */
  std::string link_id = up_node->GetName() + "-->" + down_node->GetName();
  EXPECT_EQ(pipeline.LinkModules(up_node, down_node), link_id);

  {
    LinkStatus status;
    EXPECT_TRUE(pipeline.QueryLinkStatus(&status, link_id));
    // down node parallelism is paral
    EXPECT_EQ(status.cache_size.size(), (uint32_t)paral);
  }

  for (uint32_t i = 0; i < paral; i++) {
    LinkStatus status;
    auto data = CNFrameInfo::Create(std::to_string(i));
    data->channel_idx = i;
    EXPECT_NO_THROW(pipeline.TransmitData("up_node", data));

    EXPECT_TRUE(pipeline.QueryLinkStatus(&status, link_id));
    // down node parallelism is paral
    EXPECT_EQ(status.cache_size.size(), (uint32_t)paral);
    // conveyor i has 1 element
    EXPECT_EQ(status.cache_size[i], (uint32_t)1);
  }

  {
    uint32_t stream_idx = rand_r(&seed) % paral;
    LinkStatus status;
    auto data = CNFrameInfo::Create(std::to_string(stream_idx));
    data->channel_idx = stream_idx;
    EXPECT_NO_THROW(pipeline.TransmitData("up_node", data));

    EXPECT_TRUE(pipeline.QueryLinkStatus(&status, link_id));
    // down node parallelism is paral
    EXPECT_EQ(status.cache_size.size(), (uint32_t)paral);
    // conveyor i has 1 element
    EXPECT_EQ(status.cache_size[stream_idx], (uint32_t)2);
  }
}

TEST(CorePipeline, TransmitDataEosFrame) {
  Pipeline pipeline("test pipeline");

  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node = std::make_shared<TestModule>("down_node");
  EXPECT_TRUE(pipeline.AddModule(up_node));
  EXPECT_TRUE(pipeline.AddModule(down_node));
  /* up_node ---- down_node */
  std::string link_id = up_node->GetName() + "-->" + down_node->GetName();
  EXPECT_EQ(pipeline.LinkModules(up_node, down_node), link_id);
  {
    LinkStatus status;
    EXPECT_TRUE(pipeline.QueryLinkStatus(&status, link_id));
    // down node parallelism is paral
    EXPECT_EQ(status.cache_size.size(), (uint32_t)1);
  }
  auto data = CNFrameInfo::Create("0", true);
  data->channel_idx = 0;
  EXPECT_NO_THROW(pipeline.TransmitData("up_node", data));
  {
    LinkStatus status;
    EXPECT_TRUE(pipeline.QueryLinkStatus(&status, link_id));
    // down node parallelism is paral
    EXPECT_EQ(status.cache_size.size(), (uint32_t)1);
    EXPECT_EQ(status.cache_size[0], (uint32_t)1);
  }
}

TEST(CorePipelineDeathTest, TransmitDataFailed) {
  Pipeline pipeline("test pipeline");
  auto module = std::make_shared<TestModule>("test_module");
  auto data = CNFrameInfo::Create("0");
  EXPECT_TRUE(pipeline.AddModule(module));

  // As there is only one module, there is no links. Data will not be transmitted.
  EXPECT_NO_THROW(pipeline.TransmitData("test_module", data));
  EXPECT_DEATH(pipeline.TransmitData("", data), "");
}

void RunTaskLoop(std::shared_ptr<TestModule> up_node, std::shared_ptr<TestModule> down_node) {
  Pipeline pipeline("test pipeline");
  EXPECT_TRUE(pipeline.AddModule(up_node));
  EXPECT_TRUE(pipeline.AddModule(down_node));
  pipeline.SetModuleAttribute(down_node, 1);
  /* up_node ---- down_node */
  std::string link_id = up_node->GetName() + "-->" + down_node->GetName();
  EXPECT_EQ(pipeline.LinkModules(up_node, down_node), link_id);
  {
    LinkStatus status;
    auto data = CNFrameInfo::Create("0");
    data->channel_idx = 0;
    EXPECT_NO_THROW(pipeline.TransmitData("up_node", data));

    data = CNFrameInfo::Create("0", true);
    data->channel_idx = 0;
    EXPECT_NO_THROW(pipeline.TransmitData("up_node", data));

    EXPECT_TRUE(pipeline.QueryLinkStatus(&status, link_id));
    // down node parallelism is 1
    EXPECT_EQ(status.cache_size.size(), (uint32_t)1);
    // conveyor has 2 element
    EXPECT_EQ(status.cache_size[0], (uint32_t)2);
  }
  pipeline.Start();

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_TRUE(pipeline.Stop());
}

TEST(CorePipeline, TaskLoop) {
  // down node process() will return 0, which means success. And pipeline should be charge of transmitting data.
  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node = std::make_shared<TestModule>("down_node", 0);
  RunTaskLoop(up_node, down_node);
}

TEST(CorePipeline, TaskLoopProcessFailed) {
  // down node process() will return -1, which means failed. TaskLoop() will return.
  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node = std::make_shared<TestModule>("down_node", -1);
  RunTaskLoop(up_node, down_node);
}

TEST(CorePipeline, TaskLoopProcessHasTrans) {
  // down node process() will return 0, which means success. And moudle should transmit data by itself.
  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node = std::make_shared<TestModule>("down_node", 1, true);
  RunTaskLoop(up_node, down_node);
}

TEST(CorePipeline, TaskLoopProcessHasTransFailed) {
  // down node process() will return 0, which means success. And moudle should transmit data by itself.
  // However, the module can not transmit data, which is not right. So TaskLoop() will return.
  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node = std::make_shared<TestModule>("down_node", 1);
  RunTaskLoop(up_node, down_node);
}

std::vector<CNModuleConfig> GetCfg() {
  std::vector<CNModuleConfig> m_cfgs;
  CNModuleConfig m_cfg_src;
  m_cfg_src.name = "test_source";
  m_cfg_src.className = "cnstream::DataSource";
  m_cfg_src.parallelism = 0;
  m_cfg_src.maxInputQueueSize = 30;
  m_cfg_src.next.push_back("test_infer");
  m_cfg_src.parameters["source_type"] = "ffmpeg";
  m_cfg_src.parameters["output_type"] = "mlu";
  m_cfg_src.parameters["decoder_type"] = "mlu";
  m_cfg_src.parameters["device_id"] = "0";

  CNModuleConfig m_cfg_infer;
  m_cfg_infer.name = "test_infer";
  m_cfg_infer.className = "cnstream::Inferencer";
  m_cfg_infer.parallelism = 32;
  m_cfg_infer.parameters["model_path"] = "../data/models/MLU100/Primary_Detector/resnet34ssd/resnet34_ssd.cambricon";
  m_cfg_infer.parameters["func_name"] = "subnet0";
  m_cfg_infer.parameters["postproc_name"] = "PostprocSsd";
  m_cfg_infer.parameters["device_id"] = "0";

  m_cfgs.push_back(m_cfg_src);
  m_cfgs.push_back(m_cfg_infer);
  return m_cfgs;
}

TEST(CorePipeline, AddAndGetModuleConfig) {
  Pipeline pipeline("test pipeline");
  std::vector<CNModuleConfig> m_cfgs = GetCfg();
  CNModuleConfig m_cfg_src = m_cfgs[0];
  CNModuleConfig m_cfg_infer = m_cfgs[1];

  EXPECT_EQ(pipeline.AddModuleConfig(m_cfg_src), 0);
  EXPECT_EQ(pipeline.AddModuleConfig(m_cfg_infer), 0);

  {
    CNModuleConfig m_cfg;
    m_cfg = pipeline.GetModuleConfig("test_source");
    EXPECT_EQ(m_cfg.name, m_cfg_src.name);
    EXPECT_EQ(m_cfg.className, m_cfg_src.className);
    EXPECT_EQ(m_cfg.parallelism, m_cfg_src.parallelism);
    EXPECT_EQ(m_cfg.maxInputQueueSize, m_cfg_src.maxInputQueueSize);
    EXPECT_EQ(m_cfg.next.size(), (unsigned int)1);
    EXPECT_EQ(m_cfg.next[0], "test_infer");
    EXPECT_EQ(m_cfg.parameters.size(), (unsigned int)4);
    EXPECT_EQ(m_cfg.parameters, m_cfg_src.parameters);

    ModuleParamSet params;
    params = pipeline.GetModuleParamSet("test_source");
    EXPECT_EQ(params, m_cfg_src.parameters);
  }
  {
    CNModuleConfig m_cfg;
    m_cfg = pipeline.GetModuleConfig("test_infer");
    EXPECT_EQ(m_cfg.name, m_cfg_infer.name);
    EXPECT_EQ(m_cfg.className, m_cfg_infer.className);
    EXPECT_EQ(m_cfg.parallelism, m_cfg_infer.parallelism);
    EXPECT_EQ(m_cfg.maxInputQueueSize, m_cfg_infer.maxInputQueueSize);
    EXPECT_EQ(m_cfg.next.size(), (unsigned int)0);
    EXPECT_EQ(m_cfg.parameters.size(), (unsigned int)4);
    EXPECT_EQ(m_cfg.parameters, m_cfg_infer.parameters);

    ModuleParamSet params;
    params = pipeline.GetModuleParamSet("test_infer");
    EXPECT_EQ(params, m_cfg_infer.parameters);
  }
}

TEST(CorePipeline, GetWrongModuleConfigAndParamSet) {
  Pipeline pipeline("test pipeline");
  CNModuleConfig m_cfg;
  ModuleParamSet params;

  m_cfg = pipeline.GetModuleConfig("");

  EXPECT_EQ(m_cfg.name, "");
  EXPECT_EQ(m_cfg.className, "");
  EXPECT_EQ(m_cfg.parallelism, 0);
  EXPECT_EQ(m_cfg.maxInputQueueSize, 0);
  EXPECT_EQ(m_cfg.next.size(), (unsigned int)0);
  EXPECT_EQ(m_cfg.parameters.size(), (unsigned int)0);
  EXPECT_EQ(m_cfg.parameters, params);

  EXPECT_EQ(pipeline.GetModuleParamSet(""), params);
}

TEST(CorePipeline, BuildPipeline) {
  Pipeline pipeline("test pipeline");
  std::vector<CNModuleConfig> m_cfgs = GetCfg();
  EXPECT_EQ(pipeline.BuildPipeline(m_cfgs), 0);
}

TEST(CorePipeline, BuildPipelineByJSONFile) {
  Pipeline pipeline("test pipeline");
  std::string file_path = GetExePath() + "../../modules/unitest/core/data/pipeline.json";
  EXPECT_EQ(pipeline.BuildPipelineByJSONFile(file_path), 0);
}

TEST(CorePipeline, BuildPipelineByJSONFileFailed) {
  Pipeline pipeline("test pipeline");
  std::string empty_file_path = "";
  EXPECT_EQ(pipeline.BuildPipelineByJSONFile(empty_file_path), -1);

  std::string parse_error_file_path = GetExePath() + "../../modules/unitest/core/data/parse_error.json";
  EXPECT_EQ(pipeline.BuildPipelineByJSONFile(parse_error_file_path), -1);

  std::string name_error_file_path = GetExePath() + "../../modules/unitest/core/data/name_error.json";
  EXPECT_EQ(pipeline.BuildPipelineByJSONFile(name_error_file_path), -1);
}

TEST(CorePipeline, GetModule) {
  Pipeline pipeline("test pipeline");
  std::vector<CNModuleConfig> m_cfgs = GetCfg();
  EXPECT_EQ(pipeline.BuildPipeline(m_cfgs), 0);

  EXPECT_NE(pipeline.GetModule("test_source"), nullptr);
  EXPECT_NE(pipeline.GetModule("test_infer"), nullptr);

  EXPECT_EQ(pipeline.GetModule(""), nullptr);
}

TEST(CorePipeline, GetLinkIds) {
  Pipeline pipeline("test pipeline");
  std::string file_path = GetExePath() + "../../modules/unitest/core/data/pipeline.json";
  EXPECT_EQ(pipeline.BuildPipelineByJSONFile(file_path), 0);

  std::vector<std::string> links;
  links = pipeline.GetLinkIds();
  EXPECT_EQ(links.size(), uint32_t(4));
}

TEST(CorePipeline, StreamMsgObserver) {
  Pipeline pipeline("test pipeline");
  TestObserver observer;
  pipeline.SetStreamMsgObserver(&observer);
  EXPECT_EQ(pipeline.GetStreamMsgObserver(), &observer);

  StreamMsg msg;
  msg.type = StreamMsgType::ERROR_MSG;
  msg.chn_idx = 0;
  msg.stream_id = "0";
  pipeline.NotifyStreamMsg(msg);
}

TEST(CorePipeline, CreatePerfManager) {
  Pipeline pipeline("test pipeline");
  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node = std::make_shared<TestModule>("down_node");
  // add two modules to the pipeline and link
  EXPECT_TRUE(pipeline.AddModule(up_node));
  EXPECT_TRUE(pipeline.AddModule(down_node));
  std::string link_id = pipeline.LinkModules(up_node, down_node);

  std::vector<std::string> stream_ids = {"0", "1", "2", "3"};
  EXPECT_TRUE(pipeline.CreatePerfManager(stream_ids, gTestPerfDir));

  EXPECT_TRUE(pipeline.Start());
  EXPECT_TRUE(pipeline.Stop());

  EXPECT_TRUE(pipeline.CreatePerfManager(stream_ids, ""));
  EXPECT_TRUE(pipeline.Start());
  // cannot create when perf_running is true
  EXPECT_FALSE(pipeline.CreatePerfManager(stream_ids, ""));
  EXPECT_TRUE(pipeline.Stop());
  // after pipeline stop, we could recreate perf manager
  EXPECT_TRUE(pipeline.CreatePerfManager(stream_ids, ""));
  EXPECT_TRUE(pipeline.Start());
  EXPECT_TRUE(pipeline.Stop());
}

TEST(CorePipeline, CreatePerfManagerFailedCase) {
  Pipeline pipeline1("test pipeline");
  Pipeline pipeline2("test pipeline");
  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node = std::make_shared<TestModule>("down_node");

  // add two modules to the pipeline and link
  EXPECT_TRUE(pipeline1.AddModule(up_node));
  EXPECT_TRUE(pipeline1.AddModule(down_node));
  pipeline1.LinkModules(up_node, down_node);

  EXPECT_TRUE(pipeline2.AddModule(up_node));
  EXPECT_TRUE(pipeline2.AddModule(down_node));
  pipeline2.LinkModules(up_node, down_node);

  std::vector<std::string> stream_ids = {"0", "1", "2", "3"};
  EXPECT_TRUE(pipeline1.CreatePerfManager(stream_ids, gTestPerfDir));

  EXPECT_TRUE(pipeline1.Start());

#ifdef HAVE_SQLITE
  // failed as the db file is opened by pipeline1
  EXPECT_FALSE(pipeline2.CreatePerfManager(stream_ids, gTestPerfDir));
#else
  EXPECT_TRUE(pipeline2.CreatePerfManager(stream_ids, gTestPerfDir));
#endif

  EXPECT_TRUE(pipeline1.Stop());
}

TEST(CorePipeline, PerfTaskLoop) {
  Pipeline pipeline("test pipeline");
  auto up_node = std::make_shared<TestModule>("up_node");
  auto down_node = std::make_shared<TestModule>("down_node");
  auto end_node = std::make_shared<TestModule>("end_node");
  EXPECT_TRUE(pipeline.AddModule(up_node));
  EXPECT_TRUE(pipeline.AddModule(down_node));
  EXPECT_TRUE(pipeline.AddModule(end_node));
  pipeline.LinkModules(up_node, down_node);
  pipeline.LinkModules(down_node, end_node);
  // two linked modules are added to the pipeline
  EXPECT_TRUE(pipeline.Start());
  std::vector<std::string> stream_ids = {"0", "1", "2", "3"};
  EXPECT_TRUE(pipeline.CreatePerfManager(stream_ids, gTestPerfDir));

  uint32_t data_num = 10;
  uint32_t id = 0;
  for (auto it : stream_ids) {
    for (uint32_t i = 0; i < data_num + id * 10; i++) {
      auto data = CNFrameInfo::Create(it);
      data->channel_idx = id;
      data->frame.timestamp = i;
      EXPECT_NO_THROW(pipeline.TransmitData("up_node", data));
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto eos_data = CNFrameInfo::Create(it, true);
    eos_data->channel_idx = id;
    EXPECT_NO_THROW(pipeline.TransmitData("up_node", eos_data));
    id++;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_TRUE(pipeline.Stop());
}

}  // namespace cnstream
