/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * This source code is licensed under the Apache-2.0 license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * A part of this source code is referenced from Nebula project.
 * https://github.com/Bwar/Nebula/blob/master/src/actor/DynamicCreator.hpp
 * https://github.com/Bwar/Nebula/blob/master/src/actor/ActorFactory.hpp
 *
 * Copyright (C) Bwar.
 *
 * This source code is licensed under the Apache-2.0 license found in the
 * LICENSE file in the root directory of this source tree.
 *
 *************************************************************************/

#ifndef CNSTREAM_MODULE_HPP_
#define CNSTREAM_MODULE_HPP_

/**
 * @file cnstream_module.hpp
 *
 * This file contains a declaration of the Module class and the ModuleFactory class.
 */
#include <cxxabi.h>
#include <unistd.h>

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <typeinfo>
#include <map>
#include <utility>
#include <vector>

#include "cnstream_common.hpp"
#include "cnstream_config.hpp"
#include "cnstream_eventbus.hpp"
#include "cnstream_frame.hpp"
#include "cnstream_logging.hpp"
#include "private/cnstream_module_pri.hpp"
#include "util/cnstream_queue.hpp"
#include "util/cnstream_rwlock.hpp"
#include "profiler/module_profiler.hpp"

namespace cnstream {

class Pipeline;
struct NodeContext;

/**
 * @class IModuleObserver
 *
 * @brief IModuleObserver is an interface class. Users need to implement an observer
 *        based on this, and register it to one module.
 */
class IModuleObserver {
 public:
  /**
   * @brief Notifies "data" after being processed by this module.
   *
   * @param[in] data The frame that is notified to observer.
   *
   * @return No return value.
   */
  virtual void notify(std::shared_ptr<CNFrameInfo> data) = 0;
  /**
   * @brief Default destructor. A destructor to destruct module observer.
   *
   * @return No return value.
   */
  virtual ~IModuleObserver() = default;
};

/**
 * @class Module.
 *
 * @brief Module is the parent class of all modules. A module could have configurable
 * number of upstream links and downstream links.
 * Some modules are already constructed with a framework,
 * such as source, inferencer, and so on. You can also design your own modules.
 */
class Module : private NonCopyable {
 public:
  /**
   * @brief Constructor. A constructor to construct module object.
   *
   * @param[in] name The name of a module. Modules defined in a pipeline must have different names.
   *
   * @return No return value.
   */
  explicit Module(const std::string &name) : name_(name) {}
  /**
   * @brief Destructor. A destructor to destruct module instance.
   *
   * @return No return value.
   */
  virtual ~Module();
  /**
   * @brief Registers an observer to the module.
   *
   * @param[in] observer An observer you defined.
   *
   * @return No return value.
   */
  void SetObserver(IModuleObserver *observer) {
    RwLockWriteGuard guard(observer_lock_);
    observer_ = observer;
  }
  /**
   * @brief Opens resources for a module.
   *
   * @param[in] param_set A set of parameters for this module.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   *
   * @note You do not need to call this function by yourself. This function is called
   *       by pipeline automatically when the pipeline is started. The pipeline calls the ``Process`` function
   *       of this module automatically after the ``Open`` function is done.
   */
  virtual bool Open(ModuleParamSet param_set) = 0;

  /**
   * @brief Closes resources for a module.
   *
   * @return No return value.
   *
   * @note You do not need to call this function by yourself. This function is called
   *       by pipeline automatically when the pipeline is stopped. The pipeline calls the ``Close`` function
   *       of this module automatically after the ``Open`` and ``Process`` functions are done.
   */
  virtual void Close() = 0;

  /**
   * @brief Processes data.
   *
   * @param[in] data The data to be processed by the module.
   *
   * @retval >=0: The data is processed successfully.
   * @retval <0: Pipeline will post an event with the EVENT_ERROR event type and the return number.
   */
  virtual int Process(std::shared_ptr<CNFrameInfo> data) = 0;

  /**
   * @brief Notifies flow-EOS arriving, the module should reset internal status if needed.
   *
   * @param[in] stream_id The stream identification.
   *
   * @note This function will be invoked when flow-EOS is forwarded by the framework.
   */
  virtual void OnEos(const std::string &stream_id) {}

  /**
   * @brief Gets the name of this module.
   *
   * @return Returns the name of this module.
   */
  inline std::string GetName() const { return name_; }

  /**
   * @brief Posts an event to the pipeline.
   *
   * @param[in] type The type of an event.
   * @param[in] msg The event message string.
   *
   * @return Returns true if this function has run successfully. Returns false if this
   *         module has not been added to the pipeline.
   */
  bool PostEvent(EventType type, const std::string &msg);

  /**
   * @brief Posts an event to the pipeline.
   *
   * @param[in] Event with event type, stream_id, message, module name and thread_id.
   *
   * @return Returns true if this function has run successfully. Returns false if this
   *         module has not been added to the pipeline.
   */
  bool PostEvent(Event e);

  /**
   * @brief Transmits data to the following stages.
   *
   * Valid when the module has permission to transmit data by itself.
   *
   * @param[in] data A pointer to the information of the frame.
   *
   * @return Returns true if the data has been transmitted successfully. Otherwise, returns false.
   */
  bool TransmitData(std::shared_ptr<CNFrameInfo> data);

  /**
   * @brief Checks parameters for a module, including parameter name, type, value, validity, and so on.
   *
   * @param[in] paramSet Parameters for this module.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   */
  virtual bool CheckParamSet(const ModuleParamSet &paramSet) const { return true; }

  /**
   * @brief Gets the pipeline this module belongs to.
   *
   * @return Returns the pointer to pipeline instance.
   */
  Pipeline* GetContainer() const { return container_; }

  /**
   * @brief Gets module profiler.
   *
   * @return Returns a pointer to the module's profiler.
   */
  ModuleProfiler* GetProfiler();

  /**
   * @brief Checks if this module has permission to transmit data by itself.
   *
   * @return Returns true if this module has permission to transmit data by itself. Otherwise, returns false.
   *
   * @see Process
   */

  bool HasTransmit() const { return hasTransmit_.load(); }

  /**
   * Each module registers its own parameters and descriptions.
   * CNStream Inspect tool uses this class to detect parameters of each module.
   */
  ParamRegister param_register_;


#ifdef UNIT_TEST
 public:  // NOLINT
#else
 protected:  // NOLINT
#endif

  friend class Pipeline;
  friend class CNFrameInfo;
  /**
   * @brief Sets a container to this module and identifies which pipeline the module is added to.
   *
   * @param[in] container A pipeline pointer to the container of this module.
   *
   * @note This function is called automatically by the pipeline after this module
   *       is added into the pipeline. You do not need to call this function by yourself.
   */
  void SetContainer(Pipeline *container);

  /**
   * @brief Processes the data. This function is called by a pipeline.
   *
   * @param[in] data A pointer to the information of the frame.
   *
   * @retval 0: The process has been run successfully. The data should be transmitted by framework then.
   * @retval >0: The process has been run successfully. The data has been handled by this module. The ``hasTransmit_``
   * must be set. The Pipeline::ProvideData should be called by Module to transmit data to the next modules in the
   * pipeline.
   * @retval <0: Pipeline posts an event with the EVENT_ERROR event type and return number.
   */
  int DoProcess(std::shared_ptr<CNFrameInfo> data);

  Pipeline *container_ = nullptr;  ///< The container.
  RwLock container_lock_;

  std::string name_;                      ///< The name of the module.
  std::atomic<bool> hasTransmit_{false};  ///< Whether it has permission to transmit data.

#ifdef UNIT_TEST
 public:  // NOLINT
#else
 private:    // NOLINT
#endif

  IModuleObserver *observer_ = nullptr;
  RwLock observer_lock_;
  void NotifyObserver(std::shared_ptr<CNFrameInfo> data) {
    RwLockReadGuard guard(observer_lock_);
    if (observer_) {
      observer_->notify(data);
    }
  }
  int DoTransmitData(std::shared_ptr<CNFrameInfo> data);

  size_t GetId();
  size_t id_ = INVALID_MODULE_ID;
  NodeContext* context_ = nullptr;  // used by pipeline
};

/**
 * @class ModuleEx
 *
 * @brief ModuleEx is the base class of the modules who have permission to transmit processed data by themselves.
 */
class ModuleEx : public Module {
 public:
  /**
   * @brief Constructor. A constructor to construct the module which has permission to transmit processed data by
   *        itself.
   *
   * @param[in] name The name of a module. Modules defined in a pipeline must have different names.
   *
   * @return No return value.
   */
  explicit ModuleEx(const std::string &name) : Module(name) { hasTransmit_.store(true); }
};

}  // namespace cnstream

#endif  // CNSTREAM_MODULE_HPP_
