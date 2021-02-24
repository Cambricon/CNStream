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
#include <iomanip>
#include <iostream>
#include <list>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"
#include "connector.hpp"
#include "conveyor.hpp"
#include "profiler/module_profiler.hpp"
#include "profiler/pipeline_profiler.hpp"
#include "util/cnstream_queue.hpp"
#include "util/cnstream_time_utility.hpp"

namespace cnstream {

uint32_t GetMaxStreamNumber() { return MAX_STREAM_NUM; }

uint32_t GetMaxModuleNumber() {
  /*maxModuleIdNum is sizeof(module_id_mask_) * 8  (bytes->bits)*/
  return sizeof(uint64_t) * 8;
}

uint32_t IdxManager::GetStreamIndex(const std::string& stream_id) {
  std::lock_guard<std::mutex>  guard(id_lock);
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
  std::lock_guard<std::mutex>  guard(id_lock);
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

void Pipeline::UpdateByStreamMsg(const StreamMsg& msg) {
  LOGD(CORE) << "[" << GetName() << "] "
            << "stream: " << msg.stream_id << " got message: " << msg.type;
  msgq_.Push(msg);
}

void Pipeline::StreamMsgHandleFunc() {
  while (!exit_msg_loop_) {
    StreamMsg msg;
    while (!exit_msg_loop_ && !msgq_.WaitAndTryPop(msg, std::chrono::microseconds(200))) {
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
                  << "stream: " << msg.stream_id << " notify message: " << msg.type;
        if (smsg_observer_) {
          smsg_observer_->Update(msg);
        }
        break;
      default:
        break;
    }
  }
}

Pipeline::Pipeline(const std::string& name) : name_(name) {
  // stream message handle thread
  exit_msg_loop_ = false;
  smsg_thread_ = std::thread(&Pipeline::StreamMsgHandleFunc, this);

  event_bus_ = new (std::nothrow) EventBus();
  LOGF_IF(CORE, nullptr == event_bus_) << "Pipeline::Pipeline() failed to alloc EventBus";
  GetEventBus()->AddBusWatch(std::bind(&Pipeline::DefaultBusWatch, this, std::placeholders::_1));

  idxManager_ = new (std::nothrow) IdxManager();
  LOGF_IF(CORE, nullptr == idxManager_) << "Pipeline::Pipeline() failed to alloc IdxManager";
}

Pipeline::~Pipeline() {
  running_ = false;
  for (auto& it : modules_map_) {
    it.second->SetContainer(nullptr);
  }
  exit_msg_loop_ = true;
  if (smsg_thread_.joinable()) {
    smsg_thread_.join();
  }
  delete event_bus_;
  delete idxManager_;
}

void Pipeline::SetStreamMsgObserver(StreamMsgObserver* observer) {
  smsg_observer_ = observer;
}

StreamMsgObserver* Pipeline::GetStreamMsgObserver() const {
  return smsg_observer_;
}

EventHandleFlag Pipeline::DefaultBusWatch(const Event& event) {
  StreamMsg smsg;
  EventHandleFlag ret;
  switch (event.type) {
    case EventType::EVENT_ERROR:
      smsg.type = ERROR_MSG;
      smsg.module_name = event.module_name;
      UpdateByStreamMsg(smsg);
      LOGE(CORE) << "[" << event.module_name << "]: "
                 << event.message;
      ret = EVENT_HANDLE_STOP;
      break;
    case EventType::EVENT_WARNING:
      LOGW(CORE) << "[" << event.module_name << "]: "
                 << event.message;
      ret = EVENT_HANDLE_SYNCED;
      break;
    case EventType::EVENT_STOP:
      LOGI(CORE) << "[" << event.module_name << "]: "
                 << event.message;
      ret = EVENT_HANDLE_STOP;
      break;
    case EventType::EVENT_EOS: {
      LOGD(CORE) << "Pipeline received eos from module " + event.module_name << " of stream " << event.stream_id;
      ret = EVENT_HANDLE_SYNCED;
      break;
    }
    case EventType::EVENT_STREAM_ERROR: {
      smsg.type = STREAM_ERR_MSG;
      smsg.module_name = event.module_name;
      smsg.stream_id = event.stream_id;
      UpdateByStreamMsg(smsg);
      LOGD(CORE) << "Pipeline received stream error from module " + event.module_name
                 << " of stream " << event.stream_id;
      ret = EVENT_HANDLE_SYNCED;
      break;
    }
    case EventType::EVENT_INVALID:
      LOGE(CORE) << "[" << event.module_name << "]: "
                 << event.message;
    default:
      ret = EVENT_HANDLE_NULL;
      break;
  }
  return ret;
}

bool Pipeline::ProvideData(const Module* module, std::shared_ptr<CNFrameInfo> data) {
  std::string moduleName = module->GetName();

  if (modules_map_.find(moduleName) == modules_map_.end()) return false;

  TransmitData(moduleName, data);

  return true;
}

bool Pipeline::AddModule(std::shared_ptr<Module> module) {
  std::string moduleName = module->GetName();

  if (modules_map_.find(moduleName) != modules_map_.end()) {
    LOGW(CORE) << "Module [" << moduleName << "] has already been added to this pipeline";
    return false;
  }

  LOGI(CORE) << "Add Module " << moduleName << " to pipeline";
  module->SetContainer(this);
  if (module->GetId() == INVALID_MODULE_ID) {
    LOGE(CORE) << "Failed to get module Id";
    return false;
  }

  ModuleAssociatedInfo associated_info;
  associated_info.parallelism = 1;
  associated_info.connector = std::make_shared<Connector>(associated_info.parallelism);
  modules_.insert(std::make_pair(moduleName, associated_info));
  modules_map_[moduleName] = module;

  // update modules mask
  all_modules_mask_ |= (uint64_t)1 << module->GetId();
  return true;
}

bool Pipeline::SetModuleAttribute(std::shared_ptr<Module> module, uint32_t parallelism, size_t queue_capacity) {
  std::string moduleName = module->GetName();
  if (modules_.find(moduleName) == modules_.end()) return false;
  modules_[moduleName].parallelism = parallelism;
  if (parallelism && queue_capacity) {
    modules_[moduleName].connector = std::make_shared<Connector>(parallelism, queue_capacity);
    return static_cast<bool>(modules_[moduleName].connector);
  }
  if (!parallelism && modules_[moduleName].connector) {
    modules_[moduleName].connector.reset();
  }
  return true;
}

std::string Pipeline::LinkModules(std::shared_ptr<Module> up_node, std::shared_ptr<Module> down_node) {
  if (up_node == nullptr || down_node == nullptr) {
    return "";
  }

  std::string up_node_name = up_node->GetName();
  std::string down_node_name = down_node->GetName();

  if (modules_.find(up_node_name) == modules_.end() ||
      modules_.find(down_node_name) == modules_.end()) {
    LOGE(CORE) << "module has not been added to this pipeline";
    return "";
  }

  ModuleAssociatedInfo& up_node_info = modules_.find(up_node_name)->second;
  ModuleAssociatedInfo& down_node_info = modules_.find(down_node_name)->second;

  std::string link_id = up_node->GetName() + "-->" + down_node->GetName();
  if (!down_node_info.connector) {
    LOGE(CORE) << "connector is invalid when linking " << link_id;
    return "";
  }
  auto ret = up_node_info.down_nodes.insert(down_node_name);
  if (!ret.second) {
    LOGE(CORE) << "modules have been linked already";
    return link_id;
  }

  LOGI(CORE) << "Link Module " << link_id;

  // create connector
  up_node_info.output_connectors.push_back(link_id);
  down_node_info.input_connectors.push_back(link_id);
  links_[link_id] = down_node_info.connector;

  down_node->SetParentId(up_node->GetId());
  return link_id;
}

bool Pipeline::QueryLinkStatus(LinkStatus* status, const std::string& link_id) {
  std::shared_ptr<Connector> con = links_[link_id];
  if (!con) {
    LOGE(CORE) << "can not find link according to link id";
    return false;
  }
  if (!status) {
    LOGE(CORE) << "status cannot be nullptr";
    return false;
  }
  status->stopped = con->IsStopped();
  for (uint32_t i = 0; i < con->GetConveyorCount(); ++i) {
    status->cache_size.emplace_back(con->GetConveyorSize(i));
  }
  return true;
}

bool Pipeline::Start() {
  if (IsRunning()) return true;

  // open modules
  std::vector<std::shared_ptr<Module>> opened_modules;
  bool open_module_failed = false;
  for (auto& it : modules_map_) {
    if (!it.second->Open(GetModuleParamSet(it.second->GetName()))) {
      open_module_failed = true;
      LOGE(CORE) << it.second->GetName() << " start failed!";
      break;
    } else {
      opened_modules.push_back(it.second);
    }
  }

  if (open_module_failed) {
    for (auto it : opened_modules) it->Close();
    return false;
  }

  // start data transmit
  running_.store(true);
  event_bus_->Start();

  for (const std::pair<std::string, ModuleAssociatedInfo>& it : modules_) {
    if (it.second.connector) {
      it.second.connector->Start();
    }
  }

  // create process threads
  for (auto& it : modules_) {
    const std::string node_name = it.first;
    ModuleAssociatedInfo& module_info = it.second;
    uint32_t parallelism = module_info.parallelism;
    if ((!parallelism && module_info.input_connectors.size()) ||
        (parallelism && !module_info.input_connectors.size())) {
      LOGE(CORE) << "The parallelism of the first module should be 0, and the parallelism of other modules should be "
                    "larger than 0. "
                 << "Please check the config of " << node_name << " module.";
      Stop();
      return false;
    }
    if ((!parallelism && module_info.connector) || (parallelism && !module_info.connector) ||
        (parallelism && module_info.connector && parallelism != module_info.connector->GetConveyorCount())) {
      LOGE(CORE) << "Module parallelism do not equal input Connector's Conveyor number, in module " << node_name;
      Stop();
      return false;
    }
    for (uint32_t conveyor_idx = 0; conveyor_idx < parallelism; ++conveyor_idx) {
      threads_.push_back(std::thread(&Pipeline::TaskLoop, this, node_name, conveyor_idx));
    }
  }
  LOGI(CORE) << "Pipeline Start";
  LOGI(CORE) << "All modules, except the first module, total  threads  is: " << threads_.size();
  return true;
}

bool Pipeline::Stop() {
  if (!IsRunning()) return true;

  // stop data transmit
  for (const std::pair<std::string, ModuleAssociatedInfo>& it : modules_) {
    if (it.second.connector) {
      // push data will be rejected after Stop()
      // stop first to ensure connector will be empty
      it.second.connector->Stop();
      it.second.connector->EmptyDataQueue();
    }
  }
  running_.store(false);
  for (std::thread& it : threads_) {
    if (it.joinable()) it.join();
  }
  threads_.clear();
  event_bus_->Stop();

  // close modules
  for (auto& it : modules_map_) {
    it.second->Close();
  }

  LOGI(CORE) << "[" << GetName() << "] " << "Stop";
  return true;
}

void Pipeline::TransmitData(std::string moduleName, std::shared_ptr<CNFrameInfo> data) {
  LOGF_IF(CORE, modules_.find(moduleName) == modules_.end());

  const ModuleAssociatedInfo& module_info = modules_[moduleName];
  Module* module = modules_map_[moduleName].get();

  if (IsRootNode(moduleName)) {
    /** set mask to 1 for never touched modules, for case which has multiple source modules. **/
    data->SetModulesMask(route_masks_[moduleName]);
  }
  uint64_t changed_mask = data->MarkPassed(module);

  const auto profiling_record_key = std::make_pair(data->stream_id, data->timestamp);

  if (data->IsEos()) {
    if (profiler_)
      module->GetProfiler()->OnStreamEos(data->stream_id);

    LOGI(CORE) << "[" << moduleName << "]"
               << " [" << data->stream_id << "] got eos.";
    Event e;
    e.type = EventType::EVENT_EOS;
    e.module_name = moduleName;
    e.stream_id = data->stream_id;
    e.thread_id = std::this_thread::get_id();
    event_bus_->PostEvent(e);
    if (changed_mask == all_modules_mask_) {
      // passed by all modules
      StreamMsg msg;
      msg.type = StreamMsgType::EOS_MSG;
      msg.stream_id = data->stream_id;
      msg.module_name = moduleName;
      UpdateByStreamMsg(msg);
    }
    if (profiler_ && IsLeafNode(moduleName) && PassedByAllModules(changed_mask)) {
      profiler_->OnStreamEos(data->stream_id);
    }
  } else {
    /* For the stream is removed, do not pass the packet on */
    if (IsStreamRemoved(data->stream_id)) {
      return;
    }
    if (profiler_) {
      if (IsLeafNode(moduleName) && PassedByAllModules(changed_mask)) {
        profiler_->RecordOutput(profiling_record_key);
      }
      if (!IsRootNode(moduleName)) {
        profiler_->GetModuleProfiler(moduleName)
                 ->RecordProcessEnd(kPROCESS_PROFILER_NAME, profiling_record_key);
      }
    }
  }

  // If data is invalid
  if (data->IsInvalid()) {
    StreamMsg msg;
    msg.type = StreamMsgType::FRAME_ERR_MSG;
    msg.stream_id = data->stream_id;
    msg.module_name = moduleName;
    msg.pts = data->timestamp;
    UpdateByStreamMsg(msg);
    LOGW(CORE) << "[" << GetName() << "]" << " got frame error from " << module->name_ <<
      " stream_id: " << data->stream_id << ", pts: " << data->timestamp;
    return;
  }
  module->NotifyObserver(data);
  for (auto& down_node_name : module_info.down_nodes) {
    ModuleAssociatedInfo& down_node_info = modules_.find(down_node_name)->second;
    assert(down_node_info.connector);
    assert(0 < down_node_info.input_connectors.size());
    Module* down_node = modules_map_[down_node_name].get();

    // case 1: down_node has only 1 input node: current node
    // case 2: down_node has >1 input nodes, current node has brother nodes
    // the processing data frame will not be pushed into down_node Connector
    // until processed by all brother nodes, the last node responds to transmit
    bool processed_by_all_modules = ShouldTransmit(changed_mask, down_node);

    if (processed_by_all_modules) {
      std::shared_ptr<Connector> connector = down_node_info.connector;
      int conveyor_idx = data->GetStreamIndex() % connector->GetConveyorCount();
      if (profiler_ && !data->IsEos()) {
        profiler_->GetModuleProfiler(down_node_name)
                 ->RecordProcessStart(kINPUT_PROFILER_NAME, profiling_record_key);
      }
      while (!connector->IsStopped() && connector->PushDataBufferToConveyor(conveyor_idx, data) == false) {
        if (connector->GetFailTime(conveyor_idx) % 50 == 0) {
          // Show infomation when conveyor is full in every second
          // LOGI(CORE) << "[" << down_node->name_  << " " << conveyor_idx << "] " << "Input buffer is full";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
    }
  }

  // frame done
  if (frame_done_callback_ && (0 == module_info.down_nodes.size())) {
    frame_done_callback_(data);
  }
}

void Pipeline::TaskLoop(std::string node_name, uint32_t conveyor_idx) {
  LOGF_IF(CORE, modules_.find(node_name) == modules_.end());

  ModuleAssociatedInfo& module_info = modules_[node_name];
  std::shared_ptr<Connector> connector = module_info.connector;

  if (!connector.get() || module_info.input_connectors.size() <= 0) {
    return;
  }

  size_t len = node_name.size() > 10 ? 10 : node_name.size();
  std::string thread_name = "cn-" + node_name.substr(0, len) + "-" + NumToFormatStr(conveyor_idx, 2);
  SetThreadName(thread_name, pthread_self());

  std::shared_ptr<Module> instance = modules_map_[node_name];
  while (1) {
    std::shared_ptr<CNFrameInfo> data = nullptr;
    // sync data
    while (!connector->IsStopped() && data == nullptr) {
      // LOGI(CORE) << "[" << instance->name_ << "]" << " There is no avaiable input data"; 
      data = connector->PopDataBufferFromConveyor(conveyor_idx);
    }
    if (connector->IsStopped()) {
      // when connector stops, break taskloop
      break;
    }

    if (data == nullptr) {
      continue;
    }

    assert(ShouldTransmit(data, instance.get()));

    if (profiler_ && !data->IsEos()) {
      auto profiling_record_key = std::make_pair(data->stream_id, data->timestamp);
      profiler_->GetModuleProfiler(node_name)
               ->RecordProcessEnd(kINPUT_PROFILER_NAME, profiling_record_key);
      profiler_->GetModuleProfiler(node_name)
               ->RecordProcessStart(kPROCESS_PROFILER_NAME, profiling_record_key);
    }

    int ret = instance->DoProcess(data);

    if (ret < 0) {
      /*process failed*/
      Event e;
      e.type = EventType::EVENT_ERROR;
      e.module_name = node_name;
      e.message = node_name + " process failed, return number: " + std::to_string(ret);
      e.stream_id = data->stream_id;
      e.thread_id = std::this_thread::get_id();
      event_bus_->PostEvent(e);
      StreamMsg msg;
      msg.type = StreamMsgType::ERROR_MSG;
      msg.stream_id = data->stream_id;
      msg.module_name = node_name;
      UpdateByStreamMsg(msg);
      return;
    }
  }  // while
}

/* ------config/auto-graph methods------ */
int Pipeline::AddModuleConfig(const CNModuleConfig& config) {
  modules_config_[config.name] = config;
  connections_config_[config.name] = config.next;
  return 0;
}

ModuleParamSet Pipeline::GetModuleParamSet(const std::string& moduleName) {
  ModuleParamSet paramSet;
  auto iter = modules_config_.find(moduleName);
  if (iter != modules_config_.end()) {
    for (auto& v : iter->second.parameters) {
      // filter some keys ...
      paramSet[v.first] = v.second;
    }
  }
  return paramSet;
}

CNModuleConfig Pipeline::GetModuleConfig(const std::string& module_name) {
  CNModuleConfig config = {};
  auto iter = modules_config_.find(module_name);
  if (iter != modules_config_.end()) {
    config = iter->second;
  }
  return config;
}

void Pipeline::GenerateRouteMask() {
  std::unordered_map<std::string, bool> visit_init_map;
  for (const auto& module_info : modules_) {
    visit_init_map[module_info.first] = false;
  }
  for (const auto& module_info : modules_) {
    if (IsRootNode(module_info.first)) {
      auto visit = visit_init_map;
      uint64_t route_mask = all_modules_mask_;
      // bfs
      std::queue<std::string> nodes;
      nodes.push(module_info.first);
      while (!nodes.empty()) {
        auto node = nodes.front();
        nodes.pop();
        if (visit[node]) continue;
        route_mask ^= 1UL << modules_map_[node]->GetId();
        visit[node] = true;
        for (const auto& down_node : modules_[node].down_nodes)
          nodes.push(down_node);
      }
      route_masks_[module_info.first] = route_mask;
    }
  }
}

int Pipeline::BuildPipeline(const std::vector<CNModuleConfig>& module_configs, const ProfilerConfig& profiler_config) {
  /*TODO,check configs*/
  uint64_t linked_id_mask = 0;
  ModuleCreatorWorker creator;
  std::vector<std::shared_ptr<Module>> modules;
  for (auto& v : module_configs) {
    this->AddModuleConfig(v);
    std::shared_ptr<Module> instance(creator.Create(v.className, v.name));
    if (!instance) {
      LOGE(CORE) << "Failed to create module by className(" << v.className << ") and name(" << v.name << ")";
      return -1;
    }
    this->AddModule(instance);
    this->SetModuleAttribute(instance, v.parallelism, v.maxInputQueueSize);
    modules.push_back(instance);
  }
  for (auto& v : connections_config_) {
    for (auto& name : v.second) {
      if (modules_map_.find(v.first) == modules_map_.end() ||
          modules_map_.find(name) == modules_map_.end() ||
          this->LinkModules(modules_map_[v.first], modules_map_[name]).empty()) {
        LOGE(CORE) << "Link [" << v.first << "] with [" << name << "] failed.";
        return -1;
      }
      linked_id_mask |= (uint64_t)1 << modules_map_[name]->GetId();
    }
  }
  for (auto& v : module_configs) {
    if (v.className != "cnstream::DataSource" && v.className != "cnstream::TestDataSource" &&
        v.className != "cnstream::ModuleIPC" &&
        !(((uint64_t)1 << modules_map_[v.name]->GetId()) & linked_id_mask)) {
      LOGE(CORE) << v.name << " not linked to any module.";
      return -1;
    }
  }

  GenerateRouteMask();
  profiler_config_ = profiler_config;
  profiler_ = std::unique_ptr<PipelineProfiler>(new PipelineProfiler(profiler_config, GetName(), modules));
  return 0;
}

int Pipeline::BuildPipelineByJSONFile(const std::string& config_file) {
  std::vector<CNModuleConfig> mconfs;
  ProfilerConfig profiler_config;
  bool ret = ConfigsFromJsonFile(config_file, &mconfs, &profiler_config);
  if (ret != true) {
    return -1;
  }
  return BuildPipeline(mconfs, profiler_config);
}

Module* Pipeline::GetModule(const std::string& moduleName) {
  auto iter = modules_map_.find(moduleName);
  if (iter != modules_map_.end()) {
    return modules_map_[moduleName].get();
  }
  return nullptr;
}

Module* Pipeline::GetEndModule() {
  std::string end_node_name;
  for (auto& it : modules_) {
    const std::string node_name = it.first;
    ModuleAssociatedInfo& module_info = it.second;
    if (0 == module_info.down_nodes.size()) {
      end_node_name = end_node_name.empty() ? node_name : "";
    }
  }

  if (!end_node_name.empty()) return modules_map_[end_node_name].get();
  return nullptr;
}

std::vector<std::string> Pipeline::GetModuleNames() {
  std::vector<std::string> module_names;
  for (auto& module_it : modules_) {
    const std::string node_name = module_it.first;
    module_names.push_back(node_name);
  }
  return module_names;
}

}  // namespace cnstream
