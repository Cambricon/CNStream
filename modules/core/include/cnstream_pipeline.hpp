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

#ifndef CNSTREAM_PIPELINE_HPP_
#define CNSTREAM_PIPELINE_HPP_

#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "cnstream_eventbus.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

enum StreamMsgType {
  /*
    when all modules in the pipeline receive the eos message,
    the observer set to the pipeline will receive eos message
   */
  EOS_MSG = 0,
  /*
    when the process function of a module returns a non-0 value,
    the observer set to the pipeline will receive error message.
   */
  ERROR_MSG,
  USER_MSG0 = 32,
  USER_MSG1,
  USER_MSG2,
  USER_MSG3,
  USER_MSG4,
  USER_MSG5,
  USER_MSG6,
  USER_MSG7,
  USER_MSG8,
  USER_MSG9
};  // enum StreamMsg

struct StreamMsg {
  StreamMsgType type;          // message type
  int32_t chn_idx;             // video channel inde
  std::string stream_id = "";  // stream id, set by user in CNDataFrame::stream_id
};                             // struct StreamMsg

class StreamMsgObserver {
 public:
  /*
    this function will be called by the pipeline if there is a message to the pipeline.
    see Pipeline::SetStreamMsgObserver
   */
  virtual void Update(const StreamMsg& msg) = 0;
  virtual ~StreamMsgObserver();
};  // class StreamMsgObserver

class PipelinePrivate;

struct LinkStatus {
  bool stopped;
  std::vector<uint32_t> cache_size;
};

struct CNModuleConfig {
  std::string name;
  ModuleParamSet parameters; /*key-value pairs*/
  int parallelism;           /*thread number*/
  int maxInputQueueSize;
  /*
   * the below params are for auto-build-pipeline
   */
  std::string className;         /*Module Class Name*/
  std::vector<std::string> next; /*downstream module names*/
};

class Pipeline : public Module {
 public:
  explicit Pipeline(const std::string& name);
  ~Pipeline();

  bool Open(ModuleParamSet paramSet) override;
  void Close() override;
  int Process(std::shared_ptr<CNFrameInfo> data) override;

  /************************************************************************
   * @brief provide data for this pipeline. used in source module such as decoder
   ************************************************************************/
  bool ProvideData(const Module* module, std::shared_ptr<CNFrameInfo> data);

  EventBus* GetEventBus() const;
  bool Start();
  bool Stop();
  inline bool IsRunning() { return running_; }

 public:
  int AddModuleConfig(const CNModuleConfig& config);
  /* ------auto-graph methods------ */
  int BuildPipeine(const std::vector<CNModuleConfig> configs);
  Module* GetModule(const std::string& moduleName);
  std::vector<std::string> GetLinkIds();
  ModuleParamSet GetModuleParamSet(const std::string& moduleName);
  CNModuleConfig GetModuleConfig(const std::string& module_name);

  /************************************************************************
   * @brief modules should be added to pipeline.
   * @param
   *   module[in]: which module to be add
   ************************************************************************/
  bool AddModule(std::shared_ptr<Module> module);
  bool AddModule(std::vector<std::shared_ptr<Module>> modules);

  /**********************************************************************************
   * @brief set module max parallelism (max processing thread number at a time)
   * @param
   *   module[in]: which module to be set
   *   parallelism[in]: parallelism to set
   * @return true if module has been added to this pipeline, false otherwise
   **********************************************************************************/
  bool SetModuleParallelism(std::shared_ptr<Module> module, uint32_t parallelism);
  uint32_t GetModuleParallelism(std::shared_ptr<Module> module);

  /**********************************************************************************
   * @brief link two modules
   * @attention: up_node and down_node should be added first. see Pipeline::AddModule
   * @param
   *   up_node[in]: upstream module (src)
   *   down_node[in]: downstream module (sink)
   * @return unique link id, used to query cache status, return empty string if link failed
   **********************************************************************************/
  std::string LinkModules(std::shared_ptr<Module> up_node, std::shared_ptr<Module> down_node,
                          size_t queue_capacity = 20);

 public:
  /**********************************************************************************
   * @brief query link status by unique link id
   * @param
   *   link_id[in]: unique link id
   *   status[out]: status of specified link
   * @return whether query succeeded
   **********************************************************************************/
  bool QueryLinkStatus(LinkStatus* status, const std::string& link_id);
  /*
    @brief print all modules performance information
    @attention do this after pipeline stops
   */
  void PrintPerformanceInformation() const;

  /* -----stream message methods------ */
 public:
  void SetStreamMsgObserver(StreamMsgObserver* observer) { smsg_observer_ = observer; }
  StreamMsgObserver* GetStreamMsgObserver() const { return smsg_observer_; }
  /*
    @brief notify observer
    @param
      smsg[in]: stream message.
   */
  void NotifyStreamMsg(const StreamMsg& smsg);

 private:
  StreamMsgObserver* smsg_observer_ = nullptr;

  /* ------Internal methods------ */

 private:
  void TransmitData(int64_t node_hashcode, std::shared_ptr<CNFrameInfo> data);

  void TaskLoop(int64_t node_hashcode, uint32_t conveyor_idx);

  void EventLoop();

  EventHandleFlag DefaultBusWatch(const Event& event, Module* module);

  PipelinePrivate* d_ptr_;
  volatile bool running_ = false;
  EventBus* event_bus_;
  DECLARE_PRIVATE(d_ptr_, Pipeline);
};  // class Pipeline

}  // namespace cnstream

#endif  // CNSTREAM_PIPELINE_HPP_
