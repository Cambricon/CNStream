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
#include "util/cnstream_rwlock.hpp"
#include "profiler/pipeline_profiler.hpp"

namespace cnstream {

class Connector;

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
  std::mutex id_lock;
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
   * @param module_configs The configurations of a module.
   * 
   * @param profiler_config The configuration of profiler.
   *
   * @return Returns 0 if this function has run successfully. Otherwise, returns -1.
   */
  int BuildPipeline(const std::vector<CNModuleConfig>& module_configs,
                    const ProfilerConfig& profiler_config = ProfilerConfig());
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

  /**
   * Is profiling enabled.
   * 
   * @return Returns true if profiling is enabled.
   **/
  bool IsProfilingEnabled() const;

  /**
   * Is tracing enabled
   * 
   * @return Returns true if tracing is enabled.
   **/
  bool IsTracingEnabled() const;

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

  /** profiler **/
  PipelineProfiler* GetProfiler() const;

  /** tracer **/
  PipelineTracer* GetTracer() const;

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

  /**
   * Return if module is root node of pipeline.
   * 
   * @param node_name module name.
   *
   * @return True for yes, false for no.
   **/
  bool IsRootNode(const std::string& node_name) const;

  /**
   * Return if module is leaf node of pipeline.
   * 
   * @param node_name module name.
   *
   * @return True for yes, false for no.
   **/
  bool IsLeafNode(const std::string& node_name) const;

 private:
  /** called by BuildPipeline **/
  void GenerateRouteMask();
  std::vector<std::string> GetModuleNames();

 private:
  /* ------Internal methods------ */
  void UpdateByStreamMsg(const StreamMsg& msg);
  void StreamMsgHandleFunc();
  bool ShouldTransmit(std::shared_ptr<CNFrameInfo> finfo, Module* module) const;
  bool ShouldTransmit(uint64_t passed_modules_mask, Module* module) const;
  bool PassedByAllModules(std::shared_ptr<CNFrameInfo> finfo) const;
  bool PassedByAllModules(uint64_t passed_modules_mask) const;

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
  /** first: root node name, second: mask used in Transmit **/
  std::unordered_map<std::string, uint64_t> route_masks_;
  uint64_t all_modules_mask_ = 0;

  std::vector<std::string> stream_ids_;

  ProfilerConfig profiler_config_;
  std::unique_ptr<PipelineProfiler> profiler_;
};  // class Pipeline

inline bool Pipeline::IsProfilingEnabled() const {
  return profiler_config_.enable_profiling;
}

inline bool Pipeline::IsTracingEnabled() const {
  return profiler_config_.enable_tracing;
}

inline PipelineProfiler* Pipeline::GetProfiler() const {
  return profiler_.get();
}

inline PipelineTracer* Pipeline::GetTracer() const {
  return IsTracingEnabled() ? GetProfiler()->GetTracer() : nullptr;
}

inline bool Pipeline::ShouldTransmit(std::shared_ptr<CNFrameInfo> finfo, Module* module) const {
  return ShouldTransmit(finfo->GetModulesMask(), module);
}

inline bool Pipeline::ShouldTransmit(uint64_t passed_modules_mask, Module* module) const {
  uint64_t modules_mask = module->GetModulesMask();
  return (passed_modules_mask & modules_mask) == modules_mask;
}

inline bool Pipeline::PassedByAllModules(std::shared_ptr<CNFrameInfo> finfo) const {
  return PassedByAllModules(finfo->GetModulesMask());
}

inline bool Pipeline::PassedByAllModules(uint64_t passed_modules_mask) const {
  return passed_modules_mask == all_modules_mask_;
}

inline bool Pipeline::IsRootNode(const std::string& node_name) const {
  return !modules_.find(node_name)->second.input_connectors.size();
}

inline bool Pipeline::IsLeafNode(const std::string& node_name) const {
  return !modules_.find(node_name)->second.down_nodes.size();
}

}  // namespace cnstream

#endif  // CNSTREAM_PIPELINE_HPP_
