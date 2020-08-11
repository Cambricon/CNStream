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
#include "util/cnstream_time_utility.hpp"

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
  if (id_ >= GetMaxModuleNumber()) {
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
  LOG(INFO) << "[" << GetName() << "] "
            << "stream: " << msg.stream_id << " got message: " << msg.type;
  msgq_.Push(msg);
}

void Pipeline::StreamMsgHandleFunc() {
  while (!exit_msg_loop_) {
    StreamMsg msg;
    while (!exit_msg_loop_ && !msgq_.WaitAndTryPop(msg, std::chrono::microseconds(200))) {
    }

    if (exit_msg_loop_) {
        LOG(INFO) << "[" << GetName() << "] stop updating stream message";
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
        LOG(INFO) << "[" << GetName() << "] "
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
      LOG(INFO) << "Pipeline received eos from module " + event.module_name << " of stream " << event.message;
      ret = EVENT_HANDLE_SYNCED;
      break;
    }
    case EventType::EVENT_STREAM_ERROR: {
      smsg.type = STREAM_ERR_MSG;
      smsg.module_name = event.module_name;
      smsg.stream_id = event.stream_id;
      UpdateByStreamMsg(smsg);
      LOG(INFO) << "Pipeline received stream error from module " + event.module_name
        << " of stream " << event.stream_id;
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
  if (IsRunning()) return true;

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
    RwLockReadGuard lg(perf_managers_lock_);
    for (auto it : perf_managers_) {
      it.second->SqlBeginTrans();
    }
    perf_commit_thread_ = std::thread(&Pipeline::PerfSqlCommitLoop, this);
    calculate_perf_thread_ = std::thread(&Pipeline::CalculatePerfStats, this);
    perf_del_data_thread_ = std::thread(&Pipeline::PerfDeleteDataLoop, this);
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
  LOG(INFO) << "All modules, except the first module, total  threads  is: " << threads_.size();
  return true;
}

bool Pipeline::Stop() {
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

  {
    RwLockReadGuard lg(perf_managers_lock_);
    for (auto it : perf_managers_) {
      it.second->Stop();
      it.second = nullptr;
    }
  }

  perf_running_.store(false);
  if (perf_del_data_thread_.joinable()) {
    perf_del_data_thread_.join();
  }
  if (perf_commit_thread_.joinable()) {
    perf_commit_thread_.join();
  }
  if (calculate_perf_thread_.joinable()) {
    calculate_perf_thread_.join();
  }

  {
    RwLockWriteGuard lg(perf_managers_lock_);
    perf_managers_.clear();
  }
  {
    std::lock_guard<std::mutex> lg_calc(perf_calculation_lock_);
    perf_calculators_.clear();
    stream_ids_.clear();
    end_nodes_.clear();
  }
  ClearEOSMask();
  LOG(INFO) << "[" << GetName() << "] " << "Stop";
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
    e.message = data->stream_id;
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
  }

  Module* module = modules_map_[moduleName].get();
  // If data is invalid
  if (data->IsInvalid()) {
    StreamMsg msg;
    msg.type = StreamMsgType::FRAME_ERR_MSG;
    msg.stream_id = data->stream_id;
    msg.module_name = moduleName;
    msg.pts = data->timestamp;
    UpdateByStreamMsg(msg);
    LOG(WARNING) << module->name_ << " frame error, stream_id, pts: " << data->stream_id, data->timestamp;
    return;
  }
  module->NotifyObserver(data);
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
    bool processed_by_all_modules = (frame_mask == down_node->GetModulesMask());

    if (processed_by_all_modules) {
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
  std::string thread_name = "cn-" + node_name.substr(0, len) + "-" + NumToFormatStr(conveyor_idx, 2);
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
      if (modules_map_.find(v.first) == modules_map_.end() ||
          modules_map_.find(name) == modules_map_.end() ||
          this->LinkModules(modules_map_[v.first], modules_map_[name]).empty()) {
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

bool Pipeline::CreatePerfManager(std::vector<std::string> stream_ids, std::string db_dir,
                                 uint32_t clear_data_interval_in_minutes) {
  if (perf_running_) return false;
  if (db_dir.empty()) db_dir = "perf_database";
  PerfManager::PrepareDbFileDir(db_dir + "/");

  SetStartAndEndNodeNames();
  std::vector<std::string> module_names = GetModuleNames();

  // Create PerfManager for all streams
  for (auto& stream_id : stream_ids) {
    std::string db_name = db_dir + "/stream_" + stream_id + "_" + TimeStamp::CurrentToDate() + ".db";
    std::shared_ptr<PerfManager> manager = PerfManager::CreateDefaultManager(db_name, module_names);
    if (manager == nullptr) {
      LOG(ERROR) << stream_id << "Failed to create PerfManager";
      return false;
    }
    perf_managers_[stream_id] = manager;
  }

  // Create PerfCalculators for each module and pipeline
  bool is_pipeline = true;
  for (auto& module_it : modules_map_) {
    std::string node_name = module_it.first;
    std::shared_ptr<Module> instance = module_it.second;
    if (instance && instance->ShowPerfInfo()) {
      if (!CreatePerfCalculator(db_dir, node_name, !is_pipeline)) { return false; }
    }
  }
  for (auto& end_node : end_nodes_) {
    if (!CreatePerfCalculator(db_dir, end_node, is_pipeline)) { return false; }
  }

  stream_ids_ = stream_ids;
  if (clear_data_interval_in_minutes > 0) {
    clear_data_interval_ = clear_data_interval_in_minutes;
  }
  perf_running_.store(true);
  return true;
}

void Pipeline::CalculatePerfStats() {
  uint64_t start, end;
  uint32_t interval = 2000000;  // 2s
  while (perf_running_) {
    start = TimeStamp::Current();
    std::cout << "\033[1;33m" << "\n\n$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"
              << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$" << "\033[0m" << std::endl;
    CalculateModulePerfStats();
    CalculatePipelinePerfStats();
    end = TimeStamp::Current();
    if (end > start && end - start < interval) {
      std::this_thread::sleep_for(std::chrono::microseconds(interval - (end - start)));
    }
  }
  std::cout << "\033[1;33m" << "\n\n%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
            << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%" << "\033[0m" << std::endl;
  CalculateModulePerfStats(1);
  CalculatePipelinePerfStats(1);
}

void Pipeline::PerfSqlCommitLoop() {
  while (perf_running_) {
    {
       RwLockReadGuard lg(perf_managers_lock_);
      for (auto& it : perf_managers_) {
        it.second->SqlCommitTrans();
        it.second->SqlBeginTrans();
      }
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  RwLockReadGuard lg(perf_managers_lock_);
  for (auto& it : perf_managers_) {
    it.second->SqlCommitTrans();
  }
}

void Pipeline::PerfDeleteDataLoop() {
  uint64_t start, end;
  while (perf_running_) {
    start = TimeStampBase<std::chrono::seconds>::Current();
    {
      RwLockReadGuard lg(perf_managers_lock_);
      for (auto& it : perf_managers_) {
        it.second->DeletePreviousData(clear_data_interval_);
      }
    }
    end = TimeStampBase<std::chrono::seconds>::Current();
    int64_t sleep_time = clear_data_interval_ * 60 - (end - start);
    while (perf_running_ && sleep_time > 0) {
      sleep_time--;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

void Pipeline::CalculateModulePerfStats(bool final_print) {
  std::lock_guard<std::mutex> lg(perf_calculation_lock_);
  for (auto& module_it : modules_map_) {
    std::string node_name = module_it.first;
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
      PrintTitleForLatestThroughput();
      PerfStats stats = CalcLatestThroughput("", PerfManager::GetDefaultType(),
          {node_name + PerfManager::GetStartTimeSuffix(), node_name + PerfManager::GetEndTimeSuffix(),
          node_name + PerfManager::GetThreadSuffix()}, calculator, final_print);
      PrintTitleForTotal();
      PrintThroughput(stats);
      PerfStats avg_fps = calculator->GetAvgThroughput("", PerfManager::GetDefaultType());
      PrintTitleForAverageThroughput();
      PrintTitleForTotal();
      PrintThroughput(avg_fps);
    }
  }  // for each module
}

void Pipeline::CalculatePipelinePerfStats(bool final_print) {
  std::lock_guard<std::mutex> lg(perf_calculation_lock_);
  PrintTitle("Pipeline Performance");
  for (auto& end_node : end_nodes_) {
    if (perf_calculators_.find("pipeline_" + end_node) != perf_calculators_.end()) {
      std::shared_ptr<PerfCalculator> calculator = perf_calculators_["pipeline_" + end_node];
      std::vector<std::pair<std::string, PerfStats>> latency_vec, latest_fps, entire_fps;
      std::vector<uint32_t> latency_frame_cnt_digit, latest_frame_cnt_digit, entire_frame_cnt_digit;
      for (auto& stream_id : stream_ids_) {
        PerfStats latency_stats = calculator->CalcLatency(stream_id, PerfManager::GetDefaultType(),
            {start_node_ + PerfManager::GetStartTimeSuffix(), end_node + PerfManager::GetEndTimeSuffix()});
        PerfStats fps_stats = calculator->CalcThroughput(stream_id, PerfManager::GetDefaultType(),
                                                         {end_node + PerfManager::GetEndTimeSuffix()});
        PerfStats avg_fps = calculator->GetAvgThroughput(stream_id, PerfManager::GetDefaultType());

        latency_vec.push_back(std::make_pair(stream_id, latency_stats));
        latest_fps.push_back(std::make_pair(stream_id, fps_stats));
        entire_fps.push_back(std::make_pair(stream_id, avg_fps));
        latency_frame_cnt_digit.push_back(std::to_string(latency_stats.frame_cnt).length());
        latest_frame_cnt_digit.push_back(std::to_string(fps_stats.frame_cnt).length());
        entire_frame_cnt_digit.push_back(std::to_string(avg_fps.frame_cnt).length());
      }  // for each stream

      std::cout << "End node of pipeline: " << end_node << std::endl;
      for (auto& it : latency_vec) {
        PrintStreamId(it.first);
        PrintLatency(it.second, PerfUtils::Max(latency_frame_cnt_digit));
      }

      // print latest throughput for each stream
      PrintTitleForLatestThroughput();
      for (auto& it : latest_fps) {
        PrintStreamId(it.first);
        PrintThroughput(it.second, PerfUtils::Max(latest_frame_cnt_digit));
      }
      // calculate and print latest throughput for pipeline
      PerfStats stats = CalcLatestThroughput("", PerfManager::GetDefaultType(),
          {end_node + PerfManager::GetEndTimeSuffix()}, calculator, final_print);
      std::cout << "\n(* Note: Performance info of pipeline is slightly delayed compared to that of each stream.)\n";
      PrintStr("Pipeline : ");
      PrintThroughput(stats);
      // print average throughput for each stream
      PrintTitleForAverageThroughput();
      for (auto& it : entire_fps) {
        PrintStreamId(it.first);
        PrintThroughput(it.second, PerfUtils::Max(entire_frame_cnt_digit));
      }
      // print average throughput for pipeline
      PerfStats avg_fps = calculator->GetAvgThroughput("", PerfManager::GetDefaultType());
      std::cout << "\n(* Note: Performance info of pipeline is slightly delayed compared to that of each stream.)\n";
      PrintStr("Pipeline : ");
      PrintThroughput(avg_fps);
      double running_seconds = static_cast<double>(avg_fps.latency_max) / 1e6;
      std::cout << "\nRunning time (s):" << running_seconds << std::endl;
      if (final_print) { std::cout << "\nTotal : " << avg_fps.fps << std::endl; }
    }
  }
}

bool Pipeline::RemovePerfManager(std::string stream_id) {
  LOG(INFO) << "Remove PerfManager of stream " << stream_id;
  std::vector<std::pair<std::string, PerfStats>> latency_vec, latency_vec_pipe, fps_vec, avg_fps_vec;
  {
    RwLockWriteGuard lg(perf_managers_lock_);
    if (perf_managers_.find(stream_id) == perf_managers_.end()) {
      LOG(ERROR) << "Remove PerfManager failed. Not find PerfManager of " << stream_id;
      return false;
    }
    std::shared_ptr<PerfManager> manager = perf_managers_[stream_id];
    if (manager == nullptr) {
      LOG(ERROR) << "Remove PerfManager failed. The PerfManager of " << stream_id << " is nullptr.";
      return false;
    }
    manager->Stop();
    manager->SqlCommitTrans();
    {
      std::lock_guard<std::mutex> lg_calc(perf_calculation_lock_);
      for (auto& module_it : modules_map_) {
        std::string node_name = module_it.first;
        if (perf_calculators_.find(node_name) != perf_calculators_.end()) {
          std::shared_ptr<PerfCalculator> calculator = perf_calculators_[node_name];
          PerfStats latency_stats = calculator->CalcLatency(stream_id, PerfManager::GetDefaultType(),
                {node_name + PerfManager::GetStartTimeSuffix(), node_name + PerfManager::GetEndTimeSuffix()});
          latency_vec.push_back(std::make_pair(node_name, latency_stats));
          calculator->RemovePerfStats(stream_id, PerfManager::GetDefaultType(), node_name);
          calculator->GetPerfUtils()->RemoveSql(stream_id);
        }
      }

      for (auto& end_node : end_nodes_) {
        if (perf_calculators_.find("pipeline_" + end_node) != perf_calculators_.end()) {
          std::shared_ptr<PerfCalculator> calculator = perf_calculators_["pipeline_" + end_node];
          PerfStats latency_stats = calculator->CalcLatency(stream_id, PerfManager::GetDefaultType(),
                {start_node_ + PerfManager::GetStartTimeSuffix(), end_node + PerfManager::GetEndTimeSuffix()});
          PerfStats fps_stats = calculator->CalcThroughput(stream_id, PerfManager::GetDefaultType(),
                                                          {end_node + PerfManager::GetEndTimeSuffix()});
          PerfStats avg_fps = calculator->GetAvgThroughput(stream_id, PerfManager::GetDefaultType());
          latency_vec_pipe.push_back(std::make_pair(end_node, latency_stats));
          fps_vec.push_back(std::make_pair(end_node, fps_stats));
          avg_fps_vec.push_back(std::make_pair(end_node, avg_fps));
          calculator->RemovePerfStats(stream_id, PerfManager::GetDefaultType(),
                                      end_node + PerfManager::GetEndTimeSuffix());
          calculator->GetPerfUtils()->RemoveSql(stream_id);
        }
      }

      for (uint32_t i = 0; i < stream_ids_.size(); i++) {
        if (stream_ids_[i] == stream_id) {
          stream_ids_.erase(stream_ids_.begin() + i);
          break;
        }
      }
    }  // perf calculation lock end
    perf_managers_.erase(stream_id);
  }  // perf manager write lock end

  std::cout << "\033[1;31m" << "\n\n%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
            << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%" << "\033[0m" << std::endl;
  PrintTitle("Stream [" + stream_id + "] is removed");
  for (auto &it : latency_vec) {
    PrintStr(it.first);
    PrintLatency(it.second);
  }
  for (uint32_t i = 0; i < latency_vec_pipe.size(); i++) {
    PrintStr("Pipeline");
    PrintLatency(latency_vec_pipe[i].second);
    PrintTitleForLatestThroughput();
    PrintThroughput(fps_vec[i].second);
    PrintTitleForAverageThroughput();
    PrintThroughput(avg_fps_vec[i].second);
  }
  return true;
}

bool Pipeline::AddPerfManager(std::string stream_id, std::string db_dir) {
  std::shared_ptr<PerfManager> manager;
  {
    RwLockWriteGuard lg(perf_managers_lock_);
    LOG(INFO) << "Add PerfManager for stream " << stream_id;
    if (!perf_running_) {
      LOG(ERROR) << "Please CreatePerfManager first.";
      return false;
    }
    if (perf_managers_.find(stream_id) != perf_managers_.end()) {
      LOG(ERROR) << "Create perf manager failed, PerfManager of " << stream_id << " is exist.";
      return false;
    }

    if (db_dir.empty()) db_dir = "perf_database";
    std::vector<std::string> module_names = GetModuleNames();
    std::string db_name = db_dir + "/stream_" + stream_id + "_" + TimeStamp::CurrentToDate() + ".db";

    manager = PerfManager::CreateDefaultManager(db_name, module_names);
    if (manager == nullptr) { return false; }
    perf_managers_[stream_id] = manager;
  }  // perf manager write lock end
  {
    std::lock_guard<std::mutex> lg_calc(perf_calculation_lock_);
    for (auto calculator : perf_calculators_) {
      calculator.second->GetPerfUtils()->AddSql(stream_id, manager->GetSql());
    }
    stream_ids_.push_back(stream_id);
  }  // perf calculation lock end
  return true;
}

std::vector<std::string> Pipeline::GetModuleNames() {
  std::vector<std::string> module_names;
  for (auto& module_it : modules_) {
    const std::string node_name = module_it.first;
    module_names.push_back(node_name);
  }
  return module_names;
}

void Pipeline::SetStartAndEndNodeNames() {
  if (!start_node_.empty() && end_nodes_.size() != 0) return;
  end_nodes_.clear();
  for (auto& module_it : modules_) {
    const std::string node_name = module_it.first;
    if (module_it.second.input_connectors.size() == 0) {
      start_node_ = node_name;
    }
    if (module_it.second.output_connectors.size() == 0) {
      end_nodes_.push_back(node_name);
    }
  }
}

bool Pipeline::CreatePerfCalculator(std::string db_dir, std::string node_name, bool is_pipeline) {
  std::string name = is_pipeline ? "pipeline_" + node_name : node_name;
  if (perf_calculators_.find(name) != perf_calculators_.end()) {
    LOG(ERROR) << "perf calculator is created before. name : " << name;
    return false;
  }
  std::shared_ptr<PerfCalculator> calculator;
  std::vector<std::string> keys;
  if (is_pipeline) {
    calculator = std::make_shared<PerfCalculatorForPipeline>();
    keys = {PerfManager::GetEndTimeSuffix()};
  } else {
    calculator = std::make_shared<PerfCalculatorForModule>();
    keys = {PerfManager::GetStartTimeSuffix(), PerfManager::GetEndTimeSuffix(), PerfManager::GetThreadSuffix()};
  }

  for (auto manager : perf_managers_) {
    if (!calculator->AddDataBaseHandler(manager.first, manager.second->GetSql())) {
      return false;
    }
  }
  std::string db_name = db_dir + "/" + name + "_tmp_" + TimeStamp::CurrentToDate() + ".db";
  if (!calculator->CreateDbForStoreUnprocessedData(db_name, PerfManager::GetDefaultType(), node_name, keys)) {
    return false;
  }
  perf_calculators_[name] = calculator;
  return true;
}

PerfStats Pipeline::CalcLatestThroughput(std::string sql_name, std::string perf_type,
    std::vector<std::string> keys, std::shared_ptr<PerfCalculator> calculator, bool final_print) {
  PerfStats stats;
  if (final_print) {
    calculator->SetPrintThroughput(false);
    stats = calculator->CalculateFinalThroughput(sql_name, perf_type, keys);
    calculator->SetPrintThroughput(true);
  } else {
    stats = calculator->CalcThroughput(sql_name, perf_type, keys);
  }
  return stats;
}

std::unordered_map<std::string, std::shared_ptr<PerfManager>> Pipeline::GetPerfManagers() {
  RwLockReadGuard lg(perf_managers_lock_);
  return perf_managers_;
}

}  // namespace cnstream
