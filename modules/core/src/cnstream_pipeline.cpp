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

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>

#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"
#include "cnstream_timer.hpp"
#include "connector.hpp"
#include "conveyor.hpp"
#include "threadsafe_queue.hpp"

namespace cnstream {

struct ModuleAssociatedInfo {
  std::shared_ptr<Module> instance;
  uint32_t parallelism = 0;
  std::vector<CNTimer> timers_;
  std::set<int64_t> down_nodes;
  std::vector<int> input_connectors;
  std::vector<int> output_connectors;
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
  Pipeline* q_ptr_;
  std::vector<std::shared_ptr<Connector>> connectors_;
  std::unordered_map<std::string, std::shared_ptr<Connector>> links_;
  std::vector<std::thread> threads_;
  std::thread event_thread_;
  std::map<int64_t, ModuleAssociatedInfo> modules_;
  std::mutex stop_mtx_;
  uint64_t eos_mask_ = 0;

 private:
  std::unordered_map<std::string, CNModuleConfig> modules_config_;
  std::unordered_map<std::string, std::vector<std::string>> connections_config_;
  std::map<std::string, std::shared_ptr<Module>> modules_map_;
  DECLARE_PUBLIC(q_ptr_, Pipeline);
  void SetEOSMask() {
    for (const std::pair<int64_t, ModuleAssociatedInfo> module_info : modules_) {
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
      while (!exit_msg_loop_ && !msgq_.WaitAndTryPop(msg, std::chrono::microseconds(200)))
        ;
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
  d_ptr_ = new PipelinePrivate(this);

  event_bus_ = new EventBus();
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
  EventHandleFlag ret;
  switch (event.type) {
    case EventType::EVENT_ERROR:
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
  int64_t hashcode = reinterpret_cast<int64_t>(module);

  if (d_ptr_->modules_.find(hashcode) == d_ptr_->modules_.end()) return false;

  TransmitData(hashcode, data);

  return true;
}

bool Pipeline::AddModule(std::shared_ptr<Module> module) {
  int64_t hashcode = reinterpret_cast<int64_t>(module.get());

  if (d_ptr_->modules_.find(hashcode) != d_ptr_->modules_.end()) {
    LOG(WARNING) << "Module [" << module->GetName() << "] has already been added to this pipeline";
    return false;
  }

  LOG(INFO) << "Add Module " << module->GetName() << " to pipeline";
  if (module->GetId() == Module::INVALID_MODULE_ID) {
    LOG(ERROR) << "Failed to get module Id";
    return false;
  }

  ModuleAssociatedInfo associated_info;
  associated_info.instance = module;
  associated_info.parallelism = 1;
  module->SetContainer(this);
  d_ptr_->modules_.insert(std::make_pair(hashcode, associated_info));

  return true;
}

bool Pipeline::AddModule(std::vector<std::shared_ptr<Module>> modules) {
  bool ret = true;
  for (auto module : modules) {
    ret &= AddModule(module);
  }
  return ret;
}

bool Pipeline::SetModuleParallelism(std::shared_ptr<Module> module, uint32_t parallelism) {
  int64_t hashcode = reinterpret_cast<int64_t>(module.get());
  if (d_ptr_->modules_.find(hashcode) == d_ptr_->modules_.end()) return false;
  d_ptr_->modules_[hashcode].parallelism = parallelism;
  return true;
}

uint32_t Pipeline::GetModuleParallelism(std::shared_ptr<Module> module) {
  int64_t hashcode = reinterpret_cast<int64_t>(module.get());
  if (d_ptr_->modules_.find(hashcode) == d_ptr_->modules_.end()) return 0;
  return d_ptr_->modules_[hashcode].parallelism;
}

std::string Pipeline::LinkModules(std::shared_ptr<Module> up_node, std::shared_ptr<Module> down_node,
                                  size_t queue_capacity) {
  int64_t up_node_hashcode = reinterpret_cast<int64_t>(up_node.get());
  int64_t down_node_hashcode = reinterpret_cast<int64_t>(down_node.get());

  if (d_ptr_->modules_.find(up_node_hashcode) == d_ptr_->modules_.end() ||
      d_ptr_->modules_.find(down_node_hashcode) == d_ptr_->modules_.end()) {
    LOG(ERROR) << "module has not been added to this pipeline";
    return "";
  }

  ModuleAssociatedInfo& up_node_info = d_ptr_->modules_.find(up_node_hashcode)->second;
  ModuleAssociatedInfo& down_node_info = d_ptr_->modules_.find(down_node_hashcode)->second;

  auto ret = up_node_info.down_nodes.insert(down_node_hashcode);
  if (!ret.second) {
    LOG(ERROR) << "modules have been linked already";
    return "";
  }

  LOG(INFO) << "Link Module " << up_node->GetName() << "-->" << down_node->GetName();

  std::string link_id = up_node->GetName() + "-->" + down_node->GetName();
  // create connector
  std::shared_ptr<Connector> con = std::make_shared<Connector>(down_node_info.parallelism, queue_capacity);
  d_ptr_->connectors_.push_back(con);
  up_node_info.output_connectors.push_back(static_cast<int>(d_ptr_->connectors_.size() - 1));
  down_node_info.input_connectors.push_back(static_cast<int>(d_ptr_->connectors_.size() - 1));
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
  running_ = true;
  event_bus_->running_ = true;
  d_ptr_->event_thread_ = std::thread(&Pipeline::EventLoop, this);

  for (std::shared_ptr<Connector> connector : d_ptr_->connectors_) {
    connector->Start();
  }

  // create process threads
  for (auto& it : d_ptr_->modules_) {
    const int64_t node_hashcode = it.first;
    ModuleAssociatedInfo& module_info = it.second;
    uint32_t parallelism = module_info.parallelism;
    module_info.timers_.resize(parallelism);
    for (uint32_t conveyor_idx = 0; conveyor_idx < parallelism; ++conveyor_idx) {
      d_ptr_->threads_.push_back(std::thread(&Pipeline::TaskLoop, this, node_hashcode, conveyor_idx));
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
  for (std::shared_ptr<Connector>& connector : d_ptr_->connectors_) {
    connector->Stop();
  }
  running_ = false;
  event_bus_->running_ = false;
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
  printf("\nPipeline Performance information:\n");
  for (const auto& it : d_ptr_->modules_) {
    const ModuleAssociatedInfo& module_info = it.second;
    std::string module_name = module_info.instance->GetName();
    const std::vector<CNTimer>& fps_calculators = module_info.timers_;
    if (fps_calculators.size() == 0) continue;

    printf("****************************%s Performance******************************\n", module_name.c_str());
    for (size_t i = 0; i < fps_calculators.size(); ++i) {
      fps_calculators[i].PrintFps("thread " + std::to_string(i) + ": ");
    }
  }
}

void Pipeline::TransmitData(int64_t node_hashcode, std::shared_ptr<CNFrameInfo> data) {
  LOG_IF(FATAL, d_ptr_->modules_.find(node_hashcode) == d_ptr_->modules_.end());

  const ModuleAssociatedInfo& module_info = d_ptr_->modules_[node_hashcode];

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
  for (auto& down_node_hashcode : module_info.down_nodes) {
    ModuleAssociatedInfo& down_node_info = d_ptr_->modules_.find(down_node_hashcode)->second;
    data->frame.SetModuleMask(down_node_info.instance.get(), module_info.instance.get());
  }

  // broadcast
  const std::vector<int>& connector_idxs = module_info.output_connectors;
  for (auto& idx : connector_idxs) {
    std::shared_ptr<Connector>& connector = d_ptr_->connectors_[idx];
    int conveyor_idx = chn_idx % connector->GetConveyorCount();
    connector->PushDataBufferToConveyor(conveyor_idx, data);
  }
}

void Pipeline::TaskLoop(int64_t node_hashcode, uint32_t conveyor_idx) {
  LOG_IF(FATAL, d_ptr_->modules_.find(node_hashcode) == d_ptr_->modules_.end());

  ModuleAssociatedInfo& module_info = d_ptr_->modules_[node_hashcode];

  LOG_IF(FATAL, conveyor_idx >= module_info.timers_.size());
  CNTimer& timer = module_info.timers_[conveyor_idx];

  std::vector<std::shared_ptr<Connector>> input_connectors;
  std::transform(module_info.input_connectors.begin(), module_info.input_connectors.end(),
                 std::back_inserter(input_connectors),
                 [&](int idx) -> std::shared_ptr<Connector> { return d_ptr_->connectors_[idx]; });

  if (input_connectors.size() == 0) return;

  volatile bool has_data = true;
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
          TransmitData(node_hashcode, data);
          continue;
        }

        {
          auto start_time = std::chrono::high_resolution_clock::now();
          int ret = module_info.instance->Process(data);
          auto end_time = std::chrono::high_resolution_clock::now();
          std::chrono::duration<double, std::milli> diff = end_time - start_time;
          timer.Dot(diff.count(), 1);

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
        TransmitData(node_hashcode, data);
      }  // if
      else {
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

int Pipeline::BuildPipeine(const std::vector<CNModuleConfig> configs) {
  /*TODO,check configs*/
  ModuleCreatorWorker creator;
  std::map<std::string, int> queues_size;
  for (auto& v : configs) {
    this->AddModuleConfig(v);
    Module* module = creator.Create(v.className, v.name);
    std::shared_ptr<Module> instance(module);
    d_ptr_->modules_map_[v.name] = instance;
    queues_size[v.name] = v.maxInputQueueSize;
    this->AddModule(instance);
    this->SetModuleParallelism(instance, v.parallelism);
  }
  for (auto& v : d_ptr_->connections_config_) {
    for (auto& name : v.second) {
      this->LinkModules(d_ptr_->modules_map_[v.first], d_ptr_->modules_map_[name], queues_size[name]);
    }
  }
  return 0;
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
