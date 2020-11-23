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
#include <bitset>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cnstream_common.hpp"
#include "cnstream_config.hpp"
#include "cnstream_eventbus.hpp"
#include "cnstream_module.hpp"
#include "cnstream_source.hpp"
#include "perf_calculator.hpp"
#include "util/cnstream_rwlock.hpp"

namespace cnstream {

class Connector;
class PerfCalculator;

/**
 * Data stream message type.
 */
enum StreamMsgType {
  EOS_MSG = 0,     ///< The end of a stream message. The stream has received EOS message in all modules.
  ERROR_MSG,       ///< An error message. The stream process has failed in one of the modules.
  STREAM_ERR_MSG,  ///< Stream error message, stream process failed at source.
  FRAME_ERR_MSG,   ///< Frame error message, frame decode failed at source.
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
};                 // enum StreamMsg

/**
 * Specifies a stream message.
 *
 * @see StreamMsgType.
 */
struct StreamMsg {
  StreamMsgType type;       ///< The type of a message.
  std::string stream_id;    ///< Stream id, set by user in CNFrameINfo::stream_id.
  std::string module_name;  ///< The module that posts this event.
  int64_t pts = -1;  ///< The pts of this frame.
};

/**
 * @brief Stream message observer.
 *
 * Receives stream messages from a pipeline.
 * To receive stream messages from the pipeline, you can define a class to inherit the
 * StreamMsgObserver class and call the ``Update`` function. The
 * observer instance is bounded to the pipeline using the
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
  virtual ~StreamMsgObserver() = default;
};  // class StreamMsgObserver

/**
 * The link status between modules.
 */
struct LinkStatus {
  bool stopped;                      ///< Whether the data transmissions between the modules are stopped.
  std::vector<uint32_t> cache_size;  ///< The size of each queue that is used to cache data between modules.
};

static constexpr size_t MAX_STREAM_NUM = 64;

/**
 * @brief ModuleId&StreamIdx manager for pipeline.
 *
 * Allocates and deallocates id for Pipeline modules & Streams.
 *
 */
class IdxManager {
 public:
  IdxManager() = default;
  IdxManager(const IdxManager&) = delete;
  IdxManager& operator=(const IdxManager&) = delete;
  uint32_t GetStreamIndex(const std::string& stream_id);
  void ReturnStreamIndex(const std::string& stream_id);
  size_t GetModuleIdx();
  void ReturnModuleIdx(size_t id_);

 private:
  SpinLock id_lock;
  std::unordered_map<std::string, uint32_t> stream_idx_map;
  std::bitset<MAX_STREAM_NUM> stream_bitset;
  uint64_t module_id_mask_ = 0;
};  // class IdxManager

/**
 * The manager of the modules.
 * Manages data transmission between modules, and
 * controls messages delivery.
 */
class Pipeline : private NonCopyable {
 public:
  /**
   * Constructor.
   *
   * @param name The name of the pipeline.
   */
  explicit Pipeline(const std::string& name);
  ~Pipeline();
  const std::string& GetName() const { return name_; }
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
  EventBus* GetEventBus() const { return event_bus_; }
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
   * The running status of a pipeline.
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
   * @brief Gets end module in pipeline(only valid when pipeline graph converged at end module).
   *
   * @return Returns endmodule pointer when endmodule found and pipeline graph is converged at it,
   *   otherwise return nullptr.
   */
  Module* GetEndModule();
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

 public:
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
  void SetStreamMsgObserver(StreamMsgObserver* observer);
  /**
   * Gets the stream message observer that has been bound with this pipeline.
   *
   * @return Returns the stream message observer that has been bound with this pipeline.
   *
   * @see Pipeline::SetStreamMsgObserver.
   */
  StreamMsgObserver* GetStreamMsgObserver() const;

  /**
   * @brief Creates PerfManager to measure performance of modules and pipeline for each stream.
   *
   * This function creates database for each stream.
   * One thread is for committing sqlite events to increase the speed of inserting data to the database.
   * Another is for calculating performance of modules and pipeline, and printing performance statistics afterward.
   *
   * @param stream_ids The stream IDs.
   * @param db_dir The directory where database files to be saved.
   * @param clear_data_interval The interval of clearing data in database. The default value is 10 minutes.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   */
  bool CreatePerfManager(std::vector<std::string> stream_ids, std::string db_dir,
                         uint32_t clear_data_interval = 10/*in minutes*/);
  /**
   * @brief Removes PerfManager of the stream.
   *
   * @note Calls this function after calling ``RemoveSource`` and receiving eos of this stream.
   *
   * @param stream_id The stream ID.
   *
   * @return Returns true if PerfManager of the stream has been removed successfully. Otherwise, returns false.
   */
  bool RemovePerfManager(std::string stream_id);
  /**
   * @brief Adds PerfManager of the stream.
   *
   * @note Calls this function after calling ``CreatePerfManager``.
   * @note Calls this function before calling ``AddSource``, which will add the stream to source.
   *
   * @param stream_id The stream ID.
   * @param db_dir The directory where database files to be saved.
   *
   * @return Returns true if PerfManager of the stream has been added successfully. Otherwise, returns false.
   */
  bool AddPerfManager(std::string stream_id, std::string db_dir);
  /**
   * @brief Commits sqlite events to increase the speed of inserting data to the database.
   *
   * This is a thread function. The events are committed every second.
   *
   * @return Void.
   */
  void PerfSqlCommitLoop();

  /**
   * @brief Calculates performance of modules and pipeline, and prints performance statistics every two seconds.
   *
   * This is a thread function.
   *
   * @return Void.
   */
  void CalculatePerfStats();

  /**
   * @brief Calculates the performance of modules and prints the performance statistics.
   *
   * This is called by thread function CalculatePerfStats.
   *
   * @return Void.
   */
  void CalculateModulePerfStats(bool final_print = false);
  /**
   * @brief Calculates the performance of pipeline and prints the performance statistics.
   *
   * This is called by thread function CalculatePerfStats.
   *
   * @return Void.
   */
  void CalculatePipelinePerfStats(bool final_print = false);

  /**
   * @brief Get perf managers from pipeline.
   *
   * @return std::unordered_map<std::string, std::shared_ptr<PerfManager>>
   */
  std::unordered_map<std::string, std::shared_ptr<PerfManager>> GetPerfManagers();
  /* called by pipeline */
  /**
   * Registers a callback to be called after the frame process is done.
   *
   * @return Void.
   *
   */
  inline void RegistIPCFrameDoneCallBack(std::function<void(std::shared_ptr<CNFrameInfo>)> callback) {
    frame_done_callback_ = std::move(callback);
  }

 private:
  std::vector<std::string> GetModuleNames();
  void SetStartAndEndNodeNames();
  bool CreatePerfCalculator(std::string db_dir, std::string node_name, bool is_pipeline);
  PerfStats CalcLatestThroughput(std::string sql_name, std::string perf_type, std::vector<std::string> keys,
                                 std::shared_ptr<PerfCalculator> calculator, bool final_print);

 private:
  /* ------Internal methods------ */
  void UpdateByStreamMsg(const StreamMsg& msg);
  void StreamMsgHandleFunc();
  bool ShouldTransmit(std::shared_ptr<CNFrameInfo> finfo, Module* module) const;
  bool ShouldTransmit(uint64_t passed_modules_mask, Module* module) const;

 private:
#ifdef UNIT_TEST

 public:
#endif
  void TransmitData(const std::string node_name, std::shared_ptr<CNFrameInfo> data);

  void TaskLoop(std::string node_name, uint32_t conveyor_idx);

  void EventLoop();

  EventHandleFlag DefaultBusWatch(const Event& event);

  /**
   * StreamIdx helpers for SourceModule instances.
   * ModuleIdx helpers for Module instances
   */
  friend class Module;
  friend class SourceModule;

  uint32_t GetStreamIndex(const std::string& stream_id) {
    if (idxManager_) {
      return idxManager_->GetStreamIndex(stream_id);
    }
    return INVALID_STREAM_IDX;
  }

  void ReturnStreamIndex(const std::string& stream_id) {
    if (idxManager_) {
      idxManager_->ReturnStreamIndex(stream_id);
    }
  }

  size_t GetModuleIdx() {
    if (idxManager_) {
      return idxManager_->GetModuleIdx();
    }
    return INVALID_MODULE_ID;
  }

  void ReturnModuleIdx(size_t idx) {
    if (idxManager_) {
      idxManager_->ReturnModuleIdx(idx);
    }
  }

  void PerfDeleteDataLoop();
  /**
   * The module associated information.
   */
  struct ModuleAssociatedInfo {
    uint32_t parallelism = 0;
    std::shared_ptr<Connector> connector;
    std::set<std::string> down_nodes;
    std::vector<std::string> input_connectors;
    std::vector<std::string> output_connectors;
  };

  std::string name_;
  std::atomic<bool> running_{false};
  EventBus* event_bus_ = nullptr;
  IdxManager* idxManager_ = nullptr;
  std::function<void(std::shared_ptr<CNFrameInfo>)> frame_done_callback_ = nullptr;

  ThreadSafeQueue<StreamMsg> msgq_;
  std::thread smsg_thread_;
  StreamMsgObserver* smsg_observer_ = nullptr;
  std::atomic<bool> exit_msg_loop_{false};

  std::vector<std::thread> threads_;
  std::unordered_map<std::string, std::shared_ptr<Module>> modules_map_;
  std::unordered_map<std::string, std::shared_ptr<Connector>> links_;
  std::unordered_map<std::string, ModuleAssociatedInfo> modules_;
  std::unordered_map<std::string, CNModuleConfig> modules_config_;
  std::unordered_map<std::string, std::vector<std::string>> connections_config_;

  std::vector<std::string> stream_ids_;
  std::string start_node_;
  std::vector<std::string> end_nodes_;
  std::unordered_map<std::string, std::shared_ptr<PerfManager>> perf_managers_;
  std::unordered_map<std::string, std::shared_ptr<PerfCalculator>> perf_calculators_;
  std::thread perf_commit_thread_;
  std::thread perf_del_data_thread_;
  std::thread calculate_perf_thread_;
  std::atomic<bool> perf_running_{false};
  uint32_t clear_data_interval_ = 10;
  RwLock perf_managers_lock_;
  std::mutex perf_calculation_lock_;
  uint64_t all_modules_mask_ = 0;
};  // class Pipeline

inline bool Pipeline::ShouldTransmit(std::shared_ptr<CNFrameInfo> finfo, Module* module) const {
  uint64_t passed_modules_mask = finfo->GetModulesMask();   // identifies which modules have passed this frame
  return ShouldTransmit(passed_modules_mask, module);
}

inline bool Pipeline::ShouldTransmit(uint64_t passed_modules_mask, Module* module) const {
  uint64_t modules_mask = module->GetModulesMask();  // identifies upstream nodes
  return (passed_modules_mask & modules_mask) == modules_mask;
}

}  // namespace cnstream

#endif  // CNSTREAM_PIPELINE_HPP_
