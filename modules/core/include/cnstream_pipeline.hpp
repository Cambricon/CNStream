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
 * @file cnstream_pipeline.hpp
 *
 * This file contains a declaration of the Pipeline class.
 */

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "cnstream_eventbus.hpp"
#include "cnstream_module.hpp"
#include "cnstream_source.hpp"

namespace cnstream {

/**
 * Data stream message type.
 */
enum StreamMsgType {
  EOS_MSG = 0,     ///< The end of a stream message. The stream has received EOS message in all modules.
  ERROR_MSG,       ///< An error message. The stream process has failed in one of the modules.
  USER_MSG0 = 32,  ///< Reserved message. You can define your own messages.
  USER_MSG1,       ///< Reserved message. You can define your own messages.
  USER_MSG2,       ///< Reserved message. You can define your own messages.
  USER_MSG3,       ///< Reserved message. You can define your own messages.
  USER_MSG4,       ///< Reserved message. You can define your own messages.
  USER_MSG5,       ///< Reserved message. You can define your own messages.
  USER_MSG6,       ///< Reserved message. You can define your own messages.
  USER_MSG7,       ///< Reserved message. You can define your own messages.
  USER_MSG8,       ///< Reserved message. You can define your own messages.
  USER_MSG9        ///< Reserved message. You can define your own messages.
};  // enum StreamMsg

/**
 * Specifies a stream message.
 *
 * @see StreamMsgType.
 */
struct StreamMsg {
  StreamMsgType type;          ///< The type of a message.
  int32_t chn_idx;             ///< The index of a stream channel that incremented from 0.
  std::string stream_id = "";  ///< Stream ID. You can set in CNDataFrame::stream_id.
};                             // struct StreamMsg

/**
 * @brief Stream message observer.
 *
 * Receives stream messages from a pipeline.
 * To receive stream messages from the pipeline, you can define a class to inherit the
 * StreamMsgObserver class and call the ``Update`` function. The
 * observer instance are bounded to the pipeline using the
 * Pipeline::SetStreamMsgObserver function .
 *
 * @see Pipeline::SetStreamMsgObserver StreamMsg StreamMsgType.
 */
class StreamMsgObserver {
 public:
  /**
   * Receives stream messages from a pipeline passively.
   *
   * @param msg The stream message from a pipeline.
   */
  virtual void Update(const StreamMsg& msg) = 0;
  virtual ~StreamMsgObserver();
};  // class StreamMsgObserver

class PipelinePrivate;

/**
 * The link status between modules.
 */
struct LinkStatus {
  bool stopped;                      ///< Whether the data transmissions between the modules are stopped.
  std::vector<uint32_t> cache_size;  ///< The size of each queue that is used to cache data between modules.
};

/**
 * @brief The configuration parameters of a module.
 *
 * You can use ``CNModuleConfig`` to add modules in a pipeline.
 * The module configuration can be in JSON file.
 *
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
  std::string name;  ///< The name of the module.
  ModuleParamSet
      parameters;   ///< The key-value pairs. The pipeline passes this value to the CNModuleConfig::name module.
  int parallelism;  ///< Module parallelism. It is equal to module thread number and the data queue for input data.
  int maxInputQueueSize;          ///< The maximum size of the input data queues.
  std::string className;          ///< The class name of the module.
  std::vector<std::string> next;  ///< The name of the downstream modules.
  bool showPerfInfo;              ///< Whether to show performance information or not.

  /**
   * Parses members from JSON string except CNModuleConfig::name.
   *
   * @return Returns true if the JSON file has been parsed successfully. Otherwise, returns false.
   */
  bool ParseByJSONStr(const std::string& jstr);

  /**
   * Parses members from JSON file except CNModuleConfig::name.
   *
   * @return Returns true if the JSON file has been parsed successfully. Otherwise, returns false.
   */
  bool ParseByJSONFile(const std::string& jfname);
};

/**
 * The manager of the modules.
 * Manages data transmission between modules, and
 * controls message delivery.
 */
class Pipeline : public Module {
 public:
  /**
   * Constructor.
   *
   * @param name The name of the pipeline.
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
   * Provides data for this pipeline that is used in source module or the module
   * transmission by itself.
   *
   * @param module The module that provides data.
   * @param data The data that is transmitted to the pipeline.
   *
   * @return Returns true if this function has run successfully. Returns false if the module
   *         is not added in the pipeline or the pipeline has been stopped.
   *
   * @see Module::Process.
   */
  bool ProvideData(const Module* module, std::shared_ptr<CNFrameInfo> data);

  /**
   * Gets the event bus in the pipeline.
   *
   * @return Returns the event bus.
   */
  EventBus* GetEventBus() const;
  /**
   * Starts a pipeline.
   * Starts data transmission in a pipeline.
   * Calls the ``Open`` function for all modules. See Module::Open.
   * Links modules.
   *
   * @return Returns true if this function has run successfully. Returns false if the ``Open``
   *         function did not run successfully in one of the modules, or
   *         the link modules failed.
   */
  bool Start();
  /**
   * Stops data transmissions in a pipeline.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   */
  bool Stop();
  /**
   * Returns the running status for a pipeline.
   *
   * @return Returns true if the pipeline is running. Returns false if the pipeline is
   *         not running.
   */
  inline bool IsRunning() const { return running_; }

 public:
  /**
   * Adds module configurations in a pipeline.
   *
   * @param The configuration of a module.
   *
   * @return Returns 0 if this function has run successfully. Otherwise, returns -1.
   */
  int AddModuleConfig(const CNModuleConfig& config);
  /**
   * Builds a pipeline by module configurations.
   *
   * @param configs The configurations of a module.
   *
   * @return Returns 0 if this function has run successfully. Otherwise, returns -1.
   */
  int BuildPipeline(const std::vector<CNModuleConfig>& configs);
  /**
   * Builds a pipeline from a JSON file.
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
   * @param config_file The configuration file in JSON format.
   *
   * @return Returns 0 if this function has run successfully. Otherwise, returns -1.
   *
   */
  int BuildPipelineByJSONFile(const std::string& config_file);
  /**
   * Gets a module in a pipeline by name.
   *
   * @param moduleName The module name specified in the module constructor.
   *
   * @return Returns the module pointer if the module named ``moduleName`` has been added to
   *         the pipeline. Otherwise, returns nullptr.
   */
  Module* GetModule(const std::string& moduleName);
  /**
   * Gets link-indexs that is used to query link status between modules.
   * The link-index is the return value of Pipeline::LinkModules.
   *
   * @return Returns all link-indexs in a pipeline.
   *
   * @see Pipeline::LinkModules and Pipeline::QueryLinkStatus.
   */
  std::vector<std::string> GetLinkIds();
  /**
   * Gets parameter set of a module.
   * Module parameter set is used in Module::Open. It provides the ability for modules to
   * customize parameters.
   *
   * @param moduleName The module name specified in the module constructor.
   *
   * @return Returns the customized parameters of the module. If the module does not
   *         have customized parameters or the module has not been
   *         added to this pipeline, then the value of size (ModuleParamSet::size) is 0.
   *
   * @see Module::Open.
   */
  ModuleParamSet GetModuleParamSet(const std::string& moduleName);
  /**
   * Gets the module configuration by the module name.
   *
   * @param module_name The module name specified in module constructor.
   *
   * @return Returns module configuration if this function has run successfully.
   *         Returns NULL if the module specified by ``module_name`` has not been
   *         added to this pipeline.
   */
  CNModuleConfig GetModuleConfig(const std::string& module_name);

  /**
   * Adds the module to a pipeline.
   *
   * @param module The module instance to be added to this pipeline.
   *
   * @return Returns true if this function has run successfully. Returns false if
   *         the module has been added to this pipeline.
   */
  bool AddModule(std::shared_ptr<Module> module);

  /**
   * Sets the parallelism and conveyor capacity attributes of the module.
   *
   * The SetModuleParallelism function is deprecated. Please use the SetModuleAttribute function instead.
   *
   * @param module The module to be configured.
   * @param parallelism Module parallelism, as well as Module's conveyor number of input connector.
   * @param queue_capacity The queue capacity of the Module input conveyor.
   *
   * @return Returns true if this function has run successfully. Returns false if this module
   *         has not been added to this pipeline.
   *
   * @note You must call this function before calling Pipeline::Start.
   *
   * @see CNModuleConfig::parallelism.
   */
  bool SetModuleAttribute(std::shared_ptr<Module> module, uint32_t parallelism, size_t queue_capacity = 20);
  /**
   * Gets the module parallelism.
   *
   * @param module The module you want to query.
   *
   * @return Returns the module parallelism if this function has run successfully.
   *         Returns 0 if the module has not been added to this pipeline.
   */
  uint32_t GetModuleParallelism(std::shared_ptr<Module> module);

  /**
   * Links two modules.
   * The upstream node will process data before the downstream node.
   *
   * @param up_node The upstream module.
   * @param down_node The downstream module.
   *
   * @return Returns the link-index if this function has run successfully. The link-index can
   *         used to query link status between ``up_node`` and ``down_node``.
   *         See Pipeline::QueryStatus for details. Returns NULL if one of the two nodes
   *         has not been added to this pipeline.
   *
   * @note Both ``up_node`` and ``down_node`` should be added to this pipeline before calling
   *       this function.
   *
   * @see Pipeline::QueryStatus.
   */
  std::string LinkModules(std::shared_ptr<Module> up_node, std::shared_ptr<Module> down_node);

 public:
  /**
   * Queries the link status by link-index.
   * link-index is returned by Pipeline::LinkModules.
   *
   * @param status The link status to query.
   * @param link_id The Link-index returned by Pipeline::LinkModules.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   *
   * @see Pipeline::LinkModules.
   */
  bool QueryLinkStatus(LinkStatus* status, const std::string& link_id);

  /**
   * Prints the performance information for all modules.
   */
  void PrintPerformanceInformation() const;

  /* -----stream message methods------ */
 public:
  /**
   * Binds the stream message observer with this pipeline to receive stream message from
   * this pipeline.
   *
   * @param observer The stream message observer.
   *
   * @return Void.
   *
   * @see StreamMsgObserver.
   */
  void SetStreamMsgObserver(StreamMsgObserver* observer) { smsg_observer_ = observer; }
  /**
   * Gets the stream message observer that has been bound with this pipeline.
   *
   * @return Returns the stream message observer that has been bound with this pipeline.
   *
   * @see Pipeline::SetStreamMsgObserver.
   */
  StreamMsgObserver* GetStreamMsgObserver() const { return smsg_observer_; }

  /* called by pipeline */
  /**
   * Passes the stream message to the observer of this pipeline.
   *
   * @param smsg The stream message.
   *
   * @return Void.
   *
   * @see StreamMsg.
   */
  void NotifyStreamMsg(const StreamMsg& smsg);

 private:
  StreamMsgObserver* smsg_observer_ = nullptr;  ///< Stream message observer.

  /* ------Internal methods------ */

 private:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  void TransmitData(const std::string node_name, std::shared_ptr<CNFrameInfo> data);

  void TaskLoop(std::string node_name, uint32_t conveyor_idx);

  void EventLoop();

  EventHandleFlag DefaultBusWatch(const Event& event, Module* module);

  std::atomic<bool> running_{false};
  EventBus* event_bus_;
  DECLARE_PRIVATE(d_ptr_, Pipeline);
};  // class Pipeline

}  // namespace cnstream

#endif  // CNSTREAM_PIPELINE_HPP_
