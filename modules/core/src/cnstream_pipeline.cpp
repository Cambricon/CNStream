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

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
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
#include "threadsafe_queue.hpp"

namespace cnstream {

bool CNModuleConfig::ParseByJSONStr(const std::string& jstr) {
  rapidjson::Document doc;
  if (doc.Parse<rapidjson::kParseCommentsFlag>(jstr.c_str()).HasParseError()) {
    LOG(ERROR) << "Parse module configuration failed. Error code [" << std::to_string(doc.GetParseError()) << "]"
               << " Offset [" << std::to_string(doc.GetErrorOffset()) << "]. JSON:" << jstr;
    return false;
  }

  /* get members */
  const auto end = doc.MemberEnd();

  // className
  if (end == doc.FindMember("class_name")) {
    LOG(ERROR) << "Module has to have a class_name.";
    return false;
  } else {
    if (!doc["class_name"].IsString()) {
      LOG(ERROR) << "class_name must be string type.";
      return false;
    }
    this->className = doc["class_name"].GetString();
  }

  // parallelism
  if (end != doc.FindMember("parallelism")) {
    if (!doc["parallelism"].IsUint()) {
      LOG(ERROR) << "parallelism must be uint type.";
      return false;
    }
    this->parallelism = doc["parallelism"].GetUint();
  } else {
    this->parallelism = 1;
  }

  // maxInputQueueSize
  if (end != doc.FindMember("max_input_queue_size")) {
    if (!doc["max_input_queue_size"].IsUint()) {
      LOG(ERROR) << "max_input_queue_size must be uint type.";
      return false;
    }
    this->maxInputQueueSize = doc["max_input_queue_size"].GetUint();
  } else {
    this->maxInputQueueSize = 20;
  }

  // enablePerfInfo
  if (end != doc.FindMember("show_perf_info")) {
    if (!doc["show_perf_info"].IsBool()) {
      LOG(ERROR) << "show_perf_info must be Boolean type.";
      return false;
    }
    this->showPerfInfo = doc["show_perf_info"].GetBool();
  } else {
    this->showPerfInfo = false;
  }

  // next
  if (end != doc.FindMember("next_modules")) {
    if (!doc["next_modules"].IsArray()) {
      LOG(ERROR) << "next_modules must be array type.";
      return false;
    }
    auto values = doc["next_modules"].GetArray();
    for (auto iter = values.begin(); iter != values.end(); ++iter) {
      if (!iter->IsString()) {
        LOG(ERROR) << "next_modules must be an array of strings.";
        return false;
      }
      this->next.push_back(iter->GetString());
    }
  } else {
    this->next = {};
  }

  // custom parameters
  if (end != doc.FindMember("custom_params")) {
    rapidjson::Value& custom_params = doc["custom_params"];
    if (!custom_params.IsObject()) {
      LOG(ERROR) << "custom_params must be an object.";
      return false;
    }
    this->parameters.clear();
    for (auto iter = custom_params.MemberBegin(); iter != custom_params.MemberEnd(); ++iter) {
      std::string value;
      if (!iter->value.IsString()) {
        rapidjson::StringBuffer sbuf;
        rapidjson::Writer<rapidjson::StringBuffer> jwriter(sbuf);
        iter->value.Accept(jwriter);
        value = sbuf.GetString();
      } else {
        value = iter->value.GetString();
      }
      this->parameters.insert(std::make_pair(iter->name.GetString(), value));
    }
  } else {
    this->parameters = {};
  }
  return true;
}

bool CNModuleConfig::ParseByJSONFile(const std::string& jfname) {
  std::ifstream ifs(jfname);

  if (!ifs.is_open()) {
    LOG(ERROR) << "File open failed :" << jfname;
    return false;
  }

  std::string jstr((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  ifs.close();

  if (!ParseByJSONStr(jstr)) {
    return false;
  }

  /***************************************************
   * add config file path to custom parameters
   ***************************************************/

  std::string jf_dir = "";
  auto slash_pos = jfname.rfind("/");
  if (slash_pos == std::string::npos) {
    jf_dir = ".";
  } else {
    jf_dir = jfname.substr(0, slash_pos);
  }
  jf_dir += '/';

  if (this->parameters.end() != this->parameters.find(CNS_JSON_DIR_PARAM_NAME)) {
    LOG(WARNING) << "Parameter [" << CNS_JSON_DIR_PARAM_NAME << "] does not take effect. It is set "
                 << "up by cnstream as the directory where the configuration file is located and passed to the module.";
  }

  this->parameters[CNS_JSON_DIR_PARAM_NAME] = jf_dir;
  return true;
}

struct ModuleAssociatedInfo {
  std::shared_ptr<Module> instance;
  uint32_t parallelism = 0;
  std::shared_ptr<Connector> connector;
  std::set<std::string> down_nodes;
  std::vector<std::string> input_connectors;
  std::vector<std::string> output_connectors;
};

StreamMsgObserver::~StreamMsgObserver() {}

class PipelinePrivate {
 private:
  explicit PipelinePrivate(Pipeline* q_ptr) : q_ptr_(q_ptr) {
    // stream message handle thread
    exit_msg_loop_ = false;
    smsg_thread_ = std::thread(&PipelinePrivate::StreamMsgHandleFunc, this);
  }
  ~PipelinePrivate() {
    exit_msg_loop_ = true;
    if (smsg_thread_.joinable()) smsg_thread_.join();
  }
  std::unordered_map<std::string, std::shared_ptr<Connector>> links_;
  std::vector<std::thread> threads_;
  std::thread event_thread_;
  std::map<std::string, ModuleAssociatedInfo> modules_;
  std::mutex stop_mtx_;
  uint64_t eos_mask_ = 0;

 private:
  std::unordered_map<std::string, std::shared_ptr<PerfManager>> perf_managers_;
  std::vector<std::string> stream_ids_;
  std::vector<std::string> end_nodes_;
  std::thread perf_commit_thread_;
  std::thread calculate_perf_thread_;
  std::atomic<bool> perf_running_{false};

 private:
  std::unordered_map<std::string, CNModuleConfig> modules_config_;
  std::unordered_map<std::string, std::vector<std::string>> connections_config_;
  std::map<std::string, std::shared_ptr<Module>> modules_map_;
  DECLARE_PUBLIC(q_ptr_, Pipeline);
  void SetEOSMask() {
    for (const std::pair<std::string, ModuleAssociatedInfo> module_info : modules_) {
      auto instance = module_info.second.instance;
      eos_mask_ |= (uint64_t)1 << instance->GetId();
    }
  }
  void ClearEOSMask() { eos_mask_ = 0; }

  /*
    stream message
   */
  void UpdateByStreamMsg(const StreamMsg& msg) {
    LOG(INFO) << "[" << q_ptr_->GetName() << "] got stream message: " << msg.type << " " << msg.chn_idx << " "
              << msg.stream_id;
    msgq_.Push(msg);
  }
  void StreamMsgHandleFunc() {
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
          LOG(INFO) << "[" << q_ptr_->GetName() << "] notify stream message: " << msg.type << " " << msg.chn_idx << " "
                    << msg.stream_id;
          q_ptr_->NotifyStreamMsg(msg);
          break;
        default:
          break;
      }
    }
  }

  ThreadSafeQueue<StreamMsg> msgq_;
  std::thread smsg_thread_;
  volatile bool exit_msg_loop_ = false;
};  // class PipelinePrivate

Pipeline::Pipeline(const std::string& name) : Module(name) {
  d_ptr_ = new (std::nothrow) PipelinePrivate(this);
  LOG_IF(FATAL, nullptr == d_ptr_) << "Pipeline::Pipeline() failed to alloc PipelinePrivate";

  event_bus_ = new (std::nothrow) EventBus();
  LOG_IF(FATAL, nullptr == event_bus_) << "Pipeline::Pipeline() failed to alloc EventBus";
  GetEventBus()->AddBusWatch(std::bind(&Pipeline::DefaultBusWatch, this, std::placeholders::_1, std::placeholders::_2),
                             this);
}

Pipeline::~Pipeline() {
  running_ = false;
  delete event_bus_;
  delete d_ptr_;
}

bool Pipeline::Open(ModuleParamSet paramSet) {
  (void)paramSet;
  return true;
}
void Pipeline::Close() {}
int Pipeline::Process(std::shared_ptr<CNFrameInfo> data) { return 0; }

EventHandleFlag Pipeline::DefaultBusWatch(const Event& event, Module* module) {
  StreamMsg smsg;
  EventHandleFlag ret;
  switch (event.type) {
    case EventType::EVENT_ERROR:
      smsg.type = ERROR_MSG;
      smsg.chn_idx = -1;
      d_ptr_->UpdateByStreamMsg(smsg);
      LOG(ERROR) << "[" << event.module->GetName() << "]: "
                 << "Error: " << event.message;
      ret = EVENT_HANDLE_STOP;
      break;
    case EventType::EVENT_WARNING:
      LOG(WARNING) << "[" << event.module->GetName() << "]: "
                   << "Warning: " + event.message;
      ret = EVENT_HANDLE_SYNCED;
      break;
    case EventType::EVENT_STOP:
      LOG(INFO) << "[" << event.module->GetName() << "]: "
                << "Info: " << event.message;
      ret = EVENT_HANDLE_STOP;
      break;
    case EventType::EVENT_EOS: {
      LOG(INFO) << "Pipeline received eos from module (" + event.module->GetName() << ")"
                << " thread " << event.thread_id;
      ret = EVENT_HANDLE_SYNCED;
      break;
    }
    case EventType::EVENT_INVALID:
      LOG(ERROR) << "[" << event.module->GetName() << "]: "
                 << "Info: " << event.message;
    default:
      ret = EVENT_HANDLE_NULL;
      break;
  }
  return ret;
}

bool Pipeline::ProvideData(const Module* module, std::shared_ptr<CNFrameInfo> data) {
  std::string moduleName = module->GetName();

  if (d_ptr_->modules_.find(moduleName) == d_ptr_->modules_.end()) return false;

  TransmitData(moduleName, data);

  return true;
}

bool Pipeline::AddModule(std::shared_ptr<Module> module) {
  std::string moduleName = module->GetName();

  if (d_ptr_->modules_.find(moduleName) != d_ptr_->modules_.end()) {
    LOG(WARNING) << "Module [" << module->GetName() << "] has already been added to this pipeline";
    return false;
  }

  LOG(INFO) << "Add Module " << module->GetName() << " to pipeline";
  if (module->GetId() == INVALID_MODULE_ID) {
    LOG(ERROR) << "Failed to get module Id";
    return false;
  }

  ModuleAssociatedInfo associated_info;
  associated_info.instance = module;
  associated_info.parallelism = 1;
  associated_info.connector = std::make_shared<Connector>(associated_info.parallelism);
  module->SetContainer(this);
  d_ptr_->modules_.insert(std::make_pair(moduleName, associated_info));

  return true;
}

bool Pipeline::SetModuleAttribute(std::shared_ptr<Module> module, uint32_t parallelism, size_t queue_capacity) {
  std::string moduleName = module->GetName();
  if (d_ptr_->modules_.find(moduleName) == d_ptr_->modules_.end()) return false;
  d_ptr_->modules_[moduleName].parallelism = parallelism;
  if (parallelism && queue_capacity) {
    d_ptr_->modules_[moduleName].connector = std::make_shared<Connector>(parallelism, queue_capacity);
    return static_cast<bool>(d_ptr_->modules_[moduleName].connector);
  }
  if (!parallelism && d_ptr_->modules_[moduleName].connector) {
    d_ptr_->modules_[moduleName].connector.reset();
  }
  return true;
}

uint32_t Pipeline::GetModuleParallelism(std::shared_ptr<Module> module) {
  std::string moduleName = module->GetName();
  if (d_ptr_->modules_.find(moduleName) == d_ptr_->modules_.end()) return 0;
  return d_ptr_->modules_[moduleName].parallelism;
}

std::string Pipeline::LinkModules(std::shared_ptr<Module> up_node, std::shared_ptr<Module> down_node) {
  if (up_node == nullptr || down_node == nullptr) {
    return "";
  }

  std::string up_node_name = up_node->GetName();
  std::string down_node_name = down_node->GetName();

  if (d_ptr_->modules_.find(up_node_name) == d_ptr_->modules_.end() ||
      d_ptr_->modules_.find(down_node_name) == d_ptr_->modules_.end()) {
    LOG(ERROR) << "module has not been added to this pipeline";
    return "";
  }

  ModuleAssociatedInfo& up_node_info = d_ptr_->modules_.find(up_node_name)->second;
  ModuleAssociatedInfo& down_node_info = d_ptr_->modules_.find(down_node_name)->second;

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
  d_ptr_->links_[link_id] = down_node_info.connector;

  down_node->SetParentId(up_node->GetId());
  return link_id;
}

bool Pipeline::QueryLinkStatus(LinkStatus* status, const std::string& link_id) {
  std::shared_ptr<Connector> con = d_ptr_->links_[link_id];
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
  d_ptr_->SetEOSMask();
  // open modules
  std::vector<std::shared_ptr<Module>> opened_modules;
  bool open_module_failed = false;
  for (auto& it : d_ptr_->modules_) {
    if (!it.second.instance->Open(GetModuleParamSet(it.second.instance->GetName()))) {
      open_module_failed = true;
      LOG(ERROR) << it.second.instance->GetName() << " start failed!";
      break;
    } else {
      opened_modules.push_back(it.second.instance);
    }
  }

  if (open_module_failed) {
    for (auto it : opened_modules) it->Close();
    d_ptr_->ClearEOSMask();
    return false;
  }

  if (d_ptr_->perf_running_) {
    for (auto it : d_ptr_->perf_managers_) {
      it.second->SqlBeginTrans();
    }
    d_ptr_->perf_commit_thread_ = std::thread(&Pipeline::PerfSqlCommitLoop, this);
    d_ptr_->calculate_perf_thread_ = std::thread(&Pipeline::CalculatePerfStats, this);
  }

  // start data transmit
  running_.store(true);
  event_bus_->running_.store(true);
  d_ptr_->event_thread_ = std::thread(&Pipeline::EventLoop, this);

  for (const std::pair<std::string, ModuleAssociatedInfo>& it : d_ptr_->modules_) {
    if (it.second.connector) {
      it.second.connector->Start();
    }
  }

  // create process threads
  for (auto& it : d_ptr_->modules_) {
    const std::string node_name = it.first;
    ModuleAssociatedInfo& module_info = it.second;
    uint32_t parallelism = module_info.parallelism;
    if ((!parallelism && module_info.connector) || (parallelism && !module_info.connector) ||
        (parallelism && module_info.connector && parallelism != module_info.connector->GetConveyorCount())) {
      LOG(INFO) << "Module parallelism do not equal input Connector's Conveyor number, name: "
                << module_info.instance->GetName();
      return false;
    }
    for (uint32_t conveyor_idx = 0; conveyor_idx < parallelism; ++conveyor_idx) {
      d_ptr_->threads_.push_back(std::thread(&Pipeline::TaskLoop, this, node_name, conveyor_idx));
    }
  }
  LOG(INFO) << "Pipeline Start";
  LOG(INFO) << "Total Module's threads :" << d_ptr_->threads_.size();
  return true;
}

bool Pipeline::Stop() {
  std::lock_guard<std::mutex> lk(d_ptr_->stop_mtx_);
  if (!IsRunning()) return true;

  // stop data transmit
  for (const std::pair<std::string, ModuleAssociatedInfo>& it : d_ptr_->modules_) {
    if (it.second.connector) {
      it.second.connector->EmptyDataQueue();
      it.second.connector->Stop();
    }
  }
  running_.store(false);
  event_bus_->running_.store(false);
  for (std::thread& it : d_ptr_->threads_) {
    if (it.joinable()) it.join();
  }
  d_ptr_->threads_.clear();
  if (d_ptr_->event_thread_.joinable()) {
    d_ptr_->event_thread_.join();
  }

  for (auto it : d_ptr_->perf_managers_) {
    it.second->Stop();
    it.second = nullptr;
  }
  d_ptr_->perf_running_.store(false);
  if (d_ptr_->perf_commit_thread_.joinable()) {
    d_ptr_->perf_commit_thread_.join();
  }
  if (d_ptr_->calculate_perf_thread_.joinable()) {
    d_ptr_->calculate_perf_thread_.join();
  }

  d_ptr_->perf_managers_.clear();

  // close modules
  for (auto& it : d_ptr_->modules_) {
    it.second.instance->Close();
    it.second.instance->ClearPerfManagers();
  }

  d_ptr_->ClearEOSMask();
  LOG(INFO) << "Pipeline Stop";
  return true;
}

EventBus* Pipeline::GetEventBus() const { return event_bus_; }

void Pipeline::EventLoop() {
  const std::list<std::pair<BusWatcher, Module*>>& kWatchers = event_bus_->GetBusWatchers();
  EventHandleFlag flag = EVENT_HANDLE_NULL;

  SetThreadName("cn-EventLoop", pthread_self());
  // start loop
  while (event_bus_->IsRunning()) {
    Event event = event_bus_->PollEvent();
    if (event.type == EVENT_INVALID) {
      LOG(INFO) << "[EventLoop] event type is invalid";
      break;
    } else if (event.type == EVENT_STOP) {
      LOG(INFO) << "[EventLoop] Get stop event";
      break;
    }
    std::unique_lock<std::mutex> lk(event_bus_->watcher_mut_);
    for (auto& watcher : kWatchers) {
      flag = watcher.first(event, watcher.second);
      if (flag == EVENT_HANDLE_INTERCEPTION || flag == EVENT_HANDLE_STOP) {
        break;
      }
    }
    if (flag == EVENT_HANDLE_STOP) {
      break;
    }
  }
  LOG(INFO) << "[" << GetName() << "]: Event bus exit.";
}

void Pipeline::TransmitData(std::string moduleName, std::shared_ptr<CNFrameInfo> data) {
  LOG_IF(FATAL, d_ptr_->modules_.find(moduleName) == d_ptr_->modules_.end());

  const ModuleAssociatedInfo& module_info = d_ptr_->modules_[moduleName];

  const uint32_t chn_idx = data->channel_idx;
  /*
    eos
   */
  if (data->frame.flags & CN_FRAME_FLAG_EOS) {
    LOG(INFO) << "[" << module_info.instance->GetName() << "]"
              << " Channel " << data->channel_idx << " got eos.";
    Event e;
    e.type = EventType::EVENT_EOS;
    e.module = module_info.instance.get();
    e.message = module_info.instance->GetName() + " received eos from channel " + std::to_string(chn_idx);
    e.thread_id = std::this_thread::get_id();
    event_bus_->PostEvent(e);
    const uint64_t eos_mask = data->AddEOSMask(module_info.instance.get());
    if (eos_mask == d_ptr_->eos_mask_) {
      StreamMsg msg;
      msg.type = StreamMsgType::EOS_MSG;
      msg.chn_idx = chn_idx;
      msg.stream_id = data->frame.stream_id;
      d_ptr_->UpdateByStreamMsg(msg);
    }
  } else {
    if (d_ptr_->perf_managers_.find(data->frame.stream_id) != d_ptr_->perf_managers_.end()) {
      d_ptr_->perf_managers_[data->frame.stream_id]->Record(true, "PROCESS", moduleName, data->frame.timestamp);
    }
  }

  for (auto& down_node_name : module_info.down_nodes) {
    ModuleAssociatedInfo& down_node_info = d_ptr_->modules_.find(down_node_name)->second;
    assert(down_node_info.connector);
    assert(0 < down_node_info.input_connectors.size());
    uint64_t frame_mask = data->SetModuleMask(down_node_info.instance.get(), module_info.instance.get());

    // case 1: down_node has only 1 input node: current node
    // case 2: down_node has >1 input nodes, current node has brother nodes
    // the processing data frame will not be pushed into down_node Connector
    // until processed by all brother nodes, the last node responds to transmit
    bool processed_by_all_modules = frame_mask == down_node_info.instance->GetModulesMask();

    if (processed_by_all_modules) {
      std::shared_ptr<Connector> connector = down_node_info.connector;
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
  LOG_IF(FATAL, d_ptr_->modules_.find(node_name) == d_ptr_->modules_.end());

  ModuleAssociatedInfo& module_info = d_ptr_->modules_[node_name];
  std::shared_ptr<Connector> connector = module_info.connector;

  if (!connector.get() || module_info.input_connectors.size() <= 0) {
    return;
  }

  size_t len = node_name.size() > 10 ? 10 : node_name.size();
  std::string thread_name = "cn-" + node_name.substr(0, len) + std::to_string(conveyor_idx);
  SetThreadName(thread_name, pthread_self());

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
    assert(data->GetModulesMask(module_info.instance.get()) == module_info.instance->GetModulesMask());

    data->ClearModuleMask(module_info.instance.get());
    int flags = data->frame.flags;

    if (!module_info.instance->HasTransmit() && (CN_FRAME_FLAG_EOS & flags)) {
      /*normal module, transmit EOS by the framework*/
      TransmitData(node_name, data);
      continue;
    }

    {
      if (!(data->frame.flags & CN_FRAME_FLAG_EOS)
          && d_ptr_->perf_managers_.find(data->frame.stream_id) != d_ptr_->perf_managers_.end()) {
        d_ptr_->perf_managers_[data->frame.stream_id]->Record(false, "PROCESS", node_name, data->frame.timestamp);
      }

      int ret = module_info.instance->DoProcess(data);
      /*process failed*/
      if (ret < 0) {
        Event e;
        e.type = EventType::EVENT_ERROR;
        e.module = module_info.instance.get();
        e.message = module_info.instance->GetName() + " process failed, return number: " + std::to_string(ret);
        e.thread_id = std::this_thread::get_id();
        event_bus_->PostEvent(e);
        StreamMsg msg;
        msg.type = StreamMsgType::ERROR_MSG;
        msg.chn_idx = data->channel_idx;
        msg.stream_id = data->frame.stream_id;
        d_ptr_->UpdateByStreamMsg(msg);
        return;
      } else if (ret > 0) {
        // data has been transmitted by the module itself
        if (!module_info.instance->HasTransmit()) {
          LOG(ERROR) << "Module::Process() should not return 1\n";
          return;
        }
        continue;
      }
    }
    if (!module_info.instance->HasTransmit()) {
      TransmitData(node_name, data);
    }
  }  // while
}

/* ------config/auto-graph methods------ */
int Pipeline::AddModuleConfig(const CNModuleConfig& config) {
  if (d_ptr_ == nullptr) {
    return -1;
  }
  d_ptr_->modules_config_[config.name] = config;
  d_ptr_->connections_config_[config.name] = config.next;
  return 0;
}

ModuleParamSet Pipeline::GetModuleParamSet(const std::string& moduleName) {
  ModuleParamSet paramSet;
  auto iter = d_ptr_->modules_config_.find(moduleName);
  if (iter != d_ptr_->modules_config_.end()) {
    for (auto& v : iter->second.parameters) {
      // filter some keys ...
      paramSet[v.first] = v.second;
    }
  }
  return paramSet;
}

CNModuleConfig Pipeline::GetModuleConfig(const std::string& module_name) {
  CNModuleConfig config = {};
  auto iter = d_ptr_->modules_config_.find(module_name);
  if (iter != d_ptr_->modules_config_.end()) {
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
    d_ptr_->modules_map_[v.name] = instance;
    this->AddModule(instance);
    this->SetModuleAttribute(instance, v.parallelism, v.maxInputQueueSize);
  }
  for (auto& v : d_ptr_->connections_config_) {
    for (auto& name : v.second) {
      if (this->LinkModules(d_ptr_->modules_map_[v.first], d_ptr_->modules_map_[name]).empty()) {
        LOG(ERROR) << "Link [" << v.first << "] with [" << name << "] failed.";
        return -1;
      }
      linked_id_mask |= (uint64_t)1 << d_ptr_->modules_map_[name]->GetId();
    }
  }
  for (auto& v : configs) {
    if (v.className != "cnstream::DataSource" &&
        v.className != "cnstream::ModuleIPC" &&
        !(((uint64_t)1 << d_ptr_->modules_map_[v.name]->GetId()) & linked_id_mask)) {
      LOG(ERROR) << v.name << " not linked to any module.";
      return -1;
    }
  }
  return 0;
}

int Pipeline::BuildPipelineByJSONFile(const std::string& config_file) {
  std::ifstream ifs(config_file);
  if (!ifs.is_open()) {
    LOG(ERROR) << "Failed to open file: " << config_file;
    return -1;
  }

  std::string jstr((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  ifs.close();

  /* traversing modules */
  std::vector<CNModuleConfig> mconfs;
  std::vector<std::string> namelist;
  rapidjson::Document doc;
  if (doc.Parse<rapidjson::kParseCommentsFlag>(jstr.c_str()).HasParseError()) {
    LOG(ERROR) << "Parse pipeline configuration failed. Error code [" << std::to_string(doc.GetParseError()) << "]"
               << " Offset [" << std::to_string(doc.GetErrorOffset()) << "]. ";
    return -1;
  }

  for (rapidjson::Document::ConstMemberIterator iter = doc.MemberBegin(); iter != doc.MemberEnd(); ++iter) {
    CNModuleConfig mconf;
    mconf.name = iter->name.GetString();
    if (find(namelist.begin(), namelist.end(), mconf.name) != namelist.end()) {
      LOG(ERROR) << "Module name should be unique in Jason file. Module name : [" << mconf.name + "]"
                 << " appeared more than one time.";
      return -1;
    }
    namelist.push_back(mconf.name);

    rapidjson::StringBuffer sbuf;
    rapidjson::Writer<rapidjson::StringBuffer> jwriter(sbuf);
    iter->value.Accept(jwriter);

    if (!mconf.ParseByJSONStr(std::string(sbuf.GetString()))) {
      LOG(ERROR) << "Parse module config failed. Module name : [" << mconf.name << "]";
      return -1;
    }

    /***************************************************
     * add config file path to custom parameters
     ***************************************************/

    std::string jf_dir = "";
    auto slash_pos = config_file.rfind("/");
    if (slash_pos == std::string::npos) {
      jf_dir = ".";
    } else {
      jf_dir = config_file.substr(0, slash_pos);
    }
    jf_dir += '/';

    if (mconf.parameters.end() != mconf.parameters.find(CNS_JSON_DIR_PARAM_NAME)) {
      LOG(WARNING)
          << "Parameter [" << CNS_JSON_DIR_PARAM_NAME << "] does not take effect. It is set "
          << "up by cnstream as the directory where the configuration file is located and passed to the module.";
    }

    mconf.parameters[CNS_JSON_DIR_PARAM_NAME] = jf_dir;
    mconfs.push_back(mconf);
  }

  return BuildPipeline(mconfs);
}

Module* Pipeline::GetModule(const std::string& moduleName) {
  auto iter = d_ptr_->modules_map_.find(moduleName);
  if (iter != d_ptr_->modules_map_.end()) {
    return d_ptr_->modules_map_[moduleName].get();
  }
  return nullptr;
}

std::vector<std::string> Pipeline::GetLinkIds() {
  std::vector<std::string> linkIds;
  for (auto& v : d_ptr_->links_) {
    linkIds.push_back(v.first);
  }
  return linkIds;
}

/* stream message methods */
void Pipeline::NotifyStreamMsg(const StreamMsg& smsg) {
  if (smsg_observer_) {
    smsg_observer_->Update(smsg);
  }
}

bool Pipeline::CreatePerfManager(std::vector<std::string> stream_ids, std::string db_dir) {
  if (d_ptr_->perf_running_) return false;
  std::vector<std::string> module_names;
  std::string start_node_name;
  if (db_dir == "") {
    db_dir = "perf_database";
  }
  d_ptr_->end_nodes_.clear();
  for (auto& it : d_ptr_->modules_) {
    const std::string node_name = it.first;
    LOG(INFO) << "module name " << node_name << std::endl;
    module_names.push_back(node_name);
    if (it.second.input_connectors.size() == 0) {
      start_node_name = node_name;
    }
    if (it.second.output_connectors.size() == 0) {
      d_ptr_->end_nodes_.push_back(node_name);
    }
  }
  for (auto it : stream_ids) {
    LOG(INFO) << "Create PerfManager for stream " << it;
    d_ptr_->perf_managers_[it] = std::make_shared<PerfManager>();
    if (!d_ptr_->perf_managers_[it]->Init(db_dir + "/stream_" + it + ".db", module_names, start_node_name,
                                          d_ptr_->end_nodes_)) {
      return false;
    }
  }

  for (auto& it : d_ptr_->modules_) {
    it.second.instance->SetPerfManagers(d_ptr_->perf_managers_);
  }

  d_ptr_->stream_ids_ = stream_ids;
  d_ptr_->perf_running_.store(true);
  return true;
}

void Pipeline::CalculatePerfStats() {
  while (d_ptr_->perf_running_) {
    CalculateModulePerfStats();
    std::cout << "\n" <<std::endl;
    CalculatePipelinePerfStats();
    sleep(2);
    std::cout << "\n\n" <<std::endl;
  }
  std::cout << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
            << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%" << std::endl;
  CalculateModulePerfStats();
  std::cout << "\n" <<std::endl;
  CalculatePipelinePerfStats();
}

void Pipeline::PerfSqlCommitLoop() {
  while (d_ptr_->perf_running_) {
    for (auto it : d_ptr_->perf_managers_) {
      it.second->SqlCommitTrans();
      it.second->SqlBeginTrans();
    }
    sleep(1);
  }
  for (auto it : d_ptr_->perf_managers_) {
    it.second->SqlCommitTrans();
  }
}

void Pipeline::CalculateModulePerfStats() {
  for (auto& module_it : d_ptr_->modules_) {
    std::string node_name = module_it.first;
    const ModuleAssociatedInfo& module_info = module_it.second;
    if (module_info.instance && module_info.instance->ShowPerfInfo()) {
      std::cout << "---------------------------------"
        << std::setw(15) << std::setfill('-') << "[ " + node_name << " Performance ]"
        << "-----------------------------------" << std::endl;
      for (auto stream_id : d_ptr_->stream_ids_) {
        if (d_ptr_->perf_managers_.find(stream_id) != d_ptr_->perf_managers_.end()) {
          std::cout << std::setw(2) << std::setfill(' ') << stream_id;
          PrintPerfStats(d_ptr_->perf_managers_[stream_id]->CalculatePerfStats("PROCESS", node_name));
        }
      }  // for each stream
    }
  }  // for each module
}

void Pipeline::CalculatePipelinePerfStats() {
  std::vector<std::pair<std::string, std::vector<std::pair<std::string, PerfStats>>>> pipeline_l;
  std::cout << "\033[32m"
            << "-------------------------------------[ Pipeline Performance ]"
            << "-------------------------------------" << "\033[0m" << std::endl;

  for (auto stream_id : d_ptr_->stream_ids_) {
    if (d_ptr_->perf_managers_.find(stream_id) != d_ptr_->perf_managers_.end()) {
      pipeline_l.push_back(std::make_pair(stream_id,
          d_ptr_->perf_managers_[stream_id]->CalculatePipelinePerfStats("PROCESS")));
    }
  }

  double total_fps;
  for (uint32_t i = 0; i < d_ptr_->end_nodes_.size(); i++) {
    total_fps = 0.f;
    std::cout << "End node * * " << d_ptr_->end_nodes_[i] << " * *" << std::endl;
    for (auto it : pipeline_l) {
      std::cout << std::setw(2) << std::setfill(' ') << it.first;
      PrintPerfStats(it.second[i].second);
      total_fps += it.second[i].second.fps;
    }
    std::cout << "Total fps:" << total_fps << std::endl;
  }
}

}  // namespace cnstream
