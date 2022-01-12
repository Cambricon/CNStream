/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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
#include <future>
#include <memory>
#include <string>
#include <map>
#include <vector>

#include "cnstream_frame.hpp"
#include "cnstream_graph.hpp"
#include "cnstream_pipeline.hpp"
#include "test_base.hpp"

namespace cnstream {

namespace __test_data_flow__ {

// 1. test frame order
// 2. test process order

using Clock = std::chrono::steady_clock;

std::string GenStreamId() {
  static std::atomic<int64_t> stream_id {0};
  int64_t cur_stream_id = stream_id.fetch_add(1);
  return std::to_string(cur_stream_id);
}

class TestProvider : public ModuleEx, public ModuleCreator<TestProvider> {
 public:
  explicit TestProvider(const std::string& name) : ModuleEx(name) {}
  ~TestProvider() {
    for (auto& th : threads_) th.join();
  }
  bool Open(ModuleParamSet params) override {
    stream_num_ = std::stoi(params["stream_num"]);
    data_num_per_stream_ = std::stoi(params["data_num_per_stream"]);
    return true;
  }
  void Close() override {}
  int Process(CNFrameInfoPtr data) override {return 0;}
  void StartDataLoop() {
    for (int i = 0; i < stream_num_; ++i) {
      threads_.push_back(std::thread(&TestProvider::DataLoop, this));
    }
  }

 private:
  void DataLoop() {
    auto stream_id = GenStreamId();
    auto data_num = data_num_per_stream_;
    int64_t frame_id = 0;
    while (data_num--) {
      auto data = CNFrameInfo::Create(stream_id);
      data->collection.Add("FRAME_ID", frame_id++);
      data->collection.Add(GetName() + "_TS", Clock::now());
      GetContainer()->ProvideData(this, data);
    }
    // eos
    auto data = CNFrameInfo::Create(stream_id, true);
    GetContainer()->ProvideData(this, data);
  }

  int stream_num_ = -1;
  int data_num_per_stream_ = -1;
  std::vector<std::thread> threads_;
};  // class TestProvider

class TestModule : public Module, public ModuleCreator<TestModule> {
 public:
  explicit TestModule(const std::string& name) : Module(name) {}
  bool Open(ModuleParamSet params) override {return true;}
  void Close() override {}
  int Process(CNFrameInfoPtr data) override {
    thread_local std::map<std::string, int64_t> frame_id_map;
    data->collection.Add(GetName() + "_TS", Clock::now());
    // check frame order
    if (frame_id_map.end() == frame_id_map.find(data->stream_id)) {
      frame_id_map[data->stream_id] = -1;
    }
    const int64_t cur_frame_id = data->collection.Get<int64_t>("FRAME_ID");
    const int64_t expected_frame_id = frame_id_map[data->stream_id] + 1;
    if (expected_frame_id != cur_frame_id) {
      Event event;
      event.message = "Frame out of order! Expected frame index is [" + std::to_string(expected_frame_id) +
          "] but get [" + std::to_string(cur_frame_id) + "] in stream [" + data->stream_id + "].";
      event.module_name = GetName();
      event.stream_id = data->stream_id;
      event.type = EventType::EVENT_STREAM_ERROR;
      PostEvent(event);
    }
    frame_id_map[data->stream_id]++;
    return 0;
  }
};  // class TestModule

struct NodeInfo {
  std::vector<std::weak_ptr<CNGraph<NodeInfo>::CNNode>> parents;
};  // struct NodeInfo

class TestFlowPipeline : public Pipeline, public StreamMsgObserver {
  static constexpr int kStreamNum = 8;

 public:
  TestFlowPipeline() : Pipeline("test_pipeline") {}
  enum ExitStatus {
    EXIT_WITH_UNKNOWN_ERROR = -1,
    EXIT_NORMAL = 0,
    EXIT_WITH_FRAME_OUT_OF_ORDER,
    EXIT_WITH_WRONG_FLOW_TIMING
  };  // class ExitStatus
  void Update(const StreamMsg& msg) override {
    switch (msg.type) {
      case StreamMsgType::EOS_MSG: {
        int eosd_num = eos_num_.fetch_add(1);
        if (eosd_num + 1 == static_cast<int>(graph_.GetHeads().size()) * kStreamNum)
          exit_status_.set_value(EXIT_NORMAL);
        break;
      }
      case StreamMsgType::STREAM_ERR_MSG:
        exit_status_.set_value(EXIT_WITH_FRAME_OUT_OF_ORDER);
        break;
      case StreamMsgType::ERROR_MSG:
        exit_status_.set_value(EXIT_WITH_WRONG_FLOW_TIMING);
        break;
      default:
        ASSERT_TRUE(false) << "Error happened.";
        exit_status_.set_value(EXIT_WITH_UNKNOWN_ERROR);
        break;
    }
  }
  void Init(const std::vector<std::vector<bool>>& adj_matrix) {
    // make sure your adjacency matrix is valid.
    const int vertex_num = static_cast<int>(adj_matrix.size());
    std::vector<int> indegrees(vertex_num, 0);
    std::vector<int> outdegrees(vertex_num, 0);
    for (int i = 0; i < vertex_num; ++i) {
      for (int j = 0; j < vertex_num; ++j) {
        if (adj_matrix[i][j]) {
          indegrees[j]++;
          outdegrees[i]++;
        }
      }
    }
    CNGraphConfig graph_config;
    graph_config.name = "test_pipeline";
    for (int i = 0; i < vertex_num; ++i) {
      CNModuleConfig config;
      config.name = std::to_string(i);
      if (indegrees[i]) {
        // not head
        config.className = "cnstream::__test_data_flow__::TestModule";
      } else {
        // head
        config.className = "cnstream::__test_data_flow__::TestProvider";
        config.parameters["stream_num"] = std::to_string(kStreamNum);
        config.parameters["data_num_per_stream"] = "200";
      }
      if (outdegrees[i]) {
        // not leaf
        for (int j = 0; j < vertex_num; ++j) {
          if (adj_matrix[i][j]) {
            config.next.insert(std::to_string(j));
          }
        }
      } else {
        // leaf
        config.next.insert("tschecker");
      }
      config.maxInputQueueSize = 20;
      config.parallelism = kStreamNum / 3;
      graph_config.module_configs.push_back(config);
    }
    CNModuleConfig ts_checker_config;
    ts_checker_config.name = "tschecker";
    ts_checker_config.className = "cnstream::__test_data_flow__::TSChecker";
    ts_checker_config.parallelism = kStreamNum / 3;
    ts_checker_config.maxInputQueueSize = 20;
    graph_config.module_configs.push_back(ts_checker_config);
    graph_config.profiler_config.enable_tracing = true;
    graph_config.profiler_config.enable_profiling = true;
    ASSERT_TRUE(graph_.Init(graph_config));
  }
  void StartDataFlow() {
    SetStreamMsgObserver(this);
    ASSERT_TRUE(BuildPipeline(graph_.GetConfig()));
    ASSERT_TRUE(Start());
    for (auto head : graph_.GetHeads()) {
      dynamic_cast<TestProvider*>(GetModule(head->GetFullName()))->StartDataLoop();
    }
  }
  ExitStatus WaitForStop() {
    auto ret = exit_status_.get_future().get();
    Stop();
    return ret;
  }
  const CNGraph<NodeInfo>& GetGraph() const {
    return graph_;
  }

 private:
  std::atomic<int> eos_num_ {0};
  std::promise<ExitStatus> exit_status_;
  CNGraph<NodeInfo> graph_;
};  // class TestFlowPipeline

class TSChecker : public Module, public ModuleCreator<TSChecker> {
 public:
  explicit TSChecker(const std::string& name) : Module(name) {}
  bool Open(ModuleParamSet params) {return true;}
  void Close() {}
  int Process(CNFrameInfoPtr data) {
    data->collection.Add(GetName() + "_TS", Clock::now());
    for (auto iter = GetGraph().DFSBegin(); iter != GetGraph().DFSEnd(); ++iter) {
      // Determine whether the timestamp of the current node is greater than
      // the timestamps of all the parent nodes
      if (!iter->data.parents.size()) continue;  // head node
      auto cur_ts = data->collection.Get<Clock::time_point>(iter->GetFullName() + "_TS");
      for (auto parent : iter->data.parents) {
        auto parent_ts = data->collection.Get<Clock::time_point>(parent.lock()->GetFullName() + "_TS");
        if (cur_ts <= parent_ts) {
          Event event;
          event.message = "Computational flow timing is out of order. Node [" +
              parent.lock()->GetFullName() + "] do process after node [" +
              GetName() + "].";
          event.module_name = GetName();
          event.stream_id = data->stream_id;
          event.type = EventType::EVENT_ERROR;
          PostEvent(event);
        }
      }
    }
    return 0;
  }

 private:
  const CNGraph<NodeInfo>& GetGraph() const {
    return dynamic_cast<TestFlowPipeline*>(GetContainer())->GetGraph();
  }
};  // class TSChecker

TestFlowPipeline::ExitStatus TestDataFlow(const std::vector<std::vector<bool>>& adj_matrix) {
  TestFlowPipeline pipeline;
  pipeline.Init(adj_matrix);
  pipeline.StartDataFlow();
  return pipeline.WaitForStop();
}

}  // namespace __test_data_flow__

TEST(CoreTestDataFlow, OneSource) {
  /**
   * one source
   *       0
   *      / \
   *     1   2
   *    /   / \
   *   3   4   5
   *    \     /
   *     \   /
   *       6
   **/
  std::vector<std::vector<bool>> adj_matrix = {
    {false, true,  true,  false, false, false, false},
    {false, false, false, true,  false, false, false},
    {false, false, false, false, true,  true,  false},
    {false, false, false, false, false, false, true},
    {false, false, false, false, false, false, false},
    {false, false, false, false, false, false, true},
    {false, false, false, false, false, false, false}
  };

  auto exit_status = __test_data_flow__::TestDataFlow(adj_matrix);
  EXPECT_EQ(__test_data_flow__::TestFlowPipeline::EXIT_NORMAL, exit_status)
      << "Test data flow with one source failed, exit status [" << exit_status << "].";
}

TEST(CoreTestDataFlow, TwoSource) {
  /**
   * two source
   *       0   7
   *      / \ /
   *     1   2
   *    /   / \
   *   3   4   5
   *    \     /
   *     \   /
   *       6
   **/
  std::vector<std::vector<bool>> adj_matrix = {
    {false, true,  true,  false, false, false, false, false},
    {false, false, false, true,  false, false, false, false},
    {false, false, false, false, true,  true,  false, false},
    {false, false, false, false, false, false, true,  false},
    {false, false, false, false, false, false, false, false},
    {false, false, false, false, false, false, true,  false},
    {false, false, false, false, false, false, false, false},
    {false, false, true,  false, false, false, false, false}
  };

  auto exit_status = __test_data_flow__::TestDataFlow(adj_matrix);
  EXPECT_EQ(__test_data_flow__::TestFlowPipeline::EXIT_NORMAL, exit_status)
      << "Test data flow with one source failed, exit status [" << exit_status << "].";
}

namespace __test_flow_failed__ {
class TestFailedModule : public Module, public ModuleCreator<TestFailedModule> {
 public:
  explicit TestFailedModule(const std::string& name) : Module(name) {}
  bool Open(ModuleParamSet params) override {
    test_mode_ = params["test_mode"];
    return true;
  }
  void Close() override {}
  int Process(std::shared_ptr<CNFrameInfo> data) override {
    if ("process_failed" == test_mode_) {
      return -1;
    } else if ("invalid_data" == test_mode_) {
      data->flags = static_cast<size_t>(CNFrameFlag::CN_FRAME_FLAG_INVALID);
    }
    return 0;
  }

 private:
  std::string test_mode_;  // process_failed/invalid_data
};  // class TestFailedModule
struct TestFailedObserver : public StreamMsgObserver {
 public:
  void Update(const StreamMsg& msg) override {
    switch (msg.type) {
      case StreamMsgType::EOS_MSG: {
        wait_for_stop.set_value();
        break;
      }
      case StreamMsgType::FRAME_ERR_MSG:
        received_invalid_data = true;
        break;
      case StreamMsgType::ERROR_MSG:
        received_process_failed = true;
        break;
      default:
        ASSERT_TRUE(false) << "Error happened.";
        wait_for_stop.set_value();
        break;
    }
  }
  std::promise<void> wait_for_stop;
  bool received_process_failed = false;
  bool received_invalid_data = false;
};  // class TestFailedObserver
}  // namespace __test_flow_failed__

TEST(CoreTestDataFlow, ProcessFailed) {
  CNGraphConfig graph_config;
  graph_config.name = "test_pipeline";
  CNModuleConfig provider_config;
  provider_config.name = "test_provider";
  provider_config.className = "cnstream::__test_data_flow__::TestProvider";
  provider_config.parameters["stream_num"] = "1";
  provider_config.parameters["data_num_per_stream"] = "1";
  provider_config.next.insert("test_failed");
  graph_config.module_configs.push_back(provider_config);
  CNModuleConfig failed_module_config;
  failed_module_config.name = "test_failed";
  failed_module_config.className = "cnstream::__test_flow_failed__::TestFailedModule";
  failed_module_config.parallelism = 1;
  failed_module_config.maxInputQueueSize = 20;
  failed_module_config.parameters["test_mode"] = "process_failed";
  graph_config.module_configs.push_back(failed_module_config);
  Pipeline pipeline("test_pipeline");
  __test_flow_failed__::TestFailedObserver observer;
  pipeline.SetStreamMsgObserver(&observer);
  EXPECT_TRUE(pipeline.BuildPipeline(graph_config));
  EXPECT_TRUE(pipeline.Start());
  dynamic_cast<__test_data_flow__::TestProvider*>(pipeline.GetModule("test_provider"))->StartDataLoop();
  observer.wait_for_stop.get_future().wait();
  pipeline.Stop();
  EXPECT_TRUE(observer.received_process_failed);
}

TEST(CoreTestDataFlow, InvalidData) {
  CNGraphConfig graph_config;
  graph_config.name = "test_pipeline";
  CNModuleConfig provider_config;
  provider_config.name = "test_provider";
  provider_config.className = "cnstream::__test_data_flow__::TestProvider";
  provider_config.parameters["stream_num"] = "1";
  provider_config.parameters["data_num_per_stream"] = "1";
  provider_config.next.insert("test_failed");
  graph_config.module_configs.push_back(provider_config);
  CNModuleConfig failed_module_config;
  failed_module_config.name = "test_failed";
  failed_module_config.className = "cnstream::__test_flow_failed__::TestFailedModule";
  failed_module_config.parallelism = 1;
  failed_module_config.maxInputQueueSize = 20;
  failed_module_config.parameters["test_mode"] = "invalid_data";
  graph_config.module_configs.push_back(failed_module_config);
  Pipeline pipeline("test_pipeline");
  __test_flow_failed__::TestFailedObserver observer;
  pipeline.SetStreamMsgObserver(&observer);
  EXPECT_TRUE(pipeline.BuildPipeline(graph_config));
  EXPECT_TRUE(pipeline.Start());
  dynamic_cast<__test_data_flow__::TestProvider*>(pipeline.GetModule("test_provider"))->StartDataLoop();
  observer.wait_for_stop.get_future().wait();
  pipeline.Stop();
  EXPECT_TRUE(observer.received_invalid_data);
}

}  // namespace cnstream
