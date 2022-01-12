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

#include <assert.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <map>
#include <utility>
#include <vector>

#include "cnstream_graph.hpp"
#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"
#include "connector.hpp"
#include "conveyor.hpp"
#include "profiler/module_profiler.hpp"
#include "profiler/pipeline_profiler.hpp"
#include "util/cnstream_queue.hpp"

namespace cnstream {

/**
 * @brief The node context used by pipeline.
 */
struct NodeContext {
  std::shared_ptr<Module> module;
  std::shared_ptr<Connector> connector;
  uint64_t parent_nodes_mask = 0;
  uint64_t route_mask = 0;  // for head nodes
  // for gets node instance by a module, see Module::context_;
  std::weak_ptr<CNGraph<NodeContext>::CNNode> node;
};

Pipeline::Pipeline(const std::string& name) : name_(name) {
  // stream message handle thread
  exit_msg_loop_ = false;
  smsg_thread_ = std::thread(&Pipeline::StreamMsgHandleFunc, this);

  event_bus_.reset(new (std::nothrow) EventBus());
  LOGF_IF(CORE, nullptr == event_bus_) << "Pipeline::Pipeline() failed to alloc EventBus";
  GetEventBus()->AddBusWatch(std::bind(&Pipeline::DefaultBusWatch, this, std::placeholders::_1));

  idxManager_.reset(new (std::nothrow) IdxManager());
  LOGF_IF(CORE, nullptr == idxManager_) << "Pipeline::Pipeline() failed to alloc IdxManager";

  graph_.reset(new (std::nothrow) CNGraph<NodeContext>());
  LOGF_IF(CORE, nullptr == graph_) << "Pipeline::Pipeline() failed to alloc CNGraph";
}

Pipeline::~Pipeline() {
  running_ = false;
  exit_msg_loop_ = true;
  if (smsg_thread_.joinable()) {
    smsg_thread_.join();
  }
  event_bus_.reset();
  graph_.reset();  // must release before idxManager_;
  idxManager_.reset();
}

bool Pipeline::BuildPipeline(const CNGraphConfig& graph_config) {
  auto t = graph_config;
  t.name = GetName();
  if (!graph_->Init(t)) {
    LOGE(CORE) << "Init graph failed.";
    return false;
  }
  // create modules by config
  if (!CreateModules()) {
    LOGE(CORE) << "Create modules failed.";
    return false;
  }
  // generate parant mask for all nodes and route mask for head nodes.
  GenerateModulesMask();
  // create connectors for all nodes beside head nodes.
  // This call must after GenerateModulesMask called,
  // then we can determine witch are the head nodes.
  return CreateConnectors();
}

bool Pipeline::Start() {
  if (IsRunning()) {
    LOGW(CORE) << "Pipeline is running, the Pipeline::Start function is called multiple times.";
    return false;
  }

  // open modules
  bool open_module_failed = false;
  std::vector<std::shared_ptr<Module>> opened_modules;
  for (auto node = graph_->DFSBegin(); node != graph_->DFSEnd(); ++node) {
    if (!node->data.module->Open(node->GetConfig().parameters)) {
      LOGE(CORE) << node->data.module->GetName() << " open failed!";
      open_module_failed = true;
      break;
    }
    opened_modules.push_back(node->data.module);
  }
  if (open_module_failed) {
    for (auto it : opened_modules) it->Close();
    return false;
  }

  running_.store(true);
  event_bus_->Start();

  // start data transmit
  for (auto node = graph_->DFSBegin(); node != graph_->DFSEnd(); ++node) {
    if (!node->data.parent_nodes_mask) continue;  // head node
    node->data.connector->Start();
  }

  // create process threads
  for (auto node = graph_->DFSBegin(); node != graph_->DFSEnd(); ++node) {
    if (!node->data.parent_nodes_mask) continue;  // head node
    const auto& config = node->GetConfig();
    for (int conveyor_idx = 0; conveyor_idx < config.parallelism; ++conveyor_idx) {
      threads_.push_back(std::thread(&Pipeline::TaskLoop, this, &node->data, conveyor_idx));
    }
  }
  LOGI(CORE) << "Pipeline[" << GetName() << "] " << "Start";
  return true;
}

bool Pipeline::Stop() {
  if (!IsRunning()) return true;

  // stop data transmit
  for (auto node = graph_->DFSBegin(); node != graph_->DFSEnd(); ++node) {
    if (!node->data.parent_nodes_mask) continue;  // head node
    auto connector = node->data.connector;
    if (connector) {
      // push data will be rejected after Stop()
      // stop first to ensure connector will be empty
      connector->Stop();
      connector->EmptyDataQueue();
    }
  }
  running_.store(false);
  for (std::thread& it : threads_) {
    if (it.joinable()) it.join();
  }
  threads_.clear();
  event_bus_->Stop();

  // close modules
  for (auto node = graph_->DFSBegin(); node != graph_->DFSEnd(); ++node) {
    node->data.module->Close();
  }

  // clear callback function, important! Especially for the case of using the python api,
  // the callback function will manage the life cycle of a python object.
  // When a circular reference occurs, GC(python) cannot handle it, resulting in a memory leak.
  RegisterFrameDoneCallBack(NULL);
  LOGI(CORE) << "Pipeline[" << GetName() << "] " << "Stop";
  return true;
}

Module* Pipeline::GetModule(const std::string& module_name) const {
  auto node = graph_->GetNodeByName(module_name);
  if (node.get()) return node->data.module.get();
  return nullptr;
}

CNModuleConfig Pipeline::GetModuleConfig(const std::string& module_name) const {
  auto node = graph_->GetNodeByName(module_name);
  if (node.get()) return node->GetConfig();
  return {};
}

bool Pipeline::ProvideData(const Module* module, std::shared_ptr<CNFrameInfo> data) {
  // check running.
  if (!IsRunning()) {
    LOGE(CORE) << "[" << module->GetName() << "]" << " Provide data to pipeline [" << GetName() << "] failed, "
        << "pipeline is not running, start pipeline first. " << data->stream_id;
    return false;
  }
  // check module is created by current pipeline.
  if (!module || module->GetContainer() != this) {
    LOGE(CORE) << "Provide data to pipeline [" << GetName() << "] failed, "
        << (module ? ("module named [" + module->GetName() + "] is not created by current pipeline.") :
        "module can not be nullptr.");
    return false;
  }
  // data can only created by root nodes.
  if (!data->GetModulesMask() && module->context_->parent_nodes_mask) {
    LOGE(CORE) << "Provide data to pipeline [" << GetName() << "] failed, "
        << "Data created by module named [" << module->GetName() << "]. "
        << "Data can be provided to pipeline only when the data is created by root nodes.";
    return false;
  }
  TransmitData(module->context_, data);
  return true;
}

bool Pipeline::IsRootNode(const std::string& module_name) const {
  auto module = GetModule(module_name);
  if (!module) return false;
  return !module->context_->parent_nodes_mask;
}

bool Pipeline::IsLeafNode(const std::string& module_name) const {
  auto module = GetModule(module_name);
  if (!module) return false;
  return module->context_->node.lock()->GetNext().empty();
}

bool Pipeline::CreateModules() {
  std::vector<std::shared_ptr<Module>> modules;  // used to init profiler

  all_modules_mask_ = 0;
  for (auto node_iter = graph_->DFSBegin(); node_iter != graph_->DFSEnd(); ++node_iter) {
    const CNModuleConfig& config = node_iter->GetConfig();
    // use GetFullName with a graph name prefix to create modules to prevent nodes with the same name in subgraphs.
    Module* module = ModuleFactory::Instance()->Create(config.className, node_iter->GetFullName());
    if (!module) {
      LOGE(CORE) << "Create module failed, module name : [" << config.name
          << "], class name : [" << config.className << "].";
      return false;
    }
    module->context_ = &node_iter->data;
    node_iter->data.node = *node_iter;
    node_iter->data.parent_nodes_mask = 0;
    node_iter->data.route_mask = 0;
    node_iter->data.module = std::shared_ptr<Module>(module);
    node_iter->data.module->SetContainer(this);
    modules.push_back(node_iter->data.module);
    all_modules_mask_ |= 1UL << node_iter->data.module->GetId();
  }

  profiler_.reset(new PipelineProfiler(graph_->GetConfig().profiler_config, GetName(), modules,
      GetSortedModuleNames()));
  return true;
}

std::vector<std::string> Pipeline::GetSortedModuleNames() {
  if (sorted_module_names_.empty()) {
    sorted_module_names_ = graph_->TopoSort();
  }
  return sorted_module_names_;
}

void Pipeline::GenerateModulesMask() {
  // parent mask helps to determine whether the data has passed all the parent nodes.
  for (auto cur_node = graph_->DFSBegin(); cur_node != graph_->DFSEnd(); ++cur_node) {
    const auto& next_nodes = cur_node->GetNext();
    for (const auto& next : next_nodes) {
      next->data.parent_nodes_mask |= 1UL << cur_node->data.module->GetId();
    }
  }

  // route mask helps to mark that the data has passed through all unreachable nodes.
  // consider the case of multiple head nodes. (multiple source modules)
  for (auto head : graph_->GetHeads()) {
    for (auto iter = head->DFSBegin(); iter != head->DFSEnd(); ++iter) {
      head->data.route_mask |= 1UL << iter->data.module->GetId();
    }
  }
}

bool Pipeline::CreateConnectors() {
  for (auto node_iter = graph_->DFSBegin(); node_iter != graph_->DFSEnd(); ++node_iter) {
    if (node_iter->data.parent_nodes_mask)  {  // not a head node
      const auto &config = node_iter->GetConfig();
      // check if parallelism and max_input_queue_size is valid.
      if (config.parallelism <= 0 || config.maxInputQueueSize <= 0) {
        LOGE(CORE) << "Module [" << config.name << "]: parallelism or max_input_queue_size is not valid, "
                   "parallelism[" << config.parallelism << "], "
                   "max_input_queue_size[" << config.maxInputQueueSize << "].";
        return false;
      }
      node_iter->data.connector = std::make_shared<Connector>(config.parallelism, config.maxInputQueueSize);
    }
  }
  return true;
}

static inline
bool PassedByAllParentNodes(NodeContext* context, uint64_t data_mask) {
  uint64_t parent_masks = context->parent_nodes_mask;
  return (data_mask & parent_masks) == parent_masks;
}

void Pipeline::OnProcessStart(NodeContext* context, const std::shared_ptr<CNFrameInfo>& data) {
  if (data->IsEos()) return;
  if (IsProfilingEnabled()) {
    auto record_key = std::make_pair(data->stream_id, data->timestamp);
    auto profiler = context->module->GetProfiler();
    profiler->RecordProcessEnd(kINPUT_PROFILER_NAME, record_key);
    profiler->RecordProcessStart(kPROCESS_PROFILER_NAME, record_key);
  }
}

void Pipeline::OnProcessEnd(NodeContext* context, const std::shared_ptr<CNFrameInfo>& data) {
  if (IsProfilingEnabled())
    context->module->GetProfiler()->RecordProcessEnd(kPROCESS_PROFILER_NAME,
        std::make_pair(data->stream_id, data->timestamp));
  context->module->NotifyObserver(data);
}

void Pipeline::OnProcessFailed(NodeContext* context, const std::shared_ptr<CNFrameInfo>& data, int ret) {
  auto module_name = context->module->GetName();
  Event e;
  e.type = EventType::EVENT_ERROR;
  e.module_name = module_name;
  e.message = module_name + " process failed, return number: " + std::to_string(ret);
  e.stream_id = data->stream_id;
  e.thread_id = std::this_thread::get_id();
  event_bus_->PostEvent(e);
}

void Pipeline::OnDataInvalid(NodeContext* context, const std::shared_ptr<CNFrameInfo>& data) {
  auto module = context->module;
  LOGW(CORE) << "[" << GetName() << "]" << " got frame error from " << module->GetName() <<
    " stream_id: " << data->stream_id << ", pts: " << data->timestamp;
  StreamMsg msg;
  msg.type = StreamMsgType::FRAME_ERR_MSG;
  msg.stream_id = data->stream_id;
  msg.module_name = module->GetName();
  msg.pts = data->timestamp;
  UpdateByStreamMsg(msg);
}

void Pipeline::OnEos(NodeContext* context, const std::shared_ptr<CNFrameInfo>& data) {
  auto module = context->module;
  module->NotifyObserver(data);
  if (IsProfilingEnabled())
    module->GetProfiler()->OnStreamEos(data->stream_id);
  LOGI(CORE) << "[" << module->GetName() << "]"
      << " [" << data->stream_id << "] got eos.";
  // eos message
  Event e;
  e.type = EventType::EVENT_EOS;
  e.module_name = module->GetName();
  e.stream_id = data->stream_id;
  e.thread_id = std::this_thread::get_id();
  event_bus_->PostEvent(e);
}

void Pipeline::OnPassThrough(const std::shared_ptr<CNFrameInfo>& data) {
  if (frame_done_cb_) frame_done_cb_(data);  // To notify the frame is processed by all modules
  if (data->IsEos()) {
    StreamMsg msg;
    msg.type = StreamMsgType::EOS_MSG;
    msg.stream_id = data->stream_id;
    UpdateByStreamMsg(msg);
    if (IsProfilingEnabled()) profiler_->OnStreamEos(data->stream_id);
  } else {
    if (IsProfilingEnabled()) profiler_->RecordOutput(
        std::make_pair(data->stream_id, data->timestamp));
  }
}

void Pipeline::TransmitData(NodeContext* context, const std::shared_ptr<CNFrameInfo>& data) {
  if (data->IsInvalid()) {
    OnDataInvalid(context, data);
    return;
  }
  if (!context->parent_nodes_mask) {
    // root node
    // set mask to 1 for never touched modules, for case which has multiple source modules.
    data->SetModulesMask(all_modules_mask_ ^ context->route_mask);
  }
  if (data->IsEos()) {
    OnEos(context, data);
  } else {
    OnProcessEnd(context, data);
    if (IsStreamRemoved(data->stream_id))
      return;
  }

  auto node = context->node.lock();
  auto module = context->module;
  const uint64_t cur_mask = data->MarkPassed(module.get());
  const bool passed_by_all_modules = PassedByAllModules(cur_mask);

  if (passed_by_all_modules) {
    OnPassThrough(data);
    return;
  }

  // transmit to next nodes
  for (auto next_node : node->GetNext()) {
    if (!PassedByAllParentNodes(&next_node->data, cur_mask)) continue;
    auto next_module = next_node->data.module;
    auto connector = next_node->data.connector;
    // push data to conveyor only after data passed by all parent nodes.
    if (IsProfilingEnabled() && !data->IsEos())
      next_module->GetProfiler()->RecordProcessStart(kINPUT_PROFILER_NAME,
          std::make_pair(data->stream_id, data->timestamp));
    const int conveyor_idx = data->GetStreamIndex() % connector->GetConveyorCount();
    while (!connector->IsStopped() && connector->PushDataBufferToConveyor(conveyor_idx, data) == false) {
      if (connector->GetFailTime(conveyor_idx) % 50 == 0) {
        // Show infomation when conveyor is full in every second
        LOGD(CORE) << "[" << next_module->GetName() << " " << conveyor_idx << "] " << "Input buffer is full";
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }  // while try push
  }  // loop next nodes
}

void Pipeline::TaskLoop(NodeContext* context, uint32_t conveyor_idx) {
  auto module = context->module;
  auto connector = context->connector;
  auto node_name = module->GetName();

  // process loop
  while (1) {
    std::shared_ptr<CNFrameInfo> data = nullptr;
    // pull data from conveyor
    while (!connector->IsStopped() && data == nullptr)
      data = connector->PopDataBufferFromConveyor(conveyor_idx);
    if (connector->IsStopped())
      break;
    if (data == nullptr)
      continue;
    OnProcessStart(context, data);
    int ret = module->DoProcess(data);
    if (ret < 0)
      OnProcessFailed(context, data, ret);
  }  // while process loop
}

EventHandleFlag Pipeline::DefaultBusWatch(const Event& event) {
  StreamMsg smsg;
  EventHandleFlag ret;
  switch (event.type) {
    case EventType::EVENT_ERROR:
      smsg.type = StreamMsgType::ERROR_MSG;
      smsg.module_name = event.module_name;
      smsg.stream_id = event.stream_id;
      UpdateByStreamMsg(smsg);
      LOGE(CORE) << "[" << event.module_name << "]: "
                 << event.message;
      ret = EventHandleFlag::EVENT_HANDLE_STOP;
      break;
    case EventType::EVENT_WARNING:
      LOGW(CORE) << "[" << event.module_name << "]: "
                 << event.message;
      ret = EventHandleFlag::EVENT_HANDLE_SYNCED;
      break;
    case EventType::EVENT_STOP:
      LOGI(CORE) << "[" << event.module_name << "]: "
                 << event.message;
      ret = EventHandleFlag::EVENT_HANDLE_STOP;
      break;
    case EventType::EVENT_EOS: {
      LOGD(CORE) << "Pipeline received eos from module " + event.module_name << " of stream " << event.stream_id;
      ret = EventHandleFlag::EVENT_HANDLE_SYNCED;
      break;
    }
    case EventType::EVENT_STREAM_ERROR: {
      smsg.type = StreamMsgType::STREAM_ERR_MSG;
      smsg.module_name = event.module_name;
      smsg.stream_id = event.stream_id;
      UpdateByStreamMsg(smsg);
      LOGD(CORE) << "Pipeline received stream error from module " + event.module_name
                 << " of stream " << event.stream_id;
      ret = EventHandleFlag::EVENT_HANDLE_SYNCED;
      break;
    }
    case EventType::EVENT_INVALID:
      LOGE(CORE) << "[" << event.module_name << "]: "
                 << event.message;
    default:
      ret = EventHandleFlag::EVENT_HANDLE_NULL;
      break;
  }
  return ret;
}

void Pipeline::UpdateByStreamMsg(const StreamMsg& msg) {
  LOGD(CORE) << "[" << GetName() << "] "
             << "stream: " << msg.stream_id << " got message: " << static_cast<std::size_t>(msg.type);
  msgq_.Push(msg);
}

void Pipeline::StreamMsgHandleFunc() {
  while (!exit_msg_loop_) {
    StreamMsg msg;
    while (!exit_msg_loop_ && !msgq_.WaitAndTryPop(msg, std::chrono::milliseconds(200))) {
    }

    if (exit_msg_loop_) {
        LOGI(CORE) << "[" << GetName() << "] stop updating stream message";
        return;
    }
    switch (msg.type) {
      case StreamMsgType::EOS_MSG:
      case StreamMsgType::ERROR_MSG:
      case StreamMsgType::STREAM_ERR_MSG:
      case StreamMsgType::FRAME_ERR_MSG:
      case StreamMsgType::USER_MSG0:
      case StreamMsgType::USER_MSG1:
      case StreamMsgType::USER_MSG2:
      case StreamMsgType::USER_MSG3:
      case StreamMsgType::USER_MSG4:
      case StreamMsgType::USER_MSG5:
      case StreamMsgType::USER_MSG6:
      case StreamMsgType::USER_MSG7:
      case StreamMsgType::USER_MSG8:
      case StreamMsgType::USER_MSG9:
        LOGD(CORE) << "[" << GetName() << "] "
                   << "stream: " << msg.stream_id << " notify message: " << static_cast<std::size_t>(msg.type);
        if (smsg_observer_) {
          smsg_observer_->Update(msg);
        }
        break;
      default:
        break;
    }
  }
}

uint32_t GetMaxStreamNumber() { return MAX_STREAM_NUM; }

uint32_t GetMaxModuleNumber() {
  /*maxModuleIdNum is sizeof(module_id_mask_) * 8  (bytes->bits)*/
  return sizeof(uint64_t) * 8;
}

uint32_t IdxManager::GetStreamIndex(const std::string& stream_id) {
  std::lock_guard<std::mutex> guard(id_lock);
  auto search = stream_idx_map.find(stream_id);
  if (search != stream_idx_map.end()) {
    return search->second;
  }

  for (uint32_t i = 0; i < GetMaxStreamNumber(); i++) {
    if (!stream_bitset[i]) {
      stream_bitset.set(i);
      stream_idx_map[stream_id] = i;
      return i;
    }
  }
  return INVALID_STREAM_IDX;
}

void IdxManager::ReturnStreamIndex(const std::string& stream_id) {
  std::lock_guard<std::mutex> guard(id_lock);
  auto search = stream_idx_map.find(stream_id);
  if (search == stream_idx_map.end()) {
    return;
  }
  uint32_t stream_idx = search->second;
  if (stream_idx >= GetMaxStreamNumber()) {
    return;
  }
  stream_bitset.reset(stream_idx);
  stream_idx_map.erase(search);
}

size_t IdxManager::GetModuleIdx() {
  std::lock_guard<std::mutex>  guard(id_lock);
  for (size_t i = 0; i < GetMaxModuleNumber(); i++) {
    if (!(module_id_mask_ & ((uint64_t)1 << i))) {
      module_id_mask_ |= (uint64_t)1 << i;
      return i;
    }
  }
  return INVALID_MODULE_ID;
}

void IdxManager::ReturnModuleIdx(size_t id_) {
  std::lock_guard<std::mutex>  guard(id_lock);
  if (id_ >= GetMaxModuleNumber()) {
    return;
  }
  module_id_mask_ &= ~(1 << id_);
}

}  // namespace cnstream
