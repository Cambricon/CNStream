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
#include "cnstream_timer.hpp"
#include "connector.hpp"
#include "conveyor.hpp"
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
  d_ptr_ = new(std::nothrow) PipelinePrivate(this);
  LOG_IF(FATAL, nullptr == d_ptr_) << "Pipeline::Pipeline() failed to alloc PipelinePrivate";

  event_bus_ = new(std::nothrow) EventBus();
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
  module->SetContainer(this);
  d_ptr_->modules_.insert(std::make_pair(moduleName, associated_info));

  return true;
}

bool Pipeline::SetModuleParallelism(std::shared_ptr<Module> module, uint32_t parallelism) {
  std::string moduleName = module->GetName();
  if (d_ptr_->modules_.find(moduleName) == d_ptr_->modules_.end()) return false;
  d_ptr_->modules_[moduleName].parallelism = parallelism;
  return true;
}

uint32_t Pipeline::GetModuleParallelism(std::shared_ptr<Module> module) {
  std::string moduleName = module->GetName();
  if (d_ptr_->modules_.find(moduleName) == d_ptr_->modules_.end()) return 0;
  return d_ptr_->modules_[moduleName].parallelism;
}

std::string Pipeline::LinkModules(std::shared_ptr<Module> up_node, std::shared_ptr<Module> down_node,
                                  size_t queue_capacity) {
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
  auto ret = up_node_info.down_nodes.insert(down_node_name);
  if (!ret.second) {
    LOG(ERROR) << "modules have been linked already";
    return link_id;
  }

  LOG(INFO) << "Link Module " << link_id;

  // create connector
  std::shared_ptr<Connector> con = std::make_shared<Connector>(down_node_info.parallelism, queue_capacity);
  up_node_info.output_connectors.push_back(link_id);
  down_node_info.input_connectors.push_back(link_id);
  d_ptr_->links_[link_id] = con;

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

  // start data transmit
  running_.store(true);
  event_bus_->running_.store(true);
  d_ptr_->event_thread_ = std::thread(&Pipeline::EventLoop, this);

  for (std::pair<std::string, std::shared_ptr<Connector>> connector : d_ptr_->links_) {
    connector.second->Start();
  }

  // create process threads
  for (auto& it : d_ptr_->modules_) {
    const std::string node_name = it.first;
    ModuleAssociatedInfo& module_info = it.second;
    uint32_t parallelism = module_info.parallelism;
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
  for (std::pair<std::string, std::shared_ptr<Connector>> connector : d_ptr_->links_) {
    connector.second->Stop();
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
  // close modules
  for (auto& it : d_ptr_->modules_) {
    it.second.instance->Close();
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

void Pipeline::PrintPerformanceInformation() const {
  std::cout << "\nPipeline Performance information:\n";
  for (const auto& it : d_ptr_->modules_) {
    const ModuleAssociatedInfo& module_info = it.second;
    if (module_info.instance) {
      module_info.instance->PrintPerfInfo();
    }
  }
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
    const uint64_t eos_mask = data->frame.AddEOSMask(module_info.instance.get());
    if (eos_mask == d_ptr_->eos_mask_) {
      StreamMsg msg;
      msg.type = StreamMsgType::EOS_MSG;
      msg.chn_idx = chn_idx;
      msg.stream_id = data->frame.stream_id;
      d_ptr_->UpdateByStreamMsg(msg);
    }
  }

  /*
    set module mask
   */
  for (auto& down_node_name : module_info.down_nodes) {
    ModuleAssociatedInfo& down_node_info = d_ptr_->modules_.find(down_node_name)->second;
    data->frame.SetModuleMask(down_node_info.instance.get(), module_info.instance.get());
  }

  // broadcast
  const std::vector<std::string>& connector_ids = module_info.output_connectors;
  for (auto& id : connector_ids) {
    std::shared_ptr<Connector>& connector = d_ptr_->links_[id];
    int conveyor_idx = chn_idx % connector->GetConveyorCount();
    connector->PushDataBufferToConveyor(conveyor_idx, data);
  }
}

void Pipeline::TaskLoop(std::string node_name, uint32_t conveyor_idx) {
  LOG_IF(FATAL, d_ptr_->modules_.find(node_name) == d_ptr_->modules_.end());

  ModuleAssociatedInfo& module_info = d_ptr_->modules_[node_name];

  std::vector<std::shared_ptr<Connector>> input_connectors;
  std::transform(module_info.input_connectors.begin(), module_info.input_connectors.end(),
                 std::back_inserter(input_connectors),
                 [&](std::string idx) -> std::shared_ptr<Connector> { return d_ptr_->links_[idx]; });

  if (input_connectors.size() == 0) return;

  size_t len = node_name.size() > 10 ? 10 : node_name.size();
  std::string thread_name = "cn-" + node_name.substr(0, len) + std::to_string(conveyor_idx);
  SetThreadName(thread_name, pthread_self());

  bool has_data = true;
  while (has_data) {
    has_data = false;
    std::shared_ptr<CNFrameInfo> data;
    // sync data
    for (std::shared_ptr<Connector> connector : input_connectors) {
      data = connector->PopDataBufferFromConveyor(conveyor_idx);
      if (nullptr == data.get()) {
        /*
          nullptr will be received when connector stops.
          maybe only part of the connectors stopped.
         */
        continue;
      }

      has_data = true;

      if (data->frame.GetModulesMask(module_info.instance.get()) == module_info.instance->GetModulesMask()) {
        data->frame.ClearModuleMask(module_info.instance.get());
        int flags = data->frame.flags;

        if (!module_info.instance->hasTranmit() && (CN_FRAME_FLAG_EOS & flags)) {
          /*normal module, transmit EOS by the framework*/
          TransmitData(node_name, data);
          continue;
        }

        {
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
            if (!module_info.instance->hasTranmit()) {
              LOG(ERROR) << "Module::Process() should not return 1\n";
              return;
            }
            continue;
          }
        }
        TransmitData(node_name, data);
      } else {
        // LOG(INFO) << std::hex << data->frame.GetModulesMask(module_info.instance.get()) << " : " <<
        // module_info.instance->GetModulesMask();
      }
    }  // for
  }    // while
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
  ModuleCreatorWorker creator;
  std::map<std::string, int> queues_size;
  for (auto& v : configs) {
    this->AddModuleConfig(v);
    Module* module = creator.Create(v.className, v.name);
    if (!module) {
      LOG(ERROR) << "Failed to create module by className(" << v.className << ") and name(" << v.name << ")";
      return -1;
    }
    module->ShowPerfInfo(v.showPerfInfo);

    std::shared_ptr<Module> instance(module);
    d_ptr_->modules_map_[v.name] = instance;

    queues_size[v.name] = v.maxInputQueueSize;
    this->AddModule(instance);
    this->SetModuleParallelism(instance, v.parallelism);
  }
  for (auto& v : d_ptr_->connections_config_) {
    for (auto& name : v.second) {
      if (this->LinkModules(d_ptr_->modules_map_[v.first], d_ptr_->modules_map_[name], queues_size[name]).empty()) {
        LOG(ERROR) << "Link [" << v.first << "] with [" << name << "] failed.";
        return -1;
      }
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
      LOG(ERROR) << "Parse module config failed. Module name : [" <<  mconf.name << "]";
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

}  // namespace cnstream
