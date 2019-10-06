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

/**
 * \file cnstream_pipeline.hpp
 *
 * This file contains a declaration of class Pipeline.
 */

#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "cnstream_eventbus.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

/**
 * Data stream message type.
 */
enum StreamMsgType {
  EOS_MSG = 0,     ///>  End of stream message, means the stream received eos in all modules.
  ERROR_MSG,       ///> Error message, means the stream process failed in one of the modules.
  USER_MSG0 = 32,  ///> Reserved message type, user can use it by themselves.
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

/**
 * Stream message.
 *
 * @see StreamMsgType.
 */
struct StreamMsg {
  StreamMsgType type;          ///> Message type
  int32_t chn_idx;             ///> Stream channel index, increament from 0.
  std::string stream_id = "";  ///> Stream id, set by user in CNDataFrame::stream_id
};                             // struct StreamMsg

/**
 * @brief Stream message observer.
 *
 * Stream message observer, used to receive stream message from pipeline.
 * User can inherit StreamMsgObserver and implement Update interface. The
 * observer instance can be bound to the pipeline through the
 * Pipeline::SetStreamMsgObserver to receive stream messages from pipeline.
 *
 * @see Pipeline::SetStreamMsgObserver StreamMsg StreamMsgType.
 */
class StreamMsgObserver {
 public:
  /**
   * This interface used to receive stream message from pipeline passively.
   *
   * @param msg Stream message from pipeline.
   */
  virtual void Update(const StreamMsg& msg) = 0;
  virtual ~StreamMsgObserver();
};  // class StreamMsgObserver

class PipelinePrivate;

/**
 * Link status between modules.
 */
struct LinkStatus {
  bool stopped;                      ///> Whether data transmission between modules stops.
  std::vector<uint32_t> cache_size;  ///> Number of data cache data in each data transmission queue between modules.
};

/**
 * @brief Module config parameters.
 *
 * CNModuleConfig can used to add module in pipeline.
 * Module config can write in JSON file.
 * eg.
 * @code
 * "name(CNModuleConfig::name)": {
 *   custom_params(CNModuleConfig::parameters): {
 *     "key0": "value",
 *     "key1": "value",
 *     ...
 *   }
 *  "parallelism(CNModuleConfig::parallelism)": 3,
 *  "max_input_queue_size(CNModuleConfig::maxInputQueueSize)": 20,
 *  "class_name(CNModuleConfig::className)": "Inferencer",
 *  "next_modules": ["module0(CNModuleConfig::name)", "module1(CNModuleConfig::name)", ...],
 * }
 * @endcode
 *
 * @see Pipeline::AddModuleConfig.
 */
struct CNModuleConfig {
  std::string name;           ///> Module name.
  ModuleParamSet parameters;  ///> key-value pairs, Pipeline passes this filed to module named CNModuleConfig::name.
  int parallelism;  ///> Module parallelism. it is equal to module thread number and the data queue for input data.
  int maxInputQueueSize;          ///> The max queue size for input data queues.
  std::string className;          ///> Module classs name.
  std::vector<std::string> next;  ///> Downstream modules(module name).

  /**
   * Parse members from json srting, except CNModuleConfig::name.
   *
   * @note If parse json file failed, std::string will be thrown.
   */
  void ParseByJSONStr(const std::string& jstr) noexcept(false);

  /**
   * Parse members from json file, except CNModuleConfig::name
   *
   * @note If parse json file failed, std::string will be thrown.
   */
  void ParseByJSONFile(const std::string& jfname) noexcept(false);
};

/**
 * Pipeline is the manager of modules.
 * Pipeline is used to manage data transmission between modules, and
 * controls message delivery.
 */
class Pipeline : public Module {
 public:
  /**
   * Constructor.
   *
   * @param name Name of pipeline.
   */
  explicit Pipeline(const std::string& name);
  ~Pipeline();

  /**
   * @see Module::Open.
   */
  bool Open(ModuleParamSet paramSet) override;
  /**
   * @see Module::Close.
   */
  void Close() override;
  /**
   * Useless.
   *
   * @see Module::Process.
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

  /**
   * Provide data for this pipeline. Used in source module or module transmit by itself.
   *
   * @param module Which module to provide data.
   * @param data The data transmit to pipeline.
   *
   * @return Return true for success. When module not added in pipeline, false will be returned.
   *         When pipeline is stopped, false will be returned.
   *
   * @see Module::Process.
   */
  bool ProvideData(const Module* module, std::shared_ptr<CNFrameInfo> data);

  /**
   * Get the event bus in pipeline.
   *
   * @return Return event bus.
   */
  EventBus* GetEventBus() const;
  /**
   * Start pipeline.
   * Start data transmission in pipeline.
   * Call all module's Open, see Module::Open.
   * Link modules.
   *
   * @return Return true for success. Return false when one of the module's Open return false.
   *         Return false When Link modules failed.
   */
  bool Start();
  /**
   * Stop data transmission in pipeline.
   *
   * @return Return true for success. Otherwise, false will be returned.
   */
  bool Stop();
  /**
   * Return running status for pipeline.
   *
   * @return true if pipeline is running. Return false if pipeline is not running.
   */
  inline bool IsRunning() const { return running_; }

 public:
  /**
   * Add module config in pipeline.
   *
   * @param config Module config.
   *
   * @return Return 0 for success. Otherwise, -1 will be returned.
   */
  int AddModuleConfig(const CNModuleConfig& config);
  /**
   * Build pipeline by module configs.
   *
   * @param configs Module configs.
   *
   * @return Return 0 for success. Otherwise, -1 will be returned.
   */
  int BuildPipeline(const std::vector<CNModuleConfig>& configs);
  /**
   * Build pipeline from a json file
   * JSON e.g.
   * @code
   * {
   *   "source" : {
   *                 "class_name" : "cnstream::DataSource",
   *                 "parallelism" : 0,
   *                 "next_modules" : ["detector"],
   *                 "custom_params" : {
   *                   "source_type" : "ffmpeg",
   *                   "decoder_type" : "mlu",
   *                   "device_id" : 0
   *                 }
   *              },
   *    "detector" : {...}
   * }
   * @endcode
   *
   * @param config_file JSON config file.
   *
   * @return Return 0 for success. Otherwise -1 will be returned. if parse json file failed, string will be thrown.
   */
  int BuildPipelineByJSONFile(const std::string& config_file) noexcept(false);
  /**
   * Get module in pipeline by name.
   *
   * @param moduleName Module name specified in Module's constructor.
   *
   * @return Return module pointer if module named moduleName has been added in pipeline, or nullptr will be returned.
   */
  Module* GetModule(const std::string& moduleName);
  /**
   * Get Link-indexs, link-index is the return value for Pipeline::LinkModules. It is used to query link status
   * between modules.
   *
   * @return Return all link-indexs in pipeline.
   *
   * @see Pipeline::LinkModules Pipeline::QueryLinkStatus.
   */
  std::vector<std::string> GetLinkIds();
  /**
   * Get module's parameter set.
   * Module parameter set is used in Module::Open. It provides the ability of modules to customize parameters.
   *
   * @param moduleName Module name specified in Module's constructor.
   *
   * @return Return module's customize parameters. If module has no customize parameters or module has not been
   *         added to this pipeline, then the return value's size(ModuleParamSet::size) is 0.
   *
   * @see Module::Open.
   */
  ModuleParamSet GetModuleParamSet(const std::string& moduleName);
  /**
   * Get module configuration by module name.
   *
   * @param module_name Module name specified in Module's constructor.
   *
   * @return Return module configurature for success. Return NULL if module specified by module_name has not been
   *         added in this pipeline.
   */
  CNModuleConfig GetModuleConfig(const std::string& module_name);

  /**
   * Add module to this pipeline.
   *
   * @param module Module instance to be added into this pipeline.
   *
   * @return Return true for success. Return false if module has been added into this pipeline.
   */
  bool AddModule(std::shared_ptr<Module> module);

  /**
   * Set the parallelism of the module.
   *
   * @param module The module to be config.
   * @param parallelism Module parallelism.
   *
   * @return Return true for success. Return false if this module has not been added into this pipeline.
   *
   * @note Call this function before call Pipeline::Start, or it will not be effective.
   *
   * @see CNModuleConfig::parallelism.
   */
  bool SetModuleParallelism(std::shared_ptr<Module> module, uint32_t parallelism);
  /**
   * Get module parallelism.
   *
   * @param module Module
   *
   * @return Return module parallelism for success. If module has not been added into this pipeline, 0 will be returned.
   */
  uint32_t GetModuleParallelism(std::shared_ptr<Module> module);

  /**
   * Link two modules.
   * Upstream node will process data before downstream node.
   *
   * @param up_node Upstream module.
   * @param down_node Downstream module.
   *
   * @return Return link-index for success, link-index can used to query link status between up_node and down_node,
   *         see Pipeline::QueryStatus for details. Return NULL if one of the two nodes has not been added into this
   * pipeline.
   *
   * @note up_node and down_node should be added into this pipeline before do this.
   *
   * @see Pipeline::QueryStatus.
   */
  std::string LinkModules(std::shared_ptr<Module> up_node, std::shared_ptr<Module> down_node,
                          size_t queue_capacity = 20);

 public:
  /**
   * Query link status by link-index.
   * link-index is returned by Pipeline::LinkModules.
   *
   * @param status The link status to be return.
   * @param link_id Link-index returned by Pipeline::LinkModules.
   *
   * @return Return true for query success, otherwise false will be returned.
   *
   * @see Pipeline::LinkModules.
   */
  bool QueryLinkStatus(LinkStatus* status, const std::string& link_id);

  /**
   * Print all module's performance informations.
   */
  void PrintPerformanceInformation() const;

  /* -----stream message methods------ */
 public:
  /**
   * Bind stream message observer with this pipeline to receive stream message from this pipeline.
   *
   * @param observer Stream message observer.
   *
   * @return void.
   *
   * @see StreamMsgObserver.
   */
  void SetStreamMsgObserver(StreamMsgObserver* observer) { smsg_observer_ = observer; }
  /**
   * Get stream message observer which has been bind with this pipeline.
   *
   * @return Return stream message observer which has been bind with this pipeline.
   *
   * @see Pipeline::SetStreamMsgObserver.
   */
  StreamMsgObserver* GetStreamMsgObserver() const { return smsg_observer_; }

  /* called by pipeline */
  void NotifyStreamMsg(const StreamMsg& smsg);

 private:
  StreamMsgObserver* smsg_observer_ = nullptr;  ///> Stream message observer.

  /* ------Internal methods------ */

 private:
  void TransmitData(int64_t node_hashcode, std::shared_ptr<CNFrameInfo> data);

  void TaskLoop(int64_t node_hashcode, uint32_t conveyor_idx);

  void EventLoop();

  EventHandleFlag DefaultBusWatch(const Event& event, Module* module);

  volatile bool running_ = false;
  EventBus* event_bus_;
  DECLARE_PRIVATE(d_ptr_, Pipeline);
};  // class Pipeline

}  // namespace cnstream

#endif  // CNSTREAM_PIPELINE_HPP_
