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
#include <utility>
#include <vector>
#include "cnstream_frame.hpp"
#include "cnstream_pipeline.hpp"

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

class MsgObserver : cnstream::StreamMsgObserver {
 public:
  enum StopFlag { STOP_BY_EOS = 0, STOP_BY_ERROR };
  MsgObserver(int chn_cnt, std::shared_ptr<cnstream::Pipeline> pipeline) : chn_cnt_(chn_cnt), pipeline_(pipeline) {}

  void Update(const cnstream::StreamMsg& smsg) override {
    if (stop_) return;
    if (smsg.type == cnstream::StreamMsgType::EOS_MSG) {
      LOG(INFO) << "[Observer] received EOS_MSG from channel " << smsg.chn_idx;
      EXPECT_EQ(std::find(eos_chn_.begin(), eos_chn_.end(), smsg.chn_idx), eos_chn_.end())
          << smsg.chn_idx << " " << eos_chn_.size() << " " << chn_cnt_;
      eos_chn_.push_back(smsg.chn_idx);
      if (static_cast<int>(eos_chn_.size()) == chn_cnt_) {
        stop_ = true;
        wakener_.set_value(STOP_BY_EOS);
      }
    } else if (smsg.type == cnstream::StreamMsgType::ERROR_MSG) {
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
  std::shared_ptr<cnstream::Pipeline> pipeline_ = nullptr;
  std::vector<int> eos_chn_;
  bool stop_ = false;
  std::promise<StopFlag> wakener_;
};

static const int __MIN_CHN_CNT__ = 1;
static const int __MAX_CHN_CNT__ = 124;

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

class TestProcessor : public cnstream::Module {
 public:
  explicit TestProcessor(const std::string& name, int chns) : Module(name) { cnts_.resize(chns); }
  bool Open(cnstream::ModuleParamSet param_set) override {
    opened_ = true;
    return true;
  }
  void Close() override { opened_ = false; }
  int Process(std::shared_ptr<cnstream::CNFrameInfo> data) override {
    EXPECT_EQ(true, opened_);
    EXPECT_NE(1, cnstream::CNFrameFlag::CN_FRAME_FLAG_EOS & data->frame.flags);
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
    std::uniform_int_distribution<> failure_frame_randomer(0, __MIN_FRAME_CNT__);
    std::uniform_int_distribution<> failure_chn_randomer(1, chns - 1);
    failure_chn_ = failure_chn_randomer(e_);
    failure_frame_ = failure_frame_randomer(e_);
  }
  int Process(std::shared_ptr<cnstream::CNFrameInfo> data) override {
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
  int failure_ret_num_ = 0;
};  // class TestProcessorFailure

class TestProvider : public TestProcessor {
 public:
  explicit TestProvider(int chns, cnstream::Pipeline* pipeline)
      : TestProcessor("TestProvider", chns), pipeline_(pipeline) {
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
      auto data = cnstream::CNFrameInfo::Create(std::to_string(chn_idx));
      data->channel_idx = chn_idx;
      data->frame.frame_id = frame_idx++;

      if (!pipeline_->ProvideData(this, data)) {
        return;
      }
      if (frame_cnt == 0) {
        data = cnstream::CNFrameInfo::Create(std::to_string(chn_idx));
        data->channel_idx = chn_idx;
        data->frame.flags |= cnstream::CN_FRAME_FLAG_EOS;
        pipeline_->ProvideData(this, data);
        LOG(INFO) << "Send EOS:" << chn_idx << " frame id :" << frame_idx;
      }
    }
  }
  std::vector<std::thread> threads_;
  std::vector<uint64_t> frame_cnts_;
  cnstream::Pipeline* pipeline_ = nullptr;
};  // class TestProvider

struct FailureDesc {
  FailureDesc(int failure_midx, int ret) : failure_module_idx(failure_midx), process_ret(ret) {}
  int failure_module_idx = -1;
  int process_ret = -1;
};

std::pair<std::vector<std::shared_ptr<cnstream::Module>>, std::shared_ptr<cnstream::Pipeline>>
CreatePipelineByNeighborList(const std::vector<std::list<int>>& neighbor_list, FailureDesc fdesc = {-1, -1}) {
  std::default_random_engine e(time(NULL));
  std::uniform_int_distribution<> chns_randomer(__MIN_CHN_CNT__, __MAX_CHN_CNT__);
  auto chns = chns_randomer(e);
  auto pipeline = std::make_shared<cnstream::Pipeline>("pipeline");
  std::vector<std::shared_ptr<cnstream::Module>> modules;
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
  pipeline->SetModuleParallelism(modules[0], 0);
  thread_nums.push_back(0);
  for (size_t i = 1; i < modules.size(); i++) {
    uint32_t thread_num = ths_randomer(e);
    EXPECT_TRUE(pipeline->SetModuleParallelism(modules[i], thread_num));
    // EXPECT_TRUE(pipeline->SetModuleParallelism(modules[i],
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
  pipeline->SetStreamMsgObserver(reinterpret_cast<cnstream::StreamMsgObserver*>(&msg_observer));

  pipeline->Start();
  provider->StartSendData();

  EXPECT_EQ(MsgObserver::STOP_BY_EOS, msg_observer.WaitForStop());
  provider->StopSendData();

  for (size_t i = 1; i < modules.size(); ++i) {
    auto processor = dynamic_cast<TestProcessor*>(modules[i].get());
    EXPECT_TRUE(nullptr != processor);
    for (size_t i = 1; i < processor->GetCnts().size(); ++i) {
      EXPECT_EQ(provider->GetFrameCnts()[i], processor->GetCnts()[i]);
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
  pipeline->SetStreamMsgObserver(reinterpret_cast<cnstream::StreamMsgObserver*>(&msg_observer));

  pipeline->Start();
  provider->StartSendData();

  EXPECT_EQ(MsgObserver::STOP_BY_ERROR, msg_observer.WaitForStop());
  provider->StopSendData();
}

TEST(CorePipeline, Pipeline) {
  for (auto& it : g_neighbor_lists) {
    TestProcess(it);
    TestProcessFailure(it, -1);
  }
  TestProcess(g_neighbor_lists[0]);
}
