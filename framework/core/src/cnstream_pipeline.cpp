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
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"
#include "connector.hpp"
#include "conveyor.hpp"
#include "perf_calculator.hpp"
#include "perf_manager.hpp"
#include "util/cnstream_queue.hpp"

namespace cnstream {

uint32_t GetMaxStreamNumber() { return MAX_STREAM_NUM; }

uint32_t GetMaxModuleNumber() {
  /*maxModuleIdNum is sizeof(module_id_mask_) * 8  (bytes->bits)*/
  return sizeof(uint64_t) * 8;
}

uint32_t IdxManager::GetStreamIndex(const std::string& stream_id) {
  SpinLockGuard guard(id_lock);
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
  SpinLockGuard guard(id_lock);
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
  SpinLockGuard guard(id_lock);
  for (size_t i = 0; i < GetMaxModuleNumber(); i++) {
    if (!(module_id_mask_ & ((uint64_t)1 << i))) {
      module_id_mask_ |= (uint64_t)1 << i;
      return i;
    }
  }
  return INVALID_MODULE_ID;
}

void IdxManager::ReturnModuleIdx(size_t id_) {
  SpinLockGuard guard(id_lock);
  if (id_ < 0 || id_ >= GetMaxModuleNumber()) {
    return;
  }
  module_id_mask_ &= ~(1 << id_);
}

void Pipeline::SetEOSMask() {
  for (const std::pair<std::string, std::shared_ptr<Module>> module_info : modules_map_) {
    auto instance = module_info.second;
    eos_mask_ |= (uint64_t)1 << instance->GetId();
  }
}
void Pipeline::ClearEOSMask() { eos_mask_ = 0; }

void Pipeline::UpdateByStreamMsg(const StreamMsg& msg) {
  LOG(INFO) << "[" << GetName() << "] got stream message: " << msg.type << " " << msg.stream_id;
  msgq_.Push(msg);
}

void Pipeline::StreamMsgHandleFunc() {
  while (!exit_msg_loop_) {
    StreamMsg msg;
    while (!exit_msg_loop_ && !msgq_.WaitAndTryPop(msg, std::chrono::microseconds(200))) {
    }

    if (exit_msg_loop_) return;

    switch (msg.type) {
      case StreamMsgType::EOS_MSG:
      case StreamMsgType::ERROR_MSG:
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
        LOG(INFO) << "[" << GetName() << "] notify stream message: " << msg.type << " " << msg.stream_id;
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
  LOG_IF(FATAL, nullptr == event_bus_) << "Pipeline::Pipeline() failed to alloc EventBus";
  GetEventBus()->AddBusWatch(std::bind(&Pipeline::DefaultBusWatch, this, std::placeholders::_1));

  idxManager_ = new (std::nothrow) IdxManager();
  LOG_IF(FATAL, nullptr == idxManager_) << "Pipeline::Pipeline() failed to alloc IdxManager";
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
      LOG(ERROR) << "[" << event.module_name << "]: "
                 << "Error: " << event.message;
      ret = EVENT_HANDLE_STOP;
      break;
    case EventType::EVENT_WARNING:
      LOG(WARNING) << "[" << event.module_name << "]: "
                   << "Warning: " + event.message;
      ret = EVENT_HANDLE_SYNCED;
      break;
    case EventType::EVENT_STOP:
      LOG(INFO) << "[" << event.module_name << "]: "
                << "Info: " << event.message;
      ret = EVENT_HANDLE_STOP;
      break;
    case EventType::EVENT_EOS: {
      LOG(INFO) << "Pipeline received eos from module (" + event.module_name << ")"
                << " thread " << event.thread_id;
      ret = EVENT_HANDLE_SYNCED;
      break;
    }
    case EventType::EVENT_INVALID:
      LOG(ERROR) << "[" << event.module_name << "]: "
                 << "Info: " << event.message;
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
    LOG(WARNING) << "Module [" << moduleName << "] has already been added to this pipeline";
    return false;
  }

  LOG(INFO) << "Add Module " << moduleName << " to pipeline";
  module->SetContainer(this);
  if (module->GetId() == INVALID_MODULE_ID) {
    LOG(ERROR) << "Failed to get module Id";
    return false;
  }

  ModuleAssociatedInfo associated_info;
  associated_info.parallelism = 1;
  associated_info.connector = std::make_shared<Connector>(associated_info.parallelism);
  modules_.insert(std::make_pair(moduleName, associated_info));
  modules_map_[moduleName] = module;
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
    LOG(ERROR) << "module has not been added to this pipeline";
    return "";
  }

  ModuleAssociatedInfo& up_node_info = modules_.find(up_node_name)->second;
  ModuleAssociatedInfo& down_node_info = modules_.find(down_node_name)->second;

  std::string link_id = up_node->GetName() + "-->" + down_node->GetName();
  if (!down_node_info.connector) {
    LOG(ERROR) << "connector is invalid when linking " << link_id;
    return "";
  }
  auto ret = up_node_info.down_nodes.insert(down_node_name);
  if (!ret.second) {
    LOG(ERROR) << "modules have been linked already";
    return link_id;
  }

  LOG(INFO) << "Link Module " << link_id;

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
    LOG(ERROR) << "can not find link according to link id";
    return false;
  }
  if (!status) {
    LOG(ERROR) << "status cannot be nullptr";
    return false;
  }
  status->stopped = con->IsStopped();
  for (uint32_t i = 0; i < con->GetConveyorCount(); ++i) {
    status->cache_size.emplace_back(con->GetConveyor(i)->GetBufferSize());
  }
  return true;
}

bool Pipeline::Start() {
  // set eos mask
  SetEOSMask();
  // open modules
  std::vector<std::shared_ptr<Module>> opened_modules;
  bool open_module_failed = false;
  for (auto& it : modules_map_) {
    if (!it.second->Open(GetModuleParamSet(it.second->GetName()))) {
      open_module_failed = true;
      LOG(ERROR) << it.second->GetName() << " start failed!";
      break;
    } else {
      opened_modules.push_back(it.second);
    }
  }

  if (open_module_failed) {
    for (auto it : opened_modules) it->Close();
    ClearEOSMask();
    return false;
  }

  if (perf_running_) {
    for (auto it : perf_managers_) {
      it.second->SqlBeginTrans();
    }
    perf_commit_thread_ = std::thread(&Pipeline::PerfSqlCommitLoop, this);
    calculate_perf_thread_ = std::thread(&Pipeline::CalculatePerfStats, this);
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
      LOG(ERROR) << "The parallelism of the first module should be 0, and the parallelism of other modules should be "
                    "larger than 0. "
                 << "Please check the config of " << node_name << " module.";
      return false;
    }
    if ((!parallelism && module_info.connector) || (parallelism && !module_info.connector) ||
        (parallelism && module_info.connector && parallelism != module_info.connector->GetConveyorCount())) {
      LOG(ERROR) << "Module parallelism do not equal input Connector's Conveyor number, in module " << node_name;
      return false;
    }
    for (uint32_t conveyor_idx = 0; conveyor_idx < parallelism; ++conveyor_idx) {
      threads_.push_back(std::thread(&Pipeline::TaskLoop, this, node_name, conveyor_idx));
    }
  }
  LOG(INFO) << "Pipeline Start";
  LOG(INFO) << "Total Module's threads :" << threads_.size();
  return true;
}

bool Pipeline::Stop() {
  std::lock_guard<std::mutex> lk(stop_mtx_);
  if (!IsRunning()) return true;

  // stop data transmit
  for (const std::pair<std::string, ModuleAssociatedInfo>& it : modules_) {
    if (it.second.connector) {
      it.second.connector->EmptyDataQueue();
      it.second.connector->Stop();
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

  for (auto it : perf_managers_) {
    it.second->Stop();
    it.second = nullptr;
  }

  perf_running_.store(false);
  if (perf_commit_thread_.joinable()) {
    perf_commit_thread_.join();
  }
  if (calculate_perf_thread_.joinable()) {
    calculate_perf_thread_.join();
  }

  perf_managers_.clear();

  ClearEOSMask();
  LOG(INFO) << "Pipeline Stop";
  return true;
}

void Pipeline::TransmitData(std::string moduleName, std::shared_ptr<CNFrameInfo> data) {
  LOG_IF(FATAL, modules_.find(moduleName) == modules_.end());

  const ModuleAssociatedInfo& module_info = modules_[moduleName];

  if (data->IsEos()) {
    LOG(INFO) << "[" << moduleName << "]"
              << " StreamId " << data->stream_id << " got eos.";
    Event e;
    e.type = EventType::EVENT_EOS;
    e.module_name = moduleName;
    e.message = moduleName + " received eos from  " + data->stream_id;
    e.thread_id = std::this_thread::get_id();
    event_bus_->PostEvent(e);
    const uint64_t eos_mask = data->AddEOSMask(modules_map_[moduleName].get());
    if (eos_mask == eos_mask_) {
      StreamMsg msg;
      msg.type = StreamMsgType::EOS_MSG;
      msg.stream_id = data->stream_id;
      msg.module_name = moduleName;
      UpdateByStreamMsg(msg);
    }
  } else {
    if (perf_managers_.find(data->stream_id) != perf_managers_.end()) {
      perf_managers_[data->stream_id]->Record(true, PerfManager::GetDefaultType(), moduleName, data->timestamp);
    }
  }

  Module* module = modules_map_[moduleName].get();
  for (auto& down_node_name : module_info.down_nodes) {
    ModuleAssociatedInfo& down_node_info = modules_.find(down_node_name)->second;
    assert(down_node_info.connector);
    assert(0 < down_node_info.input_connectors.size());
    Module* down_node = modules_map_[down_node_name].get();
    uint64_t frame_mask = data->SetModuleMask(down_node, module);

    // case 1: down_node has only 1 input node: current node
    // case 2: down_node has >1 input nodes, current node has brother nodes
    // the processing data frame will not be pushed into down_node Connector
    // until processed by all brother nodes, the last node responds to transmit
    bool processed_by_all_modules = frame_mask == down_node->GetModulesMask();

    if (processed_by_all_modules) {
      down_node->NotifyObserver(data);
      std::shared_ptr<Connector> connector = down_node_info.connector;
      const uint32_t chn_idx = data->channel_idx;
      int conveyor_idx = chn_idx % connector->GetConveyorCount();
      connector->PushDataBufferToConveyor(conveyor_idx, data);
    }
  }

  // frame done
  if (frame_done_callback_ && (0 == module_info.down_nodes.size())) {
    frame_done_callback_(data);
  }
}

void Pipeline::TaskLoop(std::string node_name, uint32_t conveyor_idx) {
  LOG_IF(FATAL, modules_.find(node_name) == modules_.end());

  ModuleAssociatedInfo& module_info = modules_[node_name];
  std::shared_ptr<Connector> connector = module_info.connector;

  if (!connector.get() || module_info.input_connectors.size() <= 0) {
    return;
  }

  size_t len = node_name.size() > 10 ? 10 : node_name.size();
  std::string thread_name = "cn-" + node_name.substr(0, len) + std::to_string(conveyor_idx);
  SetThreadName(thread_name, pthread_self());

  std::shared_ptr<Module> instance = modules_map_[node_name];
  bool has_data = true;
  while (has_data) {
    has_data = false;
    std::shared_ptr<CNFrameInfo> data;
    // sync data
    data = connector->PopDataBufferFromConveyor(conveyor_idx);
    if (nullptr == data.get()) {
      /*
         nullptr will be received when connector stops.
         maybe only part of the connectors stopped.
         */
      continue;
    }

    has_data = true;
    assert(data->GetModulesMask(instance.get()) == instance->GetModulesMask());
    data->ClearModuleMask(instance.get());

    if (!data->IsEos() && perf_managers_.find(data->stream_id) != perf_managers_.end()) {
      perf_managers_[data->stream_id]->Record(false, PerfManager::GetDefaultType(), node_name, data->timestamp);
      perf_managers_[data->stream_id]->Record(PerfManager::GetDefaultType(), PerfManager::GetPrimaryKey(),
                                                      std::to_string(data->timestamp), node_name + "_th",
                                                      "'" + thread_name + "'");
    }

    int ret = instance->DoProcess(data);

    if (ret < 0) {
      /*process failed*/
      Event e;
      e.type = EventType::EVENT_ERROR;
      e.module_name = node_name;
      e.message = node_name + " process failed, return number: " + std::to_string(ret);
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

int Pipeline::BuildPipeline(const std::vector<CNModuleConfig>& configs) {
  /*TODO,check configs*/
  uint64_t linked_id_mask = 0;
  ModuleCreatorWorker creator;
  for (auto& v : configs) {
    this->AddModuleConfig(v);
    std::shared_ptr<Module> instance(creator.Create(v.className, v.name));
    if (!instance) {
      LOG(ERROR) << "Failed to create module by className(" << v.className << ") and name(" << v.name << ")";
      return -1;
    }
    instance->ShowPerfInfo(v.showPerfInfo);
    this->AddModule(instance);
    this->SetModuleAttribute(instance, v.parallelism, v.maxInputQueueSize);
  }
  for (auto& v : connections_config_) {
    for (auto& name : v.second) {
      if (this->LinkModules(modules_map_[v.first], modules_map_[name]).empty()) {
        LOG(ERROR) << "Link [" << v.first << "] with [" << name << "] failed.";
        return -1;
      }
      linked_id_mask |= (uint64_t)1 << modules_map_[name]->GetId();
    }
  }
  for (auto& v : configs) {
    if (v.className != "cnstream::DataSource" && v.className != "cnstream::TestDataSource" &&
        v.className != "cnstream::ModuleIPC" &&
        !(((uint64_t)1 << modules_map_[v.name]->GetId()) & linked_id_mask)) {
      LOG(ERROR) << v.name << " not linked to any module.";
      return -1;
    }
  }
  return 0;
}

int Pipeline::BuildPipelineByJSONFile(const std::string& config_file) {
  std::vector<CNModuleConfig> mconfs;
  bool ret = ConfigsFromJsonFile(config_file, mconfs);
  if (ret != true) {
    return -1;
  }
  return BuildPipeline(mconfs);
}

Module* Pipeline::GetModule(const std::string& moduleName) {
  auto iter = modules_map_.find(moduleName);
  if (iter != modules_map_.end()) {
    return modules_map_[moduleName].get();
  }
  return nullptr;
}

bool Pipeline::CreatePerfManager(std::vector<std::string> stream_ids, std::string db_dir) {
  if (perf_running_) {
    return false;
  }
  if (db_dir == "") {
    db_dir = "perf_database";
  }
  end_nodes_.clear();

  // Get start module name, end module name and module names
  std::vector<std::string> module_names;
  for (auto& module_it : modules_) {
    const std::string node_name = module_it.first;
    LOG(INFO) << "module name " << node_name << std::endl;
    module_names.push_back(node_name);
    if (module_it.second.input_connectors.size() == 0) {
      start_node_ = node_name;
    }
    if (module_it.second.output_connectors.size() == 0) {
      end_nodes_.push_back(node_name);
    }
  }

  std::shared_ptr<PerfUtils> perf_utils = std::make_shared<PerfUtils>();
  std::vector<std::string> keys =
      PerfManager::GetKeys(module_names, {PerfManager::GetStartTimeSuffix(), PerfManager::GetEndTimeSuffix(), "_th"});

  // Create PerfManager for all streams
  for (auto& stream_id : stream_ids) {
    LOG(INFO) << "Create PerfManager for stream " << stream_id;
    std::shared_ptr<PerfManager> manager = std::make_shared<PerfManager>();
    perf_managers_[stream_id] = manager;
    if (!manager->Init(db_dir + "/stream_" + stream_id + ".db")) {
      LOG(ERROR) << "Init PerfManager of stream " << stream_id << " failed.";
      return false;
    }
    if (!manager->RegisterPerfType(PerfManager::GetDefaultType(), PerfManager::GetPrimaryKey(), keys)) {
      LOG(ERROR) << "PerfManager of stream " << stream_id << " register perf type "
                 << PerfManager::GetDefaultType() << "failed.";
      return false;
    }
    perf_utils->AddSql(stream_id, manager->GetSql());
  }

  // Create PerfCalculators for each module
  for (auto& module_it : modules_) {
    std::string node_name = module_it.first;
    if (perf_calculators_.find(node_name) != perf_calculators_.end()) {
      LOG(WARNING) << "perf calculator is created before. name : " << node_name;
    }
    perf_calculators_[node_name] = std::make_shared<PerfCalculatorForModule>();
    perf_calculators_[node_name]->SetPerfUtils(perf_utils);
  }

  // Create PerfCalculators for pipeline
  for (auto& end_node : end_nodes_) {
    if (perf_calculators_.find("pipeline_" + end_node) != perf_calculators_.end()) {
      LOG(WARNING) << "perf calculator is created before. name : "
                   << "pipeline_" + end_node;
    }
    perf_calculators_["pipeline_" + end_node] = std::make_shared<PerfCalculatorForPipeline>();
    perf_calculators_["pipeline_" + end_node]->SetPerfUtils(perf_utils);
  }

  stream_ids_ = stream_ids;
  perf_running_.store(true);
  return true;
}

void Pipeline::CalculatePerfStats() {
  while (perf_running_) {
    std::cout << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"
              << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$" << std::endl;
    CalculateModulePerfStats();
    std::cout << "\n" << std::endl;
    CalculatePipelinePerfStats();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "\n\n" << std::endl;
  }
  std::cout << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
            << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%" << std::endl;
  CalculateModulePerfStats(1);
  std::cout << "\n" << std::endl;
  CalculatePipelinePerfStats(1);
}

void Pipeline::PerfSqlCommitLoop() {
  while (perf_running_) {
    for (auto& it : perf_managers_) {
      it.second->SqlCommitTrans();
      it.second->SqlBeginTrans();
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  for (auto& it : perf_managers_) {
    it.second->SqlCommitTrans();
  }
}

static void CalcAndPrintLatestThroughput(std::string sql_name, std::string perf_type, std::vector<std::string> keys,
                                         std::shared_ptr<PerfCalculator> calculator, bool final_print,
                                         bool is_pipeline) {
  if (!is_pipeline) {
    PrintTitleForLatestThroughput();
  }
  // calculate throughput for each module
  PerfStats stats;
  if (final_print) {
    calculator->SetPrintThroughput(false);
    stats = calculator->CalculateFinalThroughput(sql_name, perf_type, keys);
    calculator->SetPrintThroughput(true);
  } else {
    stats = calculator->CalcThroughput(sql_name, perf_type, keys);
  }
  if (!is_pipeline) {
    PrintTitleForTotal();
  } else {
    std::cout << "\n(* Note: There is a slight delay.)\n";
    PrintStr("Pipeline : ");
  }
  PrintThroughput(stats);
}

void Pipeline::CalculateModulePerfStats(bool final_print) {
  for (auto& module_it : modules_map_) {
    std::string node_name = module_it.first;
    std::shared_ptr<Module> instance = module_it.second;
    if (instance && instance->ShowPerfInfo()) {
      if (perf_calculators_.find(node_name) != perf_calculators_.end()) {
        PrintTitle(node_name + " Performance");
        std::shared_ptr<PerfCalculator> calculator = perf_calculators_[node_name];
        std::vector<std::pair<std::string, PerfStats>> latency_vec;
        std::vector<uint32_t> digit_of_frame_cnt;
        for (auto& stream_id : stream_ids_) {
          // calculate and print latency for each stream
          PerfStats stats = calculator->CalcLatency(stream_id, PerfManager::GetDefaultType(),
              {node_name + PerfManager::GetStartTimeSuffix(), node_name + PerfManager::GetEndTimeSuffix()});

          digit_of_frame_cnt.push_back(std::to_string(stats.frame_cnt).length());
          latency_vec.push_back(std::make_pair(stream_id, stats));
        }  // for each stream

        for (auto& it : latency_vec) {
          PrintStreamId(it.first);
          PrintLatency(it.second, PerfUtils::Max(digit_of_frame_cnt));
        }
        // calculate and print throughput for module
        CalcAndPrintLatestThroughput("", PerfManager::GetDefaultType(),
                                     {node_name + PerfManager::GetStartTimeSuffix(),
                                      node_name + PerfManager::GetEndTimeSuffix(), node_name + "_th"},
                                      calculator, final_print, false);
        PerfStats avg_fps = calculator->GetAvgThroughput("", PerfManager::GetDefaultType());
        PrintTitleForAverageThroughput();
        PrintTitleForTotal();
        PrintThroughput(avg_fps);
      }
    }
  }  // for each module
}

void Pipeline::CalculatePipelinePerfStats(bool final_print) {
  std::cout << "\033[32m";
  PrintTitle("Pipeline Performance");
  std::cout<< "\033[0m";

  for (auto& end_node : end_nodes_) {
    if (perf_calculators_.find("pipeline_" + end_node) != perf_calculators_.end()) {
      std::shared_ptr<PerfCalculator> calculator = perf_calculators_["pipeline_" + end_node];
      std::cout << "End node : " << end_node << std::endl;

      double total_fps_tmp = 0.f;
      size_t total_fn_tmp = 0;
      std::vector<std::pair<std::string, PerfStats>> latest_fps, entire_fps, latency_vec;
      std::vector<uint32_t> latest_frame_cnt_digit, entire_frame_cnt_digit, latency_frame_cnt_digit;
      for (auto& stream_id : stream_ids_) {
        // calculate and print latency for each stream
        PerfStats latency_stats =
            calculator->CalcLatency(stream_id, PerfManager::GetDefaultType(),
            {start_node_ + PerfManager::GetStartTimeSuffix(), end_node + PerfManager::GetEndTimeSuffix()});
        // calculate throughput for each stream
        PerfStats fps_stats = calculator->CalcThroughput(stream_id, PerfManager::GetDefaultType(),
                                                         {end_node + PerfManager::GetEndTimeSuffix()});
        PerfStats avg_fps = calculator->GetAvgThroughput(stream_id, PerfManager::GetDefaultType());

        latency_vec.push_back(std::make_pair(stream_id, latency_stats));
        latest_fps.push_back(std::make_pair(stream_id, fps_stats));
        entire_fps.push_back(std::make_pair(stream_id, avg_fps));
        latency_frame_cnt_digit.push_back(std::to_string(avg_fps.frame_cnt).length());
        latest_frame_cnt_digit.push_back(std::to_string(fps_stats.frame_cnt).length());
        entire_frame_cnt_digit.push_back(std::to_string(avg_fps.frame_cnt).length());

        total_fn_tmp += avg_fps.frame_cnt;
        total_fps_tmp += avg_fps.fps;
      }  // for each stream

      uint32_t max_digit = PerfUtils::Max(latency_frame_cnt_digit);
      for (auto& it : latency_vec) {
        PrintStreamId(it.first);
        PrintLatency(it.second, max_digit);
      }
      // std::cout << "Stream Sum Total : " << total_fps_tmp << " cnt = " << total_fn_tmp << std::endl;

      // print throughput for each stream
      PrintTitleForLatestThroughput();
      max_digit = PerfUtils::Max(latest_frame_cnt_digit);
      for (auto& it : latest_fps) {
        PrintStreamId(it.first);
        PrintThroughput(it.second, max_digit);
      }
      // calculate and print throughput for pipeline
      CalcAndPrintLatestThroughput("", PerfManager::GetDefaultType(),
                                   {end_node + PerfManager::GetEndTimeSuffix()},
                                   calculator, final_print, true);

      PerfStats avg_fps = calculator->GetAvgThroughput("", PerfManager::GetDefaultType());
      PrintTitleForAverageThroughput();
      max_digit = PerfUtils::Max(entire_frame_cnt_digit);
      for (auto& it : entire_fps) {
        PrintStreamId(it.first);
        PrintThroughput(it.second, max_digit);
      }
      PrintTitleForTotal();
      PrintThroughput(avg_fps);
      double running_seconds = static_cast<double>(avg_fps.latency_max) / 1e6;
      double running_mins = running_seconds / 60;
      double running_hours = running_mins / 60;
      std::cout << "Running time (s):" << running_seconds << std::endl;
      std::cout << "Running time (m):" << running_mins << std::endl;
      std::cout << "Running time (h):" << running_hours << std::endl;
      if (final_print) {
        std::cout << "\nTotal : " << avg_fps.fps << std::endl;
      }
    }
  }
}

std::unordered_map<std::string, std::shared_ptr<PerfManager>> Pipeline::GetPerfManagers() {
  return perf_managers_;
}

}  // namespace cnstream
