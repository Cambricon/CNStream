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
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <map>
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
struct NodeContext;
template<typename T>
class CNGraph;
class IdxManager;

/**
 * @enum StreamMsgType
 *
 * @brief Enumeration variables describing the data stream message type.
 */
enum class StreamMsgType {
  EOS_MSG = 0,    /*!< The end of a stream message. The stream has received EOS message in all modules. */
  ERROR_MSG,      /*!< An error message. The stream process has failed in one of the modules. */
  STREAM_ERR_MSG, /*!< Stream error message. */
  FRAME_ERR_MSG,  /*!< Frame error message. */
  USER_MSG0 = 32, /*!< Reserved message. You can define your own messages. */
  USER_MSG1,      /*!< Reserved message. You can define your own messages. */
  USER_MSG2,      /*!< Reserved message. You can define your own messages. */
  USER_MSG3,      /*!< Reserved message. You can define your own messages. */
  USER_MSG4,      /*!< Reserved message. You can define your own messages. */
  USER_MSG5,      /*!< Reserved message. You can define your own messages. */
  USER_MSG6,      /*!< Reserved message. You can define your own messages. */
  USER_MSG7,      /*!< Reserved message. You can define your own messages. */
  USER_MSG8,      /*!< Reserved message. You can define your own messages. */
  USER_MSG9       /*!< Reserved message. You can define your own messages. */
};                // enum StreamMsg

/**
 * @struct StreamMsg
 *
 * @brief The StreamMsg is a structure holding the information of a stream message.
 *
 * @see StreamMsgType.
 */
struct StreamMsg {
  StreamMsgType type;      /*!< The type of a message. */
  std::string stream_id;   /*!< Stream ID, set in CNFrameInfo::stream_id. */
  std::string module_name; /*!< The module that posts this event. */
  int64_t pts = -1;        /*!< The PTS (Presentation Timestamp) of this frame. */
};

/**
 * @class StreamMsgObserver
 *
 * @brief Receives stream messages from a pipeline.
 * To receive stream messages from the pipeline, you can define a class to inherit the
 * StreamMsgObserver class and call the ``Update`` function. The
 * observer instance is bounded to the pipeline using the Pipeline::SetStreamMsgObserver function .
 *
 * @see Pipeline::SetStreamMsgObserver StreamMsg StreamMsgType.
 */
class StreamMsgObserver {
 public:
  /**
   * @brief Receives stream messages from a pipeline passively.
   *
   * @param[in] msg The stream message from a pipeline.
   *
   * @return No return value.
   */
  virtual void Update(const StreamMsg& msg) = 0;

  /**
   * @brief Default destructor to destruct stream message observer.
   *
   * @return No return value.
   */
  virtual ~StreamMsgObserver() = default;
};  // class StreamMsgObserver

/**
 * @class Pipeline
 *
 * @brief Pipeline is the manager of the modules, which manages data transmission between modules and controls messages delivery.
 */
class Pipeline : private NonCopyable {
 public:
  /**
   * @brief A constructor to construct one pipeline.
   *
   * @param[in] name The name of the pipeline.
   *
   * @return No return value.
   */
  explicit Pipeline(const std::string& name);
  /**
   * @brief A destructor to destruct one pipeline.
   *
   * @param[in] name The name of the pipeline.
   *
   * @return No return value.
   */
  virtual ~Pipeline();
  /**
   * @brief Gets the pipeline's name.
   *
   * @return Returns the pipeline's name.
   */
  const std::string& GetName() const;
  /**
   * @brief Builds a pipeline by module configurations.
   *
   * @param[in] module_configs The configurations of a module.
   * @param[in] profiler_config The configuration of a profiler.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   */
  bool BuildPipeline(const std::vector<CNModuleConfig>& module_configs,
                     const ProfilerConfig& profiler_config = ProfilerConfig());
  /**
   * @brief Builds a pipeline by graph configuration.
   *
   * @param[in] graph_config The configuration of a graph.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   */
  bool BuildPipeline(const CNGraphConfig& graph_config);
  /**
   * @brief Builds a pipeline from a JSON file.
   * You can learn to write a configuration file by looking at the description of CNGraphConfig.
   *
   * @see CNGraphConfig
   *
   * @param[in] config_file The configuration file in JSON format.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   *
   */
  bool BuildPipelineByJSONFile(const std::string& config_file);
  /**
   * @brief Starts a pipeline.
   * Starts data transmission in a pipeline.
   * Calls the ``Open`` function for all modules. See Module::Open.
   *
   * @return Returns true if this function has run successfully. Returns false if the ``Open``
   *         function did not run successfully in one of the modules, or the link modules failed.
   */
  bool Start();
  /**
   * @brief Stops data transmissions in a pipeline.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   */
  bool Stop();
  /**
   * @brief The running status of a pipeline.
   *
   * @return Returns true if the pipeline is running. Returns false if the pipeline is not running.
   */
  bool IsRunning() const;
  /**
   * @brief Gets a module in current pipeline by name.
   *
   * @param[in] module_name The module name specified in the module configuration.
   * If you specify a module name written in the module configuration, the first module with the same name as
   * the specified module name in the order of DFS will be returned.
   * When there are modules with the same name as other graphs in the subgraph, you can also find the
   * module by adding the graph name prefix divided by slash. eg. pipeline_name/subgraph1/module1.
   *
   * @return Returns the module pointer if the module has been added to
   *         the current pipeline. Otherwise, returns nullptr.
   */
  Module* GetModule(const std::string& module_name) const;
  /**
   * @brief Gets the module configuration by the module name.
   *
   * @param[in] module_name The module name specified in module configuration.
   * The module name can be specified by two ways, see Pipeline::GetModule for detail.
   *
   * @return Returns module configuration if this function has run successfully.
   *         Returns NULL if the module specified by ``module_name`` has not been
   *         added to the current pipeline.
   */
  CNModuleConfig GetModuleConfig(const std::string& module_name) const;
  /**
   * @brief Checks if profiling is enabled.
   *
   * @return Returns true if profiling is enabled.
   **/
  bool IsProfilingEnabled() const;
  /**
   * @brief Checks if tracing is enabled.
   *
   * @return Returns true if tracing is enabled.
   **/
  bool IsTracingEnabled() const;
  /**
   * @brief Provides data for the pipeline that is used in source module or the module transmitted by itself.
   *
   * @param[in] module The module that provides data.
   * @param[in] data The data that is transmitted to the pipeline.
   *
   * @return Returns true if this function has run successfully. Returns false if the module
   *         is not added in the pipeline or the pipeline has been stopped.
   *
   * @note ProvideData can be only called by the head modules in pipeline. A head module means the module
   * has no parent modules.
   *
   * @see Module::Process.
   */
  bool ProvideData(const Module* module, std::shared_ptr<CNFrameInfo> data);
  /**
   * @brief Gets the event bus in the pipeline.
   *
   * @return Returns the event bus.
   */
  EventBus* GetEventBus() const;
  /**
   * @brief Binds the stream message observer with a pipeline to receive stream message from this pipeline.
   *
   * @param[in] observer The stream message observer.
   *
   * @return No return value.
   *
   * @see StreamMsgObserver.
   */
  void SetStreamMsgObserver(StreamMsgObserver* observer);
  /**
   * @brief Gets the stream message observer that has been bound with this pipeline.
   *
   * @return Returns the stream message observer that has been bound with this pipeline.
   *
   * @see Pipeline::SetStreamMsgObserver.
   */
  StreamMsgObserver* GetStreamMsgObserver() const;
  /**
   * @brief Gets this pipeline's profiler.
   *
   * @return Returns profiler.
   */
  PipelineProfiler* GetProfiler() const;
  /**
   * @brief Gets this pipeline's tracer.
   *
   * @return Returns tracer.
   */
  PipelineTracer* GetTracer() const;
  /**
   * @brief Checks if module is root node of pipeline or not.
   * The module name can be specified by two ways, see Pipeline::GetModule for detail.
   *
   * @param[in] module_name module name.
   *
   * @return Returns true if it's root node, otherwise returns false.
   **/
  bool IsRootNode(const std::string& module_name) const;
  /**
   * @brief Checks if module is leaf node of pipeline.
   * The module name can be specified by two ways, see Pipeline::GetModule for detail.
   *
   * @param[in] module_name module name.
   *
   * @return Returns true if it's leaf node, otherwise returns false.
   **/
  bool IsLeafNode(const std::string& module_name) const;

  /**
   * @brief Registers a callback to be called after the frame process is done.
   *        The callback will be invalid when Pipeline::Stop is called.
   *
   * @param[in] callback The call back function.
   *
   * @return No return value.
   *
   */
  void RegisterFrameDoneCallBack(const std::function<void(std::shared_ptr<CNFrameInfo>)>& callback);

 private:
  /** called by BuildPipeline **/
  bool CreateModules();
  void GenerateModulesMask();
  bool CreateConnectors();

  /* ------Internal methods------ */
  bool PassedByAllModules(uint64_t mask) const;
  void OnProcessStart(NodeContext* context, const std::shared_ptr<CNFrameInfo>& data);
  void OnProcessEnd(NodeContext* context, const std::shared_ptr<CNFrameInfo>& data);
  void OnProcessFailed(NodeContext* context, const std::shared_ptr<CNFrameInfo>& data, int ret);
  void OnDataInvalid(NodeContext* context, const std::shared_ptr<CNFrameInfo>& data);
  void OnEos(NodeContext* context, const std::shared_ptr<CNFrameInfo>& data);
  void OnPassThrough(const std::shared_ptr<CNFrameInfo>& data);

  void TransmitData(NodeContext* context, const std::shared_ptr<CNFrameInfo>& data);
  void TaskLoop(NodeContext* context, uint32_t conveyor_idx);
  EventHandleFlag DefaultBusWatch(const Event& event);
  void UpdateByStreamMsg(const StreamMsg& msg);
  void StreamMsgHandleFunc();
  std::vector<std::string> GetSortedModuleNames();

  std::unique_ptr<CNGraph<NodeContext>> graph_;
  std::vector<std::string> sorted_module_names_;

  std::string name_;
  std::atomic<bool> running_{false};
  std::unique_ptr<EventBus> event_bus_ = nullptr;

  std::unique_ptr<IdxManager> idxManager_ = nullptr;
  std::vector<std::thread> threads_;

  // message observer members
  ThreadSafeQueue<StreamMsg> msgq_;
  std::thread smsg_thread_;
  StreamMsgObserver* smsg_observer_ = nullptr;
  std::atomic<bool> exit_msg_loop_{false};

  uint64_t all_modules_mask_ = 0;
  std::unique_ptr<PipelineProfiler> profiler_;

  std::function<void(std::shared_ptr<CNFrameInfo>)> frame_done_cb_ = NULL;

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
};  // class Pipeline

inline const std::string& Pipeline::GetName() const {
  return name_;
}

inline bool Pipeline::BuildPipeline(const std::vector<CNModuleConfig>& module_configs,
    const ProfilerConfig& profiler_config) {
  CNGraphConfig graph_config;
  graph_config.name = GetName();
  graph_config.module_configs = module_configs;
  graph_config.profiler_config = profiler_config;
  return BuildPipeline(graph_config);
}

inline bool Pipeline::BuildPipelineByJSONFile(const std::string& config_file) {
  CNGraphConfig graph_config;
  if (!graph_config.ParseByJSONFile(config_file)) {
    LOGE(CORE) << "Parse graph config file failed.";
    return false;
  }
  return BuildPipeline(graph_config);
}

inline bool Pipeline::IsRunning() const {
  return running_;
}

inline EventBus* Pipeline::GetEventBus() const {
  return event_bus_.get();
}

inline void Pipeline::SetStreamMsgObserver(StreamMsgObserver* observer) {
  smsg_observer_ = observer;
}

inline StreamMsgObserver* Pipeline::GetStreamMsgObserver() const {
  return smsg_observer_;
}

inline bool Pipeline::IsProfilingEnabled() const {
  return profiler_ ? profiler_->GetConfig().enable_profiling : false;
}

inline bool Pipeline::IsTracingEnabled() const {
  return profiler_ ? profiler_->GetConfig().enable_tracing : false;
}

inline PipelineProfiler* Pipeline::GetProfiler() const {
  return IsProfilingEnabled() ? profiler_.get() : nullptr;
}

inline PipelineTracer* Pipeline::GetTracer() const {
  return IsTracingEnabled() ? profiler_->GetTracer() : nullptr;
}

inline bool Pipeline::PassedByAllModules(uint64_t mask) const {
  return mask == all_modules_mask_;
}

inline void Pipeline::RegisterFrameDoneCallBack(const std::function<void(std::shared_ptr<CNFrameInfo>)>& callback) {
  frame_done_cb_ = callback;
}

}  // namespace cnstream

#endif  // CNSTREAM_PIPELINE_HPP_
